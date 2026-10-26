package com.yuminaga.motonav

/** A road mask from a label-free SDK image, independent of Vice City's materials/buildings. */
object MinimalRoadRaster {
    fun isRoad(c: Int): Boolean {
        val r=(c ushr 16) and 255; val g=(c ushr 8) and 255; val b=c and 255
        // Standard AMap white roads and yellow/orange arterial roads. Buildings, water,
        // green space and their boundaries never acquire a material in this mask.
        return (r>=254 && g>=254 && b>=254) ||
            (r>=200 && g in 125..245 && b+25<g && r>=g && r-b>=45)
    }
    fun indices(raw: IntArray,n: Int): ByteArray {
        require(raw.size==n*n)
        val mask=ByteArray(raw.size){if(isRoad(raw[it]))1 else 0}
        // Remove isolated SDK icons/noise, with no widening and no building extraction.
        val cleaned=MapGeo.cleanRaster(mask,n,n)
        for(i in cleaned.indices)if(cleaned[i].toInt()!=1)cleaned[i]=0
        return cleaned
    }
}
