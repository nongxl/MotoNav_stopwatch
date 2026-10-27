package android.content;
public interface SharedPreferences {
    boolean getBoolean(String key, boolean value);
    String getString(String key, String value);
    Editor edit();
    interface Editor {
        Editor putString(String key, String value);
        Editor putBoolean(String key, boolean value);
        boolean commit();
    }
}
