package com.example.motonav;

public final class WeatherAreaTest {
    private static int checks;
    private static void expect(String expected, String actual) {
        if (!expected.equals(actual)) throw new AssertionError("Expected " + expected + "; got " + actual);
        checks++;
    }
    public static void main(String[] args) {
        expect("成都市 · 武侯区", WeatherArea.format("成都市", "武侯区"));
        expect("北京市 · 朝阳区", WeatherArea.format("北京市", "朝阳区"));
        expect("阿坝藏族羌族自治州 · 汶川县", WeatherArea.format("阿坝藏族羌族自治州", "汶川县"));
        expect("苏州市 · 昆山市", WeatherArea.format("苏州市", "昆山市"));
        expect("东莞市", WeatherArea.format("东莞市", "东莞市"));
        expect("北京市朝阳区", WeatherArea.format("北京市", "北京市朝阳区"));
        expect("武侯区", WeatherArea.format(null, " 武侯区 "));
        expect("成都市 · 区县未获取", WeatherArea.format("成都市", "市辖区"));
        expect("区域未获取", WeatherArea.format(null, null));
        expect("成都市 · 武侯区", WeatherArea.fromGeocoder("成都市", "武侯区", "成都市"));
        expect("成都市 · 武侯区", WeatherArea.fromGeocoder("成都市", "望江路街道", "武侯区"));
        expect("成都市 · 郫都区", WeatherArea.fromGeocoder("郫都区", "犀浦街道", "成都市"));
        expect("苏州市 · 昆山市", WeatherArea.fromGeocoder("昆山市", "玉山镇", "苏州市"));
        expect("汶川县", WeatherArea.fromGeocoder(null, "汶川县", null));
        expect("成都市 · 区县未获取", WeatherArea.fromGeocoder("成都市", "望江路街道", "成都市"));
        expect("区域未获取", WeatherArea.fromGeocoder(null, "玉山镇", null));
        expect("乌鲁木齐市 · 区县未获取", WeatherArea.fromGeocoder("乌鲁木齐市", "新疆维吾尔自治区", null));
        System.out.println("PASS: " + checks + " district, county, municipality and unavailable-address cases");
    }
}
