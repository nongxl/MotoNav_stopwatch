package com.example.motonav;

/** 把北向、无倾斜的 SDK 截图采样回圆屏的米坐标网格；不允许越界补空白。 */
public final class AmapSampleGrid {
    private AmapSampleGrid() {}

    static double mercator(double lat) {
        double r = Math.toRadians(lat);
        return Math.log(Math.tan(Math.PI / 4 + r / 2));
    }

    public static int[] resample(int[] pixels, int width, int height,
            double left, double top, double right, double bottom,
            double north, double south, int size) {
        if (width <= 0 || height <= 0 || size <= 0 || pixels.length != width * height
                || !Double.isFinite(left + top + right + bottom + north + south)
                || left < 0 || top < 0 || right > width || bottom > height
                || right <= left || bottom <= top || north <= south
                || north >= 85 || south <= -85) {
            throw new IllegalArgumentException("截图没有完整覆盖地图范围");
        }
        int[] cols = new int[size];
        int[] rows = new int[size];
        double mn = mercator(north), ms = mercator(south);
        for (int i = 0; i < size; i++) {
            double u = (i + 0.5) / size;
            cols[i] = (int) Math.floor(left + u * (right - left));
            double lat = north + (south - north) * u;
            rows[i] = (int) Math.floor(top + (mn - mercator(lat)) / (mn - ms) * (bottom - top));
            if (cols[i] < 0 || cols[i] >= width || rows[i] < 0 || rows[i] >= height) {
                throw new IllegalArgumentException("截图采样越界，保留旧地图");
            }
        }
        int[] result = new int[size * size];
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int c = pixels[rows[y] * width + cols[x]];
                if ((c >>> 24) < 250) throw new IllegalArgumentException("截图仍有透明区域，保留旧地图");
                result[y * size + x] = c;
            }
        }
        return result;
    }
}
