package com.yuminaga.motonav

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.View
import android.view.WindowInsets
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.SwitchCompat
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.math.cos

class RideHistoryActivity : AppCompatActivity() {
    private val bg = Color.rgb(14,10,20)
    private val card = Color.rgb(26,20,36)
    private val text = Color.rgb(243,238,248)
    private val sub = Color.rgb(157,147,171)
    private val accent = Color.rgb(67,225,240)
    private lateinit var items: LinearLayout
    private lateinit var status: TextView
    private val handler = Handler(Looper.getMainLooper())
    private var exportId: String? = null
    private var generation = 0
    private val exportFile = registerForActivityResult(ActivityResultContracts.CreateDocument("application/gpx+xml")) { uri ->
        val id = exportId; exportId=null
        if (uri != null && id != null) {
            toast("正在导出 GPX…")
            RideRecorder.export(id,uri) { error -> if (!isDestroyed) toast(if (error==null) "GPX 已导出" else "导出失败：$error") }
        }
    }
    private val refresh = object : Runnable {
        override fun run() { status.text=RideRecorder.status(); handler.postDelayed(this,1000) }
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        exportId=savedInstanceState?.getString("export_id")
        val content = LinearLayout(this).apply { orientation=LinearLayout.VERTICAL; setPadding(dp(18),dp(12),dp(18),dp(24)) }
        val scroll = ScrollView(this).apply { setBackgroundColor(bg); addView(content); clipToPadding=true }
        scroll.setOnApplyWindowInsetsListener { v, ins ->
            @Suppress("DEPRECATION")
            val top=if(Build.VERSION.SDK_INT>=30) ins.getInsets(WindowInsets.Type.systemBars()).top else ins.systemWindowInsetTop
            @Suppress("DEPRECATION")
            val bottom=if(Build.VERSION.SDK_INT>=30) ins.getInsets(WindowInsets.Type.systemBars()).bottom else ins.systemWindowInsetBottom
            v.setPadding(0,top,0,bottom); ins
        }
        content.addView(button("‹ 返回首页") { finish() })
        content.addView(label("骑行轨迹",25f,text).apply { typeface=Typeface.DEFAULT_BOLD })
        val toggle = SwitchCompat(this).apply {
            this.text="自动记录真实导航和巡航"; setTextColor(this@RideHistoryActivity.text); textSize=16f
            isChecked=RideRecorder.enabled
            setOnCheckedChangeListener { _, value -> RideRecorder.setEnabled(value); status.text=RideRecorder.status() }
        }
        content.addView(toggle)
        status=label(RideRecorder.status(),13f,accent); content.addView(status)
        content.addView(label("轨迹保存在本机，不主动上传。模拟导航不记录。卸载会清除记录，重要轨迹请导出 GPX 保存。",13f,sub))
        content.addView(button("刷新记录") { load() })
        items=LinearLayout(this).apply { orientation=LinearLayout.VERTICAL }; content.addView(items)
        setContentView(scroll)
    }
    override fun onResume() { super.onResume(); handler.post(refresh); load() }
    override fun onPause() { handler.removeCallbacks(refresh); super.onPause() }
    override fun onDestroy() { generation++; handler.removeCallbacksAndMessages(null); super.onDestroy() }
    override fun onSaveInstanceState(outState: Bundle) { outState.putString("export_id",exportId); super.onSaveInstanceState(outState) }

