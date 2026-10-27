package com.example.motonav

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Typeface
import kotlin.math.ceil

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（v1.7 新增）
 *
 * 把中文文字渲染成 4 位灰度小图片（每个像素 16 级灰度，两个像素占一个字节），
 * 发给圆屏显示。这样圆屏不需要中文字库，字体也是手机系统的，清晰好看。
 */
object TextImage {
    class Img(val w: Int, val h: Int, val data: ByteArray)

    /**
     * @param maxW  图片最大宽度（像素），文字太长会先缩小字号，再不行就截断加省略号
     * @param sizePx 期望字号（像素）
     * @return 文字为空时返回 null
     */
    fun render(text: String, maxW: Int, sizePx: Float, minSizePx: Float = 16f): Img? {
        if (text.isBlank()) return null
        val p = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            typeface = Typeface.DEFAULT_BOLD
            color = Color.WHITE
            textSize = sizePx
        }
        var t = text.trim()
        while (p.measureText(t) > maxW && p.textSize > minSizePx) p.textSize = p.textSize - 1f
        if (p.measureText(t) > maxW) {
            while (t.length > 1 && p.measureText("$t…") > maxW) t = t.dropLast(1)
            t = "$t…"
        }
        val fm = p.fontMetrics
        val h = ceil(fm.descent - fm.ascent).toInt().coerceIn(1, 60)
        val w = ceil(p.measureText(t)).toInt().coerceIn(1, maxW)

        val bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        Canvas(bmp).drawText(t, 0f, -fm.ascent, p)
        val px = IntArray(w * h)
        bmp.getPixels(px, 0, w, 0, 0, w, h)
        bmp.recycle()

        val bpr = (w + 1) / 2
        val out = ByteArray(bpr * h)
        for (y in 0 until h) {
            for (x in 0 until w) {
                val a = (px[y * w + x] ushr 24) and 0xFF
                val nib = ((a + 8) / 17).coerceIn(0, 15)
                if (nib == 0) continue
                val i = y * bpr + (x shr 1)
                out[i] = if (x and 1 == 0) (out[i].toInt() or (nib shl 4)).toByte()
                         else (out[i].toInt() or nib).toByte()
            }
        }
        return Img(w, h, out)
    }
}
