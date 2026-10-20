package org.libcna.museum;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.system.Os;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * The museum's Android entry point. SDLActivity loads libmain.so (the museum, see the ANDROID branch of
 * ../../CMakeLists.txt) and calls its SDL_main.
 *
 * The museum reads Content/ through file paths, which APK assets are not, so the packaged Content/ and
 * GamerServices/ are copied into the app's own storage once per installed version and MUSEUM_CONTENT points there.
 *
 * Desktop command-line options arrive as intent extras, so a script can be run without touching the screen:
 *   adb shell am start -n org.libcna.museum/.MuseumActivity --es script 'goto:in-touch;wait:30;stats;quit'
 * Extras: script (--script), scriptfile (--script-file, e.g. a file pushed into the app's external files directory),
 * room (--room), renderer (--renderer); "args" adds further options separated by '|'.
 */
public class MuseumActivity extends SDLActivity {
    private static final String TAG = "museum";

    @Override
    protected String[] getLibraries() {
        return new String[]{"SDL3", "main"};
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            File root = getFilesDir();
            String stamp = String.valueOf(getPackageManager().getPackageInfo(getPackageName(), 0).lastUpdateTime);
            File marker = new File(root, "content.stamp");
            if (!marker.isFile() || !stamp.equals(readSmall(marker))) {
                for (String top : new String[]{"Content", "GamerServices"}) copyAssets(getAssets(), top, new File(root, top));
                try (OutputStream out = new FileOutputStream(marker)) { out.write(stamp.getBytes("UTF-8")); }
                Log.i(TAG, "[museum][android] copied the packaged Content into " + root);
            }
            Os.setenv("MUSEUM_CONTENT", new File(root, "Content").getAbsolutePath(), true);
            getExternalFilesDir(null);   // created, so adb can push a script file into it
            Os.setenv("XDG_DATA_HOME", new File(root, "data").getAbsolutePath(), true);
        } catch (Exception e) {
            Log.e(TAG, "[museum][android] preparing Content failed: " + e);
        }
        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getArguments() {
        List<String> args = new ArrayList<>();
        Bundle extras = getIntent() != null ? getIntent().getExtras() : null;
        if (extras != null) {
            for (String[] option : new String[][]{{"script", "--script"}, {"scriptfile", "--script-file"}, {"room", "--room"}, {"renderer", "--renderer"}}) {
                String value = extras.getString(option[0]);
                if (value != null && !value.isEmpty()) { args.add(option[1]); args.add(value); }
            }
            String more = extras.getString("args");
            if (more != null && !more.isEmpty()) for (String a : more.split("\\|")) if (!a.isEmpty()) args.add(a);
        }
        return args.toArray(new String[0]);
    }

    private static void copyAssets(AssetManager assets, String path, File target) throws IOException {
        String[] children = assets.list(path);
        if (children != null && children.length > 0) {
            if (!target.isDirectory() && !target.mkdirs()) throw new IOException("cannot create " + target);
            for (String child : children) copyAssets(assets, path + "/" + child, new File(target, child));
            return;
        }
        try (InputStream in = assets.open(path); OutputStream out = new FileOutputStream(target)) {
            byte[] buffer = new byte[65536];
            for (int n; (n = in.read(buffer)) > 0; ) out.write(buffer, 0, n);
        }
    }

    private static String readSmall(File file) throws IOException {
        try (InputStream in = new java.io.FileInputStream(file)) {
            byte[] data = new byte[64];
            int n = in.read(data);
            return n > 0 ? new String(data, 0, n, "UTF-8") : "";
        }
    }
}
