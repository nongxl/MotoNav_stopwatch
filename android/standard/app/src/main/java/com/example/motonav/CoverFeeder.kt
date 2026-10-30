package com.example.motonav

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Rect
import android.os.Handler
import android.os.Looper
import java.util.concurrent.Executors

/**
 * 负责音乐播放专辑封面的居中裁切缩放、量化转码与 BLE Bulk 推送管理。
 */
object CoverFeeder {
    private const val TARGET_SIZE = CoverPacket.TARGET_SIZE // 140x140

    private val worker = Executors.newSingleThreadExecutor()
    private val main = Handler(Looper.getMainLooper())
    private var coverId = 0
    private var lastKey: String = ""
    private var lastWireBytes: ByteArray? = null

    fun needsCover(key: String): Boolean = key.isBlank() || key != lastKey || lastWireBytes == null

    fun feedCover(bitmap: Bitmap?, key: String, force: Boolean = false) {
        if (bitmap == null) {
            clearCover()
            return
        }

        if (key.isNotBlank() && key == lastKey) {
            val cached = lastWireBytes
            if (force && cached != null) {
                coverId = (coverId + 1) and 0xFF
                BleLink.sendBulk(CoverPacket.TYPE_COVER, coverId, cached)
            }
            if (!force || cached != null) return
        }

        val origW = bitmap.width
        val origH = bitmap.height
        if (origW <= 0 || origH <= 0) {
            clearCover()
            return
        }
        lastKey = key
        lastWireBytes = null
        val expectedKey = key

        // 居中等比裁切并缩放到 140x140
        val side = minOf(origW, origH)
        val srcX = (origW - side) / 2
        val srcY = (origH - side) / 2
        val srcRect = Rect(srcX, srcY, srcX + side, srcY + side)
        val dstRect = Rect(0, 0, TARGET_SIZE, TARGET_SIZE)

        val scaled = Bitmap.createBitmap(TARGET_SIZE, TARGET_SIZE, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(scaled)
        val paint = Paint(Paint.FILTER_BITMAP_FLAG)
        canvas.drawBitmap(bitmap, srcRect, dstRect, paint)

        val argb = IntArray(TARGET_SIZE * TARGET_SIZE)
        scaled.getPixels(argb, 0, TARGET_SIZE, 0, 0, TARGET_SIZE, TARGET_SIZE)
        scaled.recycle()

        worker.execute {
            val encoded = CoverPacket.encode(argb, TARGET_SIZE, TARGET_SIZE) ?: return@execute
            main.post {
                if (lastKey != expectedKey) return@post
                lastWireBytes = encoded.wireBytes
                coverId = (coverId + 1) and 0xFF
                android.util.Log.i("CoverFeeder", "Sending cover bulk: id=$coverId, size=${encoded.wireBytes.size}")
                BleLink.sendBulk(CoverPacket.TYPE_COVER, coverId, encoded.wireBytes)
            }
        }
    }

    fun clearCover() {
        if (lastKey.isEmpty() && lastWireBytes == null) return
        lastKey = ""
        lastWireBytes = null
        BleLink.cancelBulk(CoverPacket.TYPE_COVER)
        BleLink.send("#ZCOV,0")
    }
}

