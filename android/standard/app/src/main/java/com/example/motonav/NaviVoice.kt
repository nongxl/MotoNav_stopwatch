package com.example.motonav

import android.content.ComponentName
import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.media.session.MediaController
import android.media.session.MediaSessionManager
import android.media.session.PlaybackState
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.speech.tts.TextToSpeech
import android.speech.tts.UtteranceProgressListener
import android.speech.tts.Voice
import com.amap.api.navi.AMapNavi
import java.util.Locale

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（v3.8 新增；v4.1 撤掉内置 AI 语音；v6.6 暂停模式直接控制音乐 App；v7.0 详细 / 简洁播报）
 *
 * 导航语音：
 * - 高德自带：和以前一样，由高德 SDK 自己播报
 * - 手机系统语音：让高德只把播报文字交给我们（不自己出声），我们用手机的语音引擎（TTS）念出来。
 *   可以选语音引擎（手机上装的，比如系统自带、讯飞等）、选声音、调语速和音调。
 * - v4.1 内置 AI 语音（v4.0）撤掉了（App 太大）；改为推荐免费的 AI 语音引擎，装好后在这里选
 * 播报声音走“导航提示”通道，连着蓝牙耳机/头盔时从耳机里出声。
 * v5.1 播报时压低音乐（像车机一样）：播报前向系统申请“临时音频焦点”，音乐 App 收到后自动压低
 *   （或暂停），播完再还回去，音乐恢复。
 *   - 手机系统语音：念之前申请，念完（引擎回调“说完了”）0.4 秒后归还
 *   - 高德自带：高德自己出声我们管不到，所以同时让高德把播报文字也给我们一份，
 *     收到文字就申请，按字数估算念多久（每字约 0.24 秒）后归还
 * v6.6 有的音乐 App（小米手机上常见）系统同意了“压低”，音乐却照样大声：它们用的播放方式系统压不了，
 *   而 App 自己又不理会。所以“暂停”模式加了一招：打开「通知使用权」后，播报时直接对正在放的音乐按“暂停”，
 *   播完按“播放”（只恢复我们暂停的那几个，不会把本来没在放的音乐打开）
 */
object NaviVoice {
    const val MODE_AMAP = 0
    const val MODE_SYSTEM = 1
    private const val PREFS = "motonav"

    @Volatile var mode = MODE_AMAP
        private set
    @Volatile var engine = ""            // 语音引擎包名，空 = 手机默认
        private set
    @Volatile var voiceName = ""         // 声音名字，空 = 引擎默认
        private set
    @Volatile var rate = 1.0f            // 语速 0.5~2.0
        private set
    @Volatile var pitch = 1.0f           // 音调 0.5~2.0
        private set
    @Volatile var status = "（还没初始化）"
    @Volatile var muted = false

    // v5.1 播报时怎么处理音乐
    const val DUCK_OFF = 0       // 不管
    const val DUCK_LOWER = 1     // 压低音量（默认）
    const val DUCK_PAUSE = 2     // 暂停，播完继续
    @Volatile var duck = DUCK_LOWER
        private set
    @Volatile var duckInfo = "（还没播报过）"

    // v7.0 播报详细程度：详细（新手，路口前多次提醒、报路名）/ 简洁（老手，只在关键时刻提醒）
    const val BC_DETAIL = 0
    const val BC_CONCISE = 1
    @Volatile var broadcast = BC_DETAIL
        private set
    @Volatile var broadcastInfo = ""
    private var lastNavi: java.lang.ref.WeakReference<AMapNavi>? = null
    private val main = Handler(Looper.getMainLooper())
    private var am: AudioManager? = null
    private var focusReq: AudioFocusRequest? = null
    private var focusHeld = false
    private var busy = 0                 // 系统语音还没念完的句数
    private var releaseAt = 0L
    private var duckCount = 0
    private var holdStart = 0L
    private val releaseRun = Runnable { tryRelease() }
    private val pausedByUs = ArrayList<MediaController>()       // v6.6 我们直接按了暂停的音乐 App
    private val focusListener = AudioManager.OnAudioFocusChangeListener { ch ->
        if (ch == AudioManager.AUDIOFOCUS_LOSS) focusHeld = false              // 被电话等抢走了
    }

