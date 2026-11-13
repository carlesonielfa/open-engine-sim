package org.openenginesim.app;

import android.content.res.AssetManager;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.provider.MediaStore;
import android.content.ContentValues;
import android.content.ContentResolver;
import android.os.Build;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

public class OpenEngineSimActivity extends SDLActivity {
    private static final String TAG = "OpenEngineSim";
    private static final int ENGINE_FILE_REQUEST = 4107;
    private static final String ASSET_VERSION = "0.2.2-android-alpha39";
    private TextView diagnosticView;
    private TextView portCreditView;
    private TextView audioGlitchView;
    private String assetStatus = "ASSETS: AINDA NAO VERIFICADOS";
    private volatile String pendingEngineScript = "";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Lock the simulator to landscape before SDL creates its surface so
        // startup never briefly builds a portrait-sized GL surface.
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
        try {
            syncAssets();
            assetStatus = validateAssets();
        } catch (IOException exception) {
            Log.e(TAG, "Failed to extract simulator assets", exception);
            assetStatus = "ASSETS: FALHOU\n" + exception;
        }
        super.onCreate(savedInstanceState);
        publishPendingNativeCrash();
        updateNativeStatus("Aguardando entrada no codigo C++...");
        showPortCredit();
    }

    @Override
    protected void onResume() {
        super.onResume();
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }

    private void showPortCredit() {
        runOnUiThread(() -> {
            if (portCreditView == null) {
                portCreditView = new TextView(this);
                portCreditView.setText("Port by M2RCOS");
                portCreditView.setTextColor(Color.WHITE);
                portCreditView.setTextSize(14.0f);
                portCreditView.setGravity(Gravity.BOTTOM | Gravity.START);
                portCreditView.setPadding(24, 16, 24, 18);
                portCreditView.setShadowLayer(3.0f, 1.0f, 1.0f, Color.BLACK);
                addContentView(portCreditView, new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            }
            portCreditView.bringToFront();
            portCreditView.setVisibility(View.VISIBLE);
        });
    }

    public void showAudioGlitch(final String status) {
        runOnUiThread(() -> {
            if (audioGlitchView == null) {
                audioGlitchView = new TextView(this);
                audioGlitchView.setTextColor(Color.WHITE);
                audioGlitchView.setBackgroundColor(0xD9B00020);
                audioGlitchView.setTextSize(15.0f);
                audioGlitchView.setGravity(Gravity.CENTER);
                audioGlitchView.setPadding(18, 8, 18, 8);
                addContentView(audioGlitchView, new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
                audioGlitchView.setX(24.0f);
                audioGlitchView.setY(24.0f);
            }
            audioGlitchView.removeCallbacks(hideAudioGlitchRunnable);
            audioGlitchView.setText("AUDIO: " + status);
            audioGlitchView.bringToFront();
            audioGlitchView.setVisibility(View.VISIBLE);
            audioGlitchView.postDelayed(hideAudioGlitchRunnable, 320);
        });
    }

    private final Runnable hideAudioGlitchRunnable = () -> {
        if (audioGlitchView != null) audioGlitchView.setVisibility(View.GONE);
    };

    public void openEngineFilePicker() {
        runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {
                "text/plain", "application/octet-stream", "application/x-mr"
            });
            startActivityForResult(intent, ENGINE_FILE_REQUEST);
        });
    }

    public String consumeSelectedEngineScript() {
        final String selected = pendingEngineScript;
        pendingEngineScript = "";
        return selected;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != ENGINE_FILE_REQUEST || resultCode != RESULT_OK || data == null) return;
        final Uri uri = data.getData();
        if (uri == null) return;
        try {
            String displayName = queryDisplayName(uri);
            if (displayName == null || displayName.isEmpty()) displayName = "imported_engine.mr";
            if (!displayName.toLowerCase().endsWith(".mr")) {
                Log.w(TAG, "Rejected non-.mr engine file: " + displayName);
                return;
            }
            displayName = displayName.replaceAll("[^A-Za-z0-9._-]", "_");
            final File engineDir = new File(getFilesDir(), "assets/engines/user");
            if (!engineDir.mkdirs() && !engineDir.isDirectory()) throw new IOException("Could not create " + engineDir);
            final File destination = new File(engineDir, displayName);
            try (InputStream input = getContentResolver().openInputStream(uri);
                 FileOutputStream output = new FileOutputStream(destination)) {
                if (input == null) throw new IOException("Could not open selected file");
                final byte[] buffer = new byte[64 * 1024];
                int count;
                while ((count = input.read(buffer)) != -1) output.write(buffer, 0, count);
            }
            pendingEngineScript = "engines/user/" + displayName;
            Log.i(TAG, "Imported engine script: " + pendingEngineScript);
        } catch (Exception exception) {
            Log.e(TAG, "Failed to import engine script", exception);
        }
    }

    private String queryDisplayName(Uri uri) {
        try (Cursor cursor = getContentResolver().query(uri, new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                final int index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (index >= 0) return cursor.getString(index);
            }
        }
        return null;
    }

    public void updateNativeStatus(final String nativeStatus) {
        runOnUiThread(() -> {
            if (diagnosticView == null) {
                diagnosticView = new TextView(this);
                diagnosticView.setTextColor(Color.GREEN);
                diagnosticView.setBackgroundColor(Color.BLACK);
                diagnosticView.setTextSize(16.0f);
                diagnosticView.setGravity(Gravity.TOP | Gravity.START);
                diagnosticView.setPadding(32, 48, 32, 32);
                addContentView(diagnosticView, new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            }
            diagnosticView.setVisibility(View.VISIBLE);
            diagnosticView.setText("OPEN ENGINE SIM - ANDROID DIAGNOSTICO V3\n\n" +
                "JAVA/SDL ACTIVITY: OK\n" + assetStatus + "\n\nNATIVO/GPU:\n" + nativeStatus);
        });
    }

    public void showRenderDiagnostics(final String renderStatus) {
        runOnUiThread(() -> {
            if (diagnosticView == null) return;
            diagnosticView.setBackgroundColor(0xCC000000);
            diagnosticView.setTextColor(Color.GREEN);
            diagnosticView.setTextSize(15.0f);
            diagnosticView.setVisibility(View.VISIBLE);
            diagnosticView.setText("OPEN ENGINE SIM - RENDER DIAGNOSTICO V5\n\n" + renderStatus);
            // Keep the diagnostic visible long enough to photograph, then reveal
            // the SDL/GPU surface automatically for the actual visual test.
            diagnosticView.removeCallbacks(hideDiagnosticsRunnable);
            diagnosticView.postDelayed(hideDiagnosticsRunnable, 2500);
        });
    }

    private final Runnable hideDiagnosticsRunnable = () -> {
        if (diagnosticView != null) diagnosticView.setVisibility(View.GONE);
        if (portCreditView != null) portCreditView.setVisibility(View.GONE);
    };

    private void publishPendingNativeCrash() {
        final File crash = new File(getFilesDir(), "native_crash_last.txt");
        if (!crash.isFile()) return;
        try {
            final String text = new String(Files.readAllBytes(crash.toPath()), StandardCharsets.UTF_8);
            final String published = writeMrLog(text);
            Log.e(TAG, "Recovered native crash report: " + published);
            crash.delete();
        } catch (Exception exception) {
            Log.e(TAG, "Failed to publish native crash report", exception);
        }
    }

    public void checkpointMrLog(final String text) {
        try {
            final File dir = new File(getFilesDir(), "logs");
            if (!dir.mkdirs() && !dir.isDirectory()) throw new IOException("Could not create " + dir);
            Files.write(new File(dir, "mr_last_session.txt").toPath(),
                text.getBytes(StandardCharsets.UTF_8));
        } catch (Exception exception) {
            Log.e(TAG, "Failed to write internal MR checkpoint", exception);
        }

        // Show every checkpoint immediately on the same green diagnostic
        // overlay used during Android startup. This runs on the UI thread even
        // if the native compiler later blocks.
        showMrDiagnostics(text);
    }

    public String writeMrLog(final String text) {
        final String name = "mr_error_" + System.currentTimeMillis() + ".txt";
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                ContentValues values = new ContentValues();
                values.put(MediaStore.MediaColumns.DISPLAY_NAME, name);
                values.put(MediaStore.MediaColumns.MIME_TYPE, "text/plain");
                values.put(MediaStore.MediaColumns.RELATIVE_PATH,
                    Environment.DIRECTORY_DOCUMENTS + "/OpenEngineSim/logs");
                ContentResolver resolver = getContentResolver();
                Uri uri = resolver.insert(MediaStore.Files.getContentUri("external"), values);
                if (uri == null) throw new IOException("MediaStore insert returned null");
                try (java.io.OutputStream output = resolver.openOutputStream(uri, "w")) {
                    if (output == null) throw new IOException("Could not open MediaStore output");
                    output.write(text.getBytes(StandardCharsets.UTF_8));
                    output.flush();
                }
                return "Documentos/OpenEngineSim/logs/" + name;
            }

            final File documents = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS);
            final File logDir = new File(documents, "OpenEngineSim/logs");
            if (!logDir.mkdirs() && !logDir.isDirectory()) throw new IOException("Could not create " + logDir);
            final File destination = new File(logDir, name);
            Files.write(destination.toPath(), text.getBytes(StandardCharsets.UTF_8));
            return destination.getAbsolutePath();
        } catch (Exception exception) {
            Log.e(TAG, "Failed to write MR log", exception);
            return "LOG SAVE FAILED: " + exception.getClass().getSimpleName() + ": " + exception.getMessage();
        }
    }

    public void showMrDiagnostics(final String status) {
        runOnUiThread(() -> {
            if (diagnosticView == null) {
                diagnosticView = new TextView(this);
                addContentView(diagnosticView, new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            }
            diagnosticView.removeCallbacks(hideDiagnosticsRunnable);
            diagnosticView.setBackgroundColor(Color.BLACK);
            diagnosticView.setTextColor(Color.GREEN);
            diagnosticView.setTextSize(15.0f);
            diagnosticView.setGravity(Gravity.TOP | Gravity.START);
            diagnosticView.setPadding(32, 48, 32, 32);
            diagnosticView.setVisibility(View.VISIBLE);
            diagnosticView.setText("OPEN ENGINE SIM - MR DIAGNOSTICO TEMPORARIO\n\n" + status +
                "\n\nO overlay fecha automaticamente apos o checkpoint.");
            // Diagnostic overlay must never become the app itself. Each new
            // checkpoint refreshes this timeout; once native code keeps running
            // normally, reveal the SDL surface again.
            diagnosticView.postDelayed(hideDiagnosticsRunnable, 2200);
        });
    }

    public void hideNativeDiagnostics() {
        runOnUiThread(() -> {
            if (diagnosticView != null) diagnosticView.setVisibility(View.GONE);
            if (portCreditView != null) portCreditView.setVisibility(View.GONE);
        });
    }

    private String validateAssets() {
        final File root = new File(getFilesDir(), "assets");
        final File vertex = new File(root, "shaders/engine_sim.vertex.spv");
        final File fragment = new File(root, "shaders/engine_sim.fragment.spv");
        final File mainScript = new File(root, "main.mr");
        return "ASSETS: " + (root.isDirectory() ? "OK" : "FALHOU") +
            "\nVERTEX SPV: " + (vertex.isFile() ? "OK" : "FALHOU") +
            "\nFRAGMENT SPV: " + (fragment.isFile() ? "OK" : "FALHOU") +
            "\nMAIN.MR: " + (mainScript.isFile() ? "OK" : "FALHOU");
    }

    private void syncAssets() throws IOException {
        final File destinationRoot = new File(getFilesDir(), "assets");
        final File marker = new File(destinationRoot, ".asset-version");
        if (marker.isFile()) {
            final String installedVersion = new String(Files.readAllBytes(marker.toPath()), StandardCharsets.UTF_8).trim();
            if (ASSET_VERSION.equals(installedVersion)) return;
        }
        // User imports live below the extracted asset tree so Piranha can
        // resolve their normal relative imports. Preserve that directory when
        // packaged assets need to be refreshed after an app update.
        final File userEngines = new File(destinationRoot, "engines/user");
        final File userBackup = new File(getFilesDir(), "user-engines-backup");
        deleteRecursively(userBackup);
        final boolean hadUserEngines = userEngines.isDirectory();
        if (hadUserEngines && !userEngines.renameTo(userBackup)) {
            throw new IOException("Could not preserve imported engines before asset refresh");
        }

        try {
            deleteRecursively(destinationRoot);
            if (!destinationRoot.mkdirs() && !destinationRoot.isDirectory()) throw new IOException("Could not create " + destinationRoot);
            copyAssetTree(getAssets(), "", destinationRoot);

            if (hadUserEngines) {
                final File restoredParent = userEngines.getParentFile();
                if (restoredParent != null && !restoredParent.mkdirs() && !restoredParent.isDirectory()) {
                    throw new IOException("Could not recreate imported-engine directory");
                }
                if (!userBackup.renameTo(userEngines)) {
                    throw new IOException("Could not restore imported engines after asset refresh");
                }
            }

            Files.write(marker.toPath(), ASSET_VERSION.getBytes(StandardCharsets.UTF_8));
        } catch (IOException exception) {
            // If refresh failed after the backup was made, keep the user's
            // imported files in the backup rather than deleting them.
            Log.e(TAG, "Asset refresh failed; preserved user engines at " + userBackup, exception);
            throw exception;
        }
    }

    private static void copyAssetTree(AssetManager manager, String assetPath, File destination) throws IOException {
        final String[] children = manager.list(assetPath);
        if (children != null && children.length > 0) {
            if (!destination.mkdirs() && !destination.isDirectory()) throw new IOException("Could not create " + destination);
            for (String child : children) copyAssetTree(manager, assetPath.isEmpty() ? child : assetPath + "/" + child, new File(destination, child));
            return;
        }
        final File parent = destination.getParentFile();
        if (parent != null && !parent.mkdirs() && !parent.isDirectory()) throw new IOException("Could not create " + parent);
        try (InputStream input = manager.open(assetPath); FileOutputStream output = new FileOutputStream(destination)) {
            final byte[] buffer = new byte[64 * 1024];
            int count;
            while ((count = input.read(buffer)) != -1) output.write(buffer, 0, count);
        }
    }

    private static void deleteRecursively(File file) throws IOException {
        if (!file.exists()) return;
        final File[] children = file.listFiles();
        if (children != null) for (File child : children) deleteRecursively(child);
        if (!file.delete()) throw new IOException("Could not delete " + file);
    }
}
