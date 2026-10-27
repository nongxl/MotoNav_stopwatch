package com.example.motonav

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Typeface
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.roundToInt
import kotlin.math.sin

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（v6.3 新增；v7.4 只有按在圆盘上才转）
 *
 * 圆屏方向圆盘：按住圆盘转动（顺时针为正），粉色指针 = 画面上方在圆屏上朝的方向。
 * 转到 0° / 90° / 180° / 270° 附近（±4°）会吸住并轻震一下。
 * onChange(角度, 是否松手)：拖动时一直回调，松手时 final = true。
 */
@SuppressLint("ViewConstructor")
class RotDial(ctx: Context, private val pink: Int, private val cyan: Int, private val text: Int,
              private val sub: Int, private val stroke: Int) : View(ctx) {

    var angle = 0
        private set
    var onChange: ((Int, Boolean) -> Unit)? = null

    private val density = resources.displayMetrics.density
    private fun dp(v: Float) = v * density

    private val pFace = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.parseColor("#120E1A") }
    private val pRim = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; color = stroke; strokeWidth = dp(2f) }
    private val pTick = Paint(Paint.ANTI_ALIAS_FLAG).apply { strokeCap = Paint.Cap.ROUND }
    private val pFill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pHub = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.parseColor("#221C2C") }
    private val pBig = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = text; textAlign = Paint.Align.CENTER; typeface = Typeface.DEFAULT_BOLD
    }
    private val pSmall = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = sub; textAlign = Paint.Align.CENTER }
    private val path = Path()

    fun setAngle(d: Int) {
        angle = ((d % 360) + 360) % 360
        invalidate()
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val w = MeasureSpec.getSize(widthMeasureSpec)
        val s = min(w, dp(300f).toInt())
        setMeasuredDimension(w, s)
    }

    override fun onDraw(c: Canvas) {
        val cx = width / 2f
        val top = dp(16f)                                   // 上面留给青色小三角
        val r = min(width / 2f, (height - top) / 2f) - dp(4f)
        val cy = top + (height - top) / 2f

        // 青色小三角：固定在正上方，表示“圆屏的上方”
        pFill.color = cyan
        path.reset()
        path.moveTo(cx - dp(10f), cy - r - dp(14f))
        path.lineTo(cx + dp(10f), cy - r - dp(14f))
        path.lineTo(cx, cy - r - dp(1f))
        path.close()
        c.drawPath(path, pFill)

        c.drawCircle(cx, cy, r, pFace)
        c.drawCircle(cx, cy, r, pRim)

        // 刻度：每 5° 一根，30° 长一点，0/90/180/270 粉色加粗
        for (i in 0 until 72) {
            val deg = i * 5
            val a = Math.toRadians(deg.toDouble())
            val major = deg % 90 == 0
            val mid = deg % 30 == 0
            val len = when { major -> dp(16f); mid -> dp(13f); else -> dp(7f) }
            pTick.color = when { major -> pink; mid -> Color.parseColor("#D8D2E0"); else -> Color.parseColor("#6E6680") }
            pTick.strokeWidth = when { major -> dp(3.5f); mid -> dp(2f); else -> dp(1.2f) }
            val r0 = r - dp(6f) - len
            val r1 = r - dp(6f)
            val sx = sin(a).toFloat(); val sy = -cos(a).toFloat()
            c.drawLine(cx + sx * r0, cy + sy * r0, cx + sx * r1, cy + sy * r1, pTick)
        }

        // 粉色指针：从中心圆边上指向当前角度
        val hubR = r * 0.36f
        val a = Math.toRadians(angle.toDouble())
        val ux = sin(a).toFloat(); val uy = -cos(a).toFloat()
        val tipR = r - dp(28f)
        val headLen = dp(22f)
        pTick.color = pink; pTick.strokeWidth = dp(6f); pTick.strokeCap = Paint.Cap.BUTT
        c.drawLine(cx + ux * hubR, cy + uy * hubR, cx + ux * (tipR - headLen + dp(2f)), cy + uy * (tipR - headLen + dp(2f)), pTick)
        pTick.strokeCap = Paint.Cap.ROUND
        val bx = cx + ux * (tipR - headLen); val by = cy + uy * (tipR - headLen)
        val px = -uy; val py = ux                            // 垂直方向
        val hw = dp(12f)
        pFill.color = pink
        path.reset()
        path.moveTo(cx + ux * tipR, cy + uy * tipR)
        path.lineTo(bx + px * hw, by + py * hw)
        path.lineTo(bx - px * hw, by - py * hw)
        path.close()
        c.drawPath(path, pFill)

        // 中心圆：角度数字 + “按住转动”
        c.drawCircle(cx, cy, hubR, pHub)
        pBig.textSize = hubR * 0.62f
        c.drawText("$angle°", cx, cy + pBig.textSize * 0.25f, pBig)
        pSmall.textSize = hubR * 0.2f
        c.drawText("按住转动", cx, cy + hubR * 0.58f, pSmall)
    }

    // ---- 手指转圆盘 ----
    private var lastFinger = 0.0
    private var acc = 0.0
    private var startVal = 0

    private fun fingerAngle(e: MotionEvent): Double {
        val top = dp(16f)
        val cy = top + (height - top) / 2f
        return Math.toDegrees(atan2((e.x - width / 2f).toDouble(), -(e.y - cy).toDouble()))
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                // v7.4 只有按在圆盘上才转；圆盘外面（左右空白处）交给页面，照常上下滑动
                val top = dp(16f)
                val r = min(width / 2f, (height - top) / 2f) - dp(4f)
                val cy = top + (height - top) / 2f
                val dx = e.x - width / 2f; val dy = e.y - cy
                if (dx * dx + dy * dy > (r + dp(6f)) * (r + dp(6f))) return false
                parent?.requestDisallowInterceptTouchEvent(true)      // 别让外面的页面跟着滚动
                lastFinger = fingerAngle(e)
                acc = 0.0
                startVal = angle
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                val f = fingerAngle(e)
                var d = f - lastFinger
                if (d > 180) d -= 360
                if (d < -180) d += 360
                lastFinger = f
                acc += d
                var v = (((startVal + acc).roundToInt() % 360) + 360) % 360
                val near = ((v + 45) / 90 % 4) * 90                    // 最近的 90° 整数倍
                var diff = v - near
                if (diff > 180) diff -= 360
                if (diff < -180) diff += 360
                if (kotlin.math.abs(diff) <= 4) v = near               // 吸住
                if (v != angle) {
                    if (v % 90 == 0) performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
                    angle = v
                    invalidate()
                    onChange?.invoke(angle, false)
                }
                return true
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                parent?.requestDisallowInterceptTouchEvent(false)
                onChange?.invoke(angle, true)
                return true
            }
        }
        return super.onTouchEvent(e)
    }
}
