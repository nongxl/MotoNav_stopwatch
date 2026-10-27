package com.yuminaga.motonav;

/** Formats administrative areas without substituting a street or a weather station for the district. */
public final class WeatherArea {
    private WeatherArea() {}

    private static String clean(String value) {
        if (value == null) return "";
        String text = value.trim();
        return text.equals("市辖区") || text.equals("县") ? "" : text;
    }

    public static String format(String city, String district) {
        String c = clean(city);
        String d = clean(district);
        if (d.isEmpty()) return c.isEmpty() ? "区域未获取" : c + " · 区县未获取";
        if (c.isEmpty() || c.equals(d)) return d;
        if (d.startsWith(c)) return d;
        return c + " · " + d;
    }

    private static boolean isDistrict(String text) {
        return text.endsWith("区") && !text.endsWith("自治区")
                || text.endsWith("县") || text.endsWith("旗");
    }

    public static String fromGeocoder(String locality, String subLocality, String subAdminArea) {
        String city = clean(locality);
        String sub = clean(subLocality);
        String admin = clean(subAdminArea);
        if (isDistrict(sub)) return format(city.isEmpty() ? admin : city, sub);
        if (isDistrict(city)) return format(admin, city);
        if (isDistrict(admin)) return format(city, admin);
        // A county-level city can be returned as locality beneath its prefecture-level city.
        if (city.endsWith("市") && admin.endsWith("市") && !city.equals(admin)) {
            return format(admin, city);
        }
        return format(city.isEmpty() ? admin : city, "");
    }
}
