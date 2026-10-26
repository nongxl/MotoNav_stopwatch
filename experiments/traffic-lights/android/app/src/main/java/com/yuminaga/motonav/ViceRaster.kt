package com.yuminaga.motonav

import java.io.ByteArrayOutputStream
import java.util.zip.Deflater
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.roundToInt
import kotlin.math.sin
import kotlin.math.sqrt

/** 普通轮廓保留局部平面，复杂交汇点使用共享顶点的四材质覆盖率。 */
object ViceRaster {
    private val pairA = intArrayOf(0, 0, 0, 1, 1, 2)
    private val pairB = intArrayOf(1, 2, 3, 2, 3, 3)
    private val offsets = doubleArrayOf(
        0.0, 0.0625, 0.125, 0.1875, 0.25, 0.3125,
        0.375, 0.4375, 0.5, 0.5625, 0.625, 0.6875,
        0.8125, 1.0, 1.25, 1.625
    )
    private val nx = DoubleArray(64) { cos(it * PI / 32.0) }
    private val ny = DoubleArray(64) { sin(it * PI / 32.0) }
    private const val GUARD = 2047 // 保留编号：纯材质的接缝过渡格，不占路口表。

    private fun encodeOffset(distance: Double): Int {
        val d = abs(distance)
        var best = 0
        var error = abs(d - offsets[0])
        for (i in 1 until offsets.size) {
            val next = abs(d - offsets[i])
            if (next < error) { best = i; error = next }
        }
        return best or if (distance < 0) 16 else 0
    }

    private fun distance(code: Int, dx: Double, dy: Double): Double {
        val offset = (code shr 6) and 31
        return nx[code and 63] * dx + ny[code and 63] * dy +
            offsets[offset and 15] * if (offset and 16 != 0) -1 else 1
    }

    /** 单个源像素方形与局部边界的相交面积，用来构造共享顶点。 */
    private fun coverage(code: Int, distance: Double): Double {
        val a = maxOf(abs(nx[code and 63]), abs(ny[code and 63]))
        val b = minOf(abs(nx[code and 63]), abs(ny[code and 63]))
        val t = distance + (a + b) * 0.5
        return when {
            t <= 0 -> 0.0
            t >= a + b -> 1.0
            b < 0.00001 -> t / a
            t < b -> t * t / (2 * a * b)
            t <= a -> (t - b * 0.5) / a
            else -> 1 - (a + b - t) * (a + b - t) / (2 * a * b)
        }
    }

