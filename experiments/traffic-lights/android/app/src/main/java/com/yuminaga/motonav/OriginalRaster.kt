package com.yuminaga.motonav

/** Original AMap colors/labels are sampled directly and packed in the screen's 16-color protocol. */
object OriginalRaster {
    fun encode(f: MapGeo.Frame,x0: Int,y0: Int,n: Int,raw: IntArray,stop: () -> Boolean): ByteArray {
        require(raw.size==n*n)
        val colors=OriginalMapColors.quantize(raw){stop()}
        val bytes=MapGeo.encodeRaster(f.id,x0,y0,n,n,2000,colors.indices,colors.palette)
        // Never silently replace an original map with Vice City when transmission is too large.
        require(bytes.size<=480000){"高德原版底图数据过大，保留当前地图等待重试"}
        return bytes
    }
}