    private var ctx: Context? = null
    private var tts: TextToSpeech? = null
    private var ready = false
    private var pending: String? = null
    private var lastText = ""
    private var lastAt = 0L
    private var onReady: (() -> Unit)? = null

    fun load(context: Context) {
        if (ctx != null) return
        ctx = context.applicationContext
        val sp = ctx!!.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        mode = sp.getInt("voice_mode", MODE_AMAP)
        engine = sp.getString("voice_engine", "") ?: ""
        voiceName = sp.getString("voice_name", "") ?: ""
        rate = sp.getFloat("voice_rate", 1.0f)
        pitch = sp.getFloat("voice_pitch", 1.0f)
        duck = sp.getInt("voice_duck", DUCK_LOWER)
        broadcast = sp.getInt("voice_broadcast", BC_DETAIL)
        am = ctx!!.getSystemService(Context.AUDIO_SERVICE) as? AudioManager
        if (mode != MODE_AMAP && mode != MODE_SYSTEM) mode = MODE_SYSTEM     // v4.1 以前选过“AI 语音”的改回系统语音
        if (mode == MODE_SYSTEM) initTts()
    }

    private fun save() {
        ctx?.getSharedPreferences(PREFS, Context.MODE_PRIVATE)?.edit()
            ?.putInt("voice_mode", mode)?.putString("voice_engine", engine)?.putString("voice_name", voiceName)
            ?.putFloat("voice_rate", rate)?.putFloat("voice_pitch", pitch)?.putInt("voice_duck", duck)
            ?.putInt("voice_broadcast", broadcast)?.apply()
    }

    /** 准备语音引擎（换引擎时重新创建）；done = 准备好以后要做的事（比如刷新声音列表） */
    fun initTts(done: (() -> Unit)? = null) {
        val c = ctx ?: return
        try { tts?.shutdown() } catch (_: Exception) {}
        ready = false
        onReady = done
        status = "语音引擎准备中…"
        val listener = TextToSpeech.OnInitListener { r ->
            if (r == TextToSpeech.SUCCESS) {
                ready = true
                applyParams()
                tts?.setOnUtteranceProgressListener(object : UtteranceProgressListener() {   // v5.1 念完归还焦点
                    override fun onStart(id: String?) {}
                    override fun onDone(id: String?) = utterEnd()
                    @Deprecated("") override fun onError(id: String?) = utterEnd()
                    override fun onError(id: String?, code: Int) = utterEnd()
                    override fun onStop(id: String?, interrupted: Boolean) = utterEnd()
                })
                status = "语音引擎：${engineLabel()}"
                pending?.let { speakNow(it) }
                pending = null
            } else {
                status = "语音引擎启动失败（$r），请换一个引擎或用高德自带语音"
            }
            onReady?.invoke(); onReady = null
        }
        tts = if (engine.isNotEmpty()) TextToSpeech(c, listener, engine) else TextToSpeech(c, listener)
    }