    @JvmOverloads
    fun append(base: ByteArray, clean: ByteArray, w: Int, h: Int, fallback: ByteArray,
               shouldCancel: () -> Boolean = { false }): ByteArray {
        fun stopped() = Thread.currentThread().isInterrupted || shouldCancel()
        if (stopped()) return base
        val bw = (w + 15) / 16
        val bh = (h + 15) / 16
        val geometry = ByteArray(bw * bh * 512)
        fun address(x: Int, y: Int) = (((y / 16) * bw + x / 16) * 256 +
            (y and 15) * 16 + (x and 15)) * 2
        fun get(x: Int, y: Int): Int {
            val k = address(x, y)
            return (geometry[k].toInt() and 255) or ((geometry[k + 1].toInt() and 255) shl 8)
        }
        fun put(x: Int, y: Int, code: Int) {
            val k = address(x, y)
            geometry[k] = code.toByte(); geometry[k + 1] = (code shr 8).toByte()
        }
        val counts = IntArray(4)
        val col = IntArray(3)
        val row = IntArray(3)
        for (y in 0 until h) for (x in 0 until w) {
            if (x == 0 && stopped()) return base
            val material = fallback[y * w + x].toInt() and 3
            var code = material shl 14
            if (x > 0 && y > 0 && x < w - 1 && y < h - 1) {
                counts.fill(0)
                for (dy in -1..1) for (dx in -1..1) counts[clean[(y + dy) * w + x + dx].toInt() and 3]++
                var a = -1
                var b = -1
                var kinds = 0
                for (c in 0..3) if (counts[c] > 0) {
                    if (a < 0) a = c else if (b < 0) b = c
                    kinds++
                }
                if (kinds >= 3) {
                    // 先标记交汇点；第二遍使用已经生成的邻格轮廓计算共享顶点。
                    code = code or (7 shl 11)
                } else if (kinds == 2) {
                    col.fill(0); row.fill(0)
                    var sum = 0
                    for (dy in -1..1) for (dx in -1..1) {
                        if ((clean[(y + dy) * w + x + dx].toInt() and 3) == b) {
                            val wx = if (dx == 0) 2 else 1
                            val wy = if (dy == 0) 2 else 1
                            sum += wx * wy
                            col[dx + 1] += wy; row[dy + 1] += wx
                        }
                    }
                    val gx = (col[2] - col[0]) / 8.0
                    val gy = (row[2] - row[0]) / 8.0
                    val gradient = sqrt(gx * gx + gy * gy)
                    if (gradient >= 0.12) {
                        var d = (sum / 16.0 - 0.5) / gradient
                        if (a == 0 && b == 1) d += 0.75
                        if (abs(d) <= 1.875) {
                            val direction = (atan2(gy, gx) * 32.0 / PI).roundToInt() and 63
                            val pair = pairA.indices.first { pairA[it] == a && pairB[it] == b } + 1
                            code = code or direction or (encodeOffset(d) shl 6) or (pair shl 11)
                        }
                    }
                }
            }
            put(x, y, code)
        }

        val vertexCache = HashMap<Int, Int>()
        fun vertex(vx: Int, vy: Int): Int = vertexCache.getOrPut(vy * (w + 1) + vx) {
            val mass = DoubleArray(4)
            var samples = 0
            // 同一个顶点只计算一次，相邻路口小块的公共边严格使用同一组覆盖率。
            for (cy in vy - 1..vy) for (cx in vx - 1..vx) {
                if (cx !in 0 until w || cy !in 0 until h) continue
                val code = get(cx, cy)
                val pair = (code shr 11) and 7
                if (pair in 1..6) {
                    val f = coverage(code, distance(code, vx - cx - 0.5, vy - cy - 0.5))
                    mass[pairA[pair - 1]] += 1 - f
                    mass[pairB[pair - 1]] += f
                } else mass[(code shr 14) and 3] += 1.0
                samples++
            }
            // 保留交汇点的窄过渡，压低弱占比，避免整块路口变软。
            for (c in 0..3) mass[c] = maxOf(0.0, mass[c] / samples.coerceAtLeast(1) - 0.08)
            val total = mass.sum()
            val q = IntArray(4)
            val remainder = DoubleArray(4)
            if (total > 0) {
                for (c in 0..3) {
                    val target = mass[c] * 15 / total
                    q[c] = floor(target).toInt()
                    remainder[c] = target - q[c]
                }
                repeat(15 - q.sum()) {
                    var best = 0
                    for (c in 1..3) if (remainder[c] > remainder[best]) best = c
                    q[best]++; remainder[best] = -1.0
                }
            } else q[0] = 15
            q[0] or (q[1] shl 4) or (q[2] shl 8) or (q[3] shl 12)
        }
        val patchIds = HashMap<Long, Int>()
        val patches = ArrayList<Long>()
        for (y in 1 until h - 1) for (x in 1 until w - 1) {
            if (x == 1 && stopped()) return base
            val code = get(x, y)
            if ((code shr 11) and 7 != 7) continue
            val key = vertex(x, y).toLong() or (vertex(x + 1, y).toLong() shl 16) or
                (vertex(x, y + 1).toLong() shl 32) or (vertex(x + 1, y + 1).toLong() shl 48)
            var index = patchIds[key]
            if (index == null && patches.size < GUARD) {
                index = patches.size; patches.add(key); patchIds[key] = index
            }
            put(x, y, (code and 0xC000) or (7 shl 11) or (index ?: GUARD))
        }

        // 标记需要和邻格衔接的少量纯色格。保留编号不参与传播，避免把整张底图标成过渡区域。
        val sx = intArrayOf(-1, 1, 0, 0, -1, 1, -1, 1)
        val sy = intArrayOf(0, 0, -1, 1, -1, -1, 1, 1)
        for (y in 1 until h - 1) for (x in 1 until w - 1) {
            if (x == 1 && stopped()) return base
            val code = get(x, y)
            if ((code shr 11) and 7 != 0) continue
            val material = (code shr 14) and 3
            var guard = false
            for (i in sx.indices) {
                val neighbor = get(x + sx[i], y + sy[i])
                val pair = (neighbor shr 11) and 7
                if (pair == 7 && (neighbor and GUARD) < patches.size) guard = true
                else if (pair in 1..6 && (material == pairA[pair - 1] || material == pairB[pair - 1]) &&
                    abs(distance(neighbor, -sx[i] * 0.5, -sy[i] * 0.5)) < 0.95) guard = true
                if (guard) break
            }
            if (guard) put(x, y, code or (7 shl 11) or GUARD)
        }

        if (stopped()) return base
        pack(base, geometry, patches, 3, Deflater.BEST_SPEED)?.let { return it }
        if (stopped()) return base
        pack(base, geometry, patches, 3, Deflater.DEFAULT_COMPRESSION)?.let { return it }
        // 数据过大时保留上一版精细轮廓，避免直接退回粗糙地图。
        for (y in 0 until h) for (x in 0 until w) {
            if (x == 0 && stopped()) return base
            val code = get(x, y)
            if ((code shr 11) and 7 == 7) put(x, y, code and 0xC000)
        }
        return pack(base, geometry, emptyList(), 2, Deflater.DEFAULT_COMPRESSION) ?: base
    }

    private fun pack(base: ByteArray, geometry: ByteArray, patches: List<Long>, version: Int, level: Int): ByteArray? {
        val payload = ByteArray(geometry.size + patches.size * 8)
        geometry.copyInto(payload)
        for (i in patches.indices) for (b in 0..7) payload[geometry.size + i * 8 + b] = (patches[i] ushr (b * 8)).toByte()
        val header = if (version == 3) 12 else 8
        val packed = ByteArrayOutputStream()
        val deflater = Deflater(level)
        try {
            deflater.setInput(payload); deflater.finish()
            val buffer = ByteArray(8192)
            while (!deflater.finished()) {
                val n = deflater.deflate(buffer)
                if (n <= 0) return null
                packed.write(buffer, 0, n)
                if (base.size + header + packed.size() > 480_000) return null
            }
        } finally { deflater.end() }
        val extension = packed.toByteArray()
        val out = ByteArrayOutputStream(base.size + header + extension.size)
        out.write(base)
        out.write(byteArrayOf('V'.code.toByte(), 'C'.code.toByte(), 'E'.code.toByte(), version.toByte()))
        for (shift in 0..24 step 8) out.write(extension.size shr shift)
        if (version == 3) {
            out.write(patches.size); out.write(patches.size shr 8)
            out.write(0); out.write(0)
        }
        out.write(extension)
        return out.toByteArray()
    }
}