    private fun load() {
        val token=++generation
        RideRecorder.history { all,error ->
            if (isDestroyed || token!=generation) return@history
            items.removeAllViews()
            if (error!=null) { items.addView(label("读取失败：$error",14f,sub)); return@history }
            if (all.isEmpty()) { items.addView(label("还没有轨迹。开始真实导航或巡航，收到有效定位后会自动记录。",16f,sub)); return@history }
            items.addView(label("共 ${all.size} 条记录${if(all.size>100) "，显示最近 100 条" else ""}",13f,sub))
            for (s in all.take(100)) {
                val box=LinearLayout(this).apply {
                    orientation=LinearLayout.VERTICAL; setPadding(dp(14),dp(12),dp(14),dp(12))
                    background=GradientDrawable().apply { setColor(card); cornerRadius=dp(16).toFloat() }
                }
                box.addView(label(s.title,17f,text).apply { typeface=Typeface.DEFAULT_BOLD; maxLines=2 })
                val date=SimpleDateFormat("yyyy-MM-dd HH:mm",Locale.CHINA).format(Date(s.start))
                box.addView(label("$date · ${s.mode}${if(s.active) " · 记录中" else if(s.interrupted) " · 中断后已恢复" else ""}",12f,sub))
                box.addView(label("${RideRecorder.distanceText(s.meters)} · ${RideRecorder.durationText(s.end-s.start)}",16f,accent))
                box.addView(label("最高 ${s.maxKmh.toInt()} km/h · ${s.points} 个点 · 点此查看",12f,sub))
                box.setOnClickListener { detail(s) }
                items.addView(box,LinearLayout.LayoutParams(-1,-2).apply { topMargin=dp(12) })
            }
        }
    }
    private fun detail(s: RideSummary) {
        val box=LinearLayout(this).apply { orientation=LinearLayout.VERTICAL; setPadding(dp(12),0,dp(12),0) }
        box.addView(TextView(this).apply { this.text="轨迹示意（无底图）\n粉色为轨迹，绿色为起点，橙色为终点。定位中断处不连线。"; textSize=12f; setPadding(0,dp(8),0,dp(8)) })
        val preview=RidePreview(this)
        box.addView(preview,LinearLayout.LayoutParams(-1,dp(260)))
        val dialog=AlertDialog.Builder(this).setTitle(s.title).setView(box)
            .setPositiveButton("导出 GPX") { _,_ ->
                exportId=s.id
                val date=SimpleDateFormat("yyyyMMdd-HHmmss",Locale.US).format(Date(s.start))
                exportFile.launch("MotoNav-$date.gpx")
            }.setNegativeButton("关闭",null)
            .setNeutralButton("删除") { _,_ ->
                AlertDialog.Builder(this).setTitle("删除这条轨迹？").setMessage("删除后无法恢复。")
                    .setPositiveButton("删除") { _,_ -> RideRecorder.delete(s.id) { err -> if(!isDestroyed) { if(err!=null) toast(err); load() } } }
                    .setNegativeButton("取消",null).show()
            }.show()
        RideRecorder.preview(s.id) { points,error ->
            if (!isDestroyed && dialog.isShowing) { preview.points=points; preview.invalidate(); if(error!=null) toast("轨迹读取失败：$error") }
        }
    }
    private fun dp(n: Int)=(n*resources.displayMetrics.density+0.5f).toInt()
    private fun label(s: String,size: Float,color: Int)=TextView(this).apply { text=s; textSize=size; setTextColor(color); setPadding(0,dp(7),0,dp(7)) }
    private fun button(s: String,click: () -> Unit)=label(s,15f,accent).apply { minHeight=dp(48); gravity=Gravity.CENTER_VERTICAL; setOnClickListener { click() } }
    private fun toast(s: String)=Toast.makeText(this,s,Toast.LENGTH_LONG).show()
}

private class RidePreview(c: Context): View(c) {
    var points: List<RidePoint> = emptyList()
    private val paint=Paint(Paint.ANTI_ALIAS_FLAG)
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas); canvas.drawColor(Color.rgb(14,10,20))
        if (points.isEmpty()) { paint.color=Color.LTGRAY; paint.textSize=14*resources.displayMetrics.scaledDensity; canvas.drawText("等待轨迹数据…",20f,height/2f,paint); return }
        val lat0=points.map { it.lat }.average()
        val scaleX=cos(Math.toRadians(lat0)).coerceAtLeast(0.01)
        val minX=points.minOf { it.lng*scaleX }; val maxX=points.maxOf { it.lng*scaleX }
        val minY=points.minOf { it.lat }; val maxY=points.maxOf { it.lat }
        val pad=24*resources.displayMetrics.density
        val scale=minOf((width-2*pad)/(maxX-minX).coerceAtLeast(0.0001),(height-2*pad)/(maxY-minY).coerceAtLeast(0.0001)).coerceAtLeast(0.0)
        fun x(p: RidePoint)=(width/2.0+(p.lng*scaleX-(minX+maxX)/2)*scale).toFloat()
        fun y(p: RidePoint)=(height/2.0-(p.lat-(minY+maxY)/2)*scale).toFloat()
        val path=Path()
        points.forEachIndexed { i,p -> if(i==0 || p.segment) path.moveTo(x(p),y(p)) else path.lineTo(x(p),y(p)) }
        paint.style=Paint.Style.STROKE; paint.strokeWidth=3*resources.displayMetrics.density; paint.color=Color.rgb(255,79,180)
        canvas.drawPath(path,paint)
        paint.style=Paint.Style.FILL; paint.color=Color.rgb(61,220,132); canvas.drawCircle(x(points.first()),y(points.first()),6*resources.displayMetrics.density,paint)
        paint.color=Color.rgb(255,181,71); canvas.drawCircle(x(points.last()),y(points.last()),4*resources.displayMetrics.density,paint)
    }
}
