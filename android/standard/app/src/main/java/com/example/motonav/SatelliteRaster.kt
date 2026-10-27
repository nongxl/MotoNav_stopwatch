package com.example.motonav
import android.graphics.Bitmap
import java.util.concurrent.CancellationException
import kotlin.math.roundToInt

/** Provider-neutral wire packet; there is no network access in this encoder. */
class RasterResult(val bytes: ByteArray,val tiles: Int,val fromCache: Int,
                   val previewRaw: Bitmap?,val previewCls: Bitmap?)

/** Encode AMap satellite pixels in the existing 16-color firmware packet. */
object SatelliteRaster {
    const val SAT_MPP=3.5
    fun encode(f: MapGeo.Frame,cx: Double,cy: Double,half: Double,raw: IntArray,
               shouldCancel: () -> Boolean = { false }): RasterResult {
        fun checkCancelled(){if(Thread.currentThread().isInterrupted || shouldCancel())throw CancellationException()}
        checkCancelled()
        val n=(2*half/SAT_MPP).roundToInt()
        require(n>=16 && raw.size==n*n){"高德卫星图尺寸不正确"}
        val x0=(cx-half).roundToInt();val y0=(cy+half).roundToInt()
        val R=IntArray(raw.size){(raw[it] shr 16) and 255}
        val G=IntArray(raw.size){(raw[it] shr 8) and 255}
        val B=IntArray(raw.size){raw[it] and 255}
        // 3×3 模糊
        fun blur(a: IntArray): IntArray {
            val o = IntArray(n * n)
            for (j in 0 until n) for (i in 0 until n) {
                var sum = 0; var cnt = 0
                for (dj in -1..1) for (di in -1..1) {
                    val jj = j + dj; val ii = i + di
                    if (jj in 0 until n && ii in 0 until n) { sum += a[jj * n + ii]; cnt++ }
                }
                o[j * n + i] = sum / cnt
            }
            return o
        }
        val r1 = blur(R); val g1 = blur(G); val b1 = blur(B)
        // k 均值找 16 种颜色：先按亮度排序均匀取 16 个起点，迭代 10 轮
        val rnd = java.util.Random(7)
        val S = minOf(6000, n * n)
        val sIdx = IntArray(S) { rnd.nextInt(n * n) }
        val sorted = sIdx.sortedBy { r1[it] * 3 + g1[it] * 6 + b1[it] }
        val K = 16
        val cr = DoubleArray(K) { r1[sorted[(it * 2 + 1) * S / (2 * K)]].toDouble() }
        val cg = DoubleArray(K) { g1[sorted[(it * 2 + 1) * S / (2 * K)]].toDouble() }
        val cb = DoubleArray(K) { b1[sorted[(it * 2 + 1) * S / (2 * K)]].toDouble() }
        fun nearest(r: Int, g: Int, b: Int): Int {
            var best = 0; var bd = Double.MAX_VALUE
            for (k in 0 until K) {
                val dr = r - cr[k]; val dg = g - cg[k]; val db = b - cb[k]
                val d = dr * dr * 2 + dg * dg * 4 + db * db * 3
                if (d < bd) { bd = d; best = k }
            }
            return best
        }
        repeat(10) {
            checkCancelled()
            val sr = DoubleArray(K); val sg = DoubleArray(K); val sb = DoubleArray(K); val sc = IntArray(K)
            for (p in sIdx) { val k = nearest(r1[p], g1[p], b1[p]); sr[k] += r1[p].toDouble(); sg[k] += g1[p].toDouble(); sb[k] += b1[p].toDouble(); sc[k]++ }
            for (k in 0 until K) if (sc[k] > 0) { cr[k] = sr[k] / sc[k]; cg[k] = sg[k] / sc[k]; cb[k] = sb[k] / sc[k] }
        }
        val idx = ByteArray(n * n) { p -> nearest(r1[p], g1[p], b1[p]).toByte() }
        val pal = IntArray(K) { k ->
            val r = cr[k].roundToInt().coerceIn(0, 255); val g = cg[k].roundToInt().coerceIn(0, 255); val b = cb[k].roundToInt().coerceIn(0, 255)
            ((r shr 3) shl 11) or ((g shr 2) shl 5) or (b shr 3)
        }
        checkCancelled()
        val bytes = MapGeo.encodeRaster(f.id, x0, y0, n, n, (SAT_MPP * 1000).roundToInt(), idx, pal)
        if (bytes.size > 480_000) throw Exception("卫星图压缩后 ${bytes.size / 1024} KB，超过圆屏缓冲")
        // 预览
        val pv = 300
        val pr = IntArray(pv * pv); val pc = IntArray(pv * pv)
        for (j in 0 until pv) for (i in 0 until pv) {
            val k = (j * n / pv) * n + (i * n / pv)
            pr[j * pv + i] = (0xFF shl 24) or (R[k] shl 16) or (G[k] shl 8) or B[k]
            val q = idx[k].toInt()
            pc[j * pv + i] = (0xFF shl 24) or (cr[q].roundToInt().coerceIn(0, 255) shl 16) or (cg[q].roundToInt().coerceIn(0, 255) shl 8) or cb[q].roundToInt().coerceIn(0, 255)
        }
        val bRaw = Bitmap.createBitmap(pr, pv, pv, Bitmap.Config.ARGB_8888)
        val bCls = Bitmap.createBitmap(pc, pv, pv, Bitmap.Config.ARGB_8888)
        return RasterResult(bytes,1,0,bRaw,bCls)
    }
}