    private fun applyParams() {
        val t = tts ?: return
        try {
            t.setAudioAttributes(AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_ASSISTANCE_NAVIGATION_GUIDANCE)
                .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH).build())
        } catch (_: Exception) {}
        val v = if (voiceName.isNotEmpty()) try { t.voices?.firstOrNull { it.name == voiceName } } catch (_: Exception) { null } else null
        if (v != null) t.setVoice(v) else t.setLanguage(Locale.SIMPLIFIED_CHINESE)
        t.setSpeechRate(rate)
        t.setPitch(pitch)
    }

    // ---------- 设置 ----------

    fun setMode(m: Int) {
        mode = m; save()
        if (m == MODE_SYSTEM && tts == null) initTts()
    }
    fun setEngine(pkg: String, done: (() -> Unit)? = null) { engine = pkg; voiceName = ""; save(); initTts(done) }
    fun setVoice(name: String) { voiceName = name; save(); applyParams() }
    fun setRate(r: Float) { rate = r.coerceIn(0.5f, 2.0f); save(); tts?.setSpeechRate(rate) }
    fun setPitch(p: Float) { pitch = p.coerceIn(0.5f, 2.0f); save(); tts?.setPitch(pitch) }
    /** v5.1 播报时音乐：不管 / 压低 / 暂停（高德自带语音下，下次开始导航生效） */
    fun setDuck(d: Int) { duck = d; save(); if (d == DUCK_OFF) main.post { releaseAt = 0; abandon() } }

    /** 手机上装的语音引擎：(包名, 名字) */
    fun engines(): List<Pair<String, String>> = try { tts?.engines?.map { it.name to it.label } ?: emptyList() } catch (_: Exception) { emptyList() }

    /** v4.1 某个语音引擎装了没有（包名里含 key 就算） */
    fun hasEngine(key: String): Boolean = engines().any { it.first.contains(key, ignoreCase = true) }

    fun engineLabel(): String {
        val e = engines()
        val cur = if (engine.isNotEmpty()) engine else (try { tts?.defaultEngine } catch (_: Exception) { null } ?: "")
        return e.firstOrNull { it.first == cur }?.second ?: if (cur.isEmpty()) "手机默认" else cur
    }

    /** 当前引擎里的中文声音 */
    fun zhVoices(): List<Voice> = try {
        (tts?.voices ?: emptySet()).filter { it.locale.language == "zh" || it.locale.language == "cmn" || it.locale.language == "yue" }
            .sortedWith(compareBy({ it.isNetworkConnectionRequired }, { it.name }))
    } catch (_: Exception) { emptyList() }

    fun voiceLabel(v: Voice): String {
        val region = when (v.locale.country) { "TW" -> "台湾"; "HK" -> "香港"; "CN", "" -> "普通话"; else -> v.locale.country }
        val lang = if (v.locale.language == "yue") "粤语" else region
        val net = if (v.isNetworkConnectionRequired) "需联网" else "本地"
        return "$lang · ${v.name}（$net）"
    }

    fun currentVoiceLabel(): String {
        if (voiceName.isEmpty()) return "引擎默认"
        return zhVoices().firstOrNull { it.name == voiceName }?.let { voiceLabel(it) } ?: voiceName
    }

    // ---------- 播报 ----------

    /** 按当前设置告诉高德：自己出声，还是把文字交给我们 */
    fun applyTo(navi: AMapNavi?) {
        if (navi == null) return
        lastNavi = java.lang.ref.WeakReference(navi)
        val inner = !muted && mode == MODE_AMAP
        val callback = !muted && (mode == MODE_SYSTEM || duck != DUCK_OFF)     // v5.1 高德自带也要文字，用来压低音乐
        if (!Refl.call(navi, "setUseInnerVoice", inner, callback).first) Refl.call(navi, "setUseInnerVoice", inner)
        applyBroadcast(navi)                                                   // v7.0 详细 / 简洁
    }

    /** v7.0 改播报详细程度：存起来，导航中也马上告诉高德 */
    fun setBroadcast(b: Int) {
        broadcast = b; save()
        applyBroadcast(lastNavi?.get())
    }

    /** 告诉高德用详细还是简洁播报（高德的值从 SDK 里读，读不到就用 简洁 1 / 详细 2） */
    private fun applyBroadcast(navi: AMapNavi?) {
        if (navi == null) return
        val name = if (broadcast == BC_CONCISE) "CONCISE" else "DETAIL"
        val v = try {
            Class.forName("com.amap.api.navi.enums.BroadcastMode").getField(name).getInt(null)
        } catch (_: Throwable) { if (broadcast == BC_CONCISE) 1 else 2 }
        val ok = Refl.call(navi, "setBroadcastMode", v).first
        broadcastInfo = if (ok) "已设为${if (broadcast == BC_CONCISE) "简洁" else "详细"}播报" else "这个高德 SDK 不支持切换播报详细程度"
    }

    /**
     * 高德给了一段播报文字。
     * - 手机系统语音：我们来念
     * - 高德自带（或高德原版界面 byAmap=true）：高德自己在念，我们只按字数压低音乐
     */
    fun onNaviText(text: String?, byAmap: Boolean = false) {
        if (text.isNullOrBlank() || muted) return
        val now = SystemClock.elapsedRealtime()
        if (text == lastText && now - lastAt < 1500) return       // 新旧两种回调可能各来一次
        lastText = text; lastAt = now
        if (mode == MODE_AMAP || byAmap) {
            if (duck != DUCK_OFF) main.post { hold(estimateMs(text), "高德") }
            return
        }
        speak(text)
    }

    /** 估算念完要多久：中文每字约 0.24 秒，标点停顿算一点，再加 0.6 秒余量 */
    private fun estimateMs(text: String): Long {
        val chars = text.count { !it.isWhitespace() }
        return 600L + chars * 240L
    }

    fun speak(text: String) {
        if (tts == null) initTts()
        if (!ready) { pending = text; return }
        speakNow(text)
    }

    private fun speakNow(text: String) {
        if (duck != DUCK_OFF) main.post {
            busy++
            hold((estimateMs(text) / rate).toLong() * 2 + 3000, "系统语音")   // 保险：万一引擎不回调“说完了”，最多压这么久
        }
        try {
            val r = tts?.speak(text, TextToSpeech.QUEUE_ADD, Bundle(), "motonav-${SystemClock.elapsedRealtime()}")
            if (r != TextToSpeech.SUCCESS) utterEnd()
        } catch (_: Exception) { utterEnd() }
    }

    // ---------- v5.1 播报时压低音乐 ----------

    /** 系统语音一句念完 */
    private fun utterEnd() {
        main.post {
            if (busy > 0) busy--
            if (busy == 0 && focusHeld) {                         // 全念完了：0.4 秒后归还（连着几句中间不让音乐忽大忽小）
                releaseAt = SystemClock.elapsedRealtime() + 400
                main.removeCallbacks(releaseRun); main.postDelayed(releaseRun, 400)
            }
        }
    }

    /** 申请焦点（压低/暂停音乐），至少保持 ms 毫秒；只在主线程调用 */
    private fun hold(ms: Long, who: String) {
        val now = SystemClock.elapsedRealtime()
        if (!focusHeld) {
            if (!request()) { duckInfo = "系统没给音频焦点（$who），这次没压低"; return }
            focusHeld = true
            holdStart = now
            if (duck == DUCK_PAUSE) main.postDelayed({ if (focusHeld) pauseOthers() }, 200)   // v6.6 不听话的音乐 App：直接按暂停
        }
        duckCount++
        duckInfo = "已${if (duck == DUCK_PAUSE) "暂停" else "压低"}音乐 $duckCount 次（最近一次：$who，约 ${ms / 1000.0} 秒）" + (if (duck == DUCK_PAUSE) pauseNote else "")
        val until = now + ms
        if (until > releaseAt) {
            releaseAt = until
            main.removeCallbacks(releaseRun); main.postDelayed(releaseRun, ms)
        }
    }

    private fun tryRelease() {
        val left = releaseAt - SystemClock.elapsedRealtime()
        if (left > 50) { main.postDelayed(releaseRun, left); return }
        if (busy > 0 && duck != DUCK_OFF && SystemClock.elapsedRealtime() - holdStart < 60_000) {   // 系统语音还在念（最多等 1 分钟）
            main.postDelayed(releaseRun, 500); return
        }
        abandon()
    }

    private fun request(): Boolean {
        val a = am ?: return false
        val gain = if (duck == DUCK_PAUSE) AudioManager.AUDIOFOCUS_GAIN_TRANSIENT else AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK
        return try {
            if (Build.VERSION.SDK_INT >= 26) {
                val req = AudioFocusRequest.Builder(gain)
                    .setAudioAttributes(AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_ASSISTANCE_NAVIGATION_GUIDANCE)
                        .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH).build())
                    .setOnAudioFocusChangeListener(focusListener, main)
                    .build()
                focusReq = req
                a.requestAudioFocus(req) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED
            } else {
                @Suppress("DEPRECATION")
                a.requestAudioFocus(focusListener, AudioManager.STREAM_MUSIC, gain) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED
            }
        } catch (_: Exception) { false }
    }

    private fun abandon() {
        main.removeCallbacks(releaseRun)
        if (!focusHeld) return
        focusHeld = false
        busy = 0
        if (pausedByUs.isNotEmpty()) main.postDelayed({ resumeOthers() }, 300)     // v6.6 播完：把我们暂停的音乐接着放
        val a = am ?: return
        try {
            if (Build.VERSION.SDK_INT >= 26) focusReq?.let { a.abandonAudioFocusRequest(it) }
            else @Suppress("DEPRECATION") a.abandonAudioFocus(focusListener)
        } catch (_: Exception) {}
    }

    // ---------- v6.6 直接让音乐 App 暂停 / 继续（需要「通知使用权」） ----------

    @Volatile private var pauseNote = ""

    /** 有没有「通知使用权」（有了才能看到、控制别的 App 正在放的音乐） */
    fun canControlMedia(): Boolean = ctx?.let { AmapProbe.notifEnabled(it) } ?: false

    private fun sessions(): List<MediaController> {
        val c = ctx ?: return emptyList()
        if (!AmapProbe.notifEnabled(c)) return emptyList()
        return try {
            val msm = c.getSystemService(Context.MEDIA_SESSION_SERVICE) as MediaSessionManager
            msm.getActiveSessions(ComponentName(c, AmapNotifyListener::class.java))
        } catch (_: Exception) { emptyList() }
    }

    private fun appName(pkg: String): String = try {
        val pm = ctx!!.packageManager
        pm.getApplicationLabel(pm.getApplicationInfo(pkg, 0)).toString()
    } catch (_: Exception) { pkg }

    /** 系统请求发出去了，还在放的音乐（不理会请求的 App）：直接按暂停 */
    private fun pauseOthers() {
        val c = ctx ?: return
        if (!AmapProbe.notifEnabled(c)) { pauseNote = "\n（没开「通知使用权」，只能请音乐 App 自己暂停）"; return }
        val names = ArrayList<String>()
        for (mc in sessions()) {
            if (mc.packageName == c.packageName) continue
            if (mc.playbackState?.state != PlaybackState.STATE_PLAYING) continue
            try { mc.transportControls.pause(); pausedByUs += mc; names += appName(mc.packageName) } catch (_: Exception) {}
        }
        pauseNote = if (names.isEmpty()) "\n（音乐 App 自己暂停了）" else "\n（直接按了暂停：${names.joinToString("、")}）"
        duckInfo = duckInfo.substringBefore("\n") + pauseNote
    }

    /** 播报完：只把我们暂停的那几个接着放 */
    private fun resumeOthers() {
        if (focusHeld) return                                   // 又开始播报了：等这次播完再说
        for (mc in pausedByUs) {
            try { if (mc.playbackState?.state != PlaybackState.STATE_PLAYING) mc.transportControls.play() } catch (_: Exception) {}
        }
        pausedByUs.clear()
    }

    private const val TEST_TEXT = "前方三百米右转，进入人民南路。注意，前方有测速，限速六十。"
    fun test() = speak(TEST_TEXT)
}
