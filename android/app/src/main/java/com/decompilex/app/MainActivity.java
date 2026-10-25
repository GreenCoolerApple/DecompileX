package com.decompilex.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.Color;
import android.database.Cursor;
import android.provider.DocumentsContract.Document;
import java.io.File;
import java.io.FileOutputStream;
import java.util.HashSet;
import java.util.Set;
import android.provider.OpenableColumns;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.GridLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.graphics.drawable.GradientDrawable;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

public class MainActivity extends Activity {
    static { System.loadLibrary("DecompileX"); }

    private static final int OPEN_FILE = 100;
    private static final int OPEN_COMPARE = 101;
    private static final int SAVE_BIN = 102;
    private static final int SAVE_REPORT = 103;
    private static final int PICK_ISO_TREE = 104;
    private static final int SAVE_ISO = 105;
    private static final long MAX_ISO_INPUT = 512L * 1024 * 1024;
    private static final int MAX_ISO_ENTRIES = 2000;
    private Uri isoTree;
    private static final int MAX_BYTES = 16 * 1024 * 1024;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final int bg = Color.rgb(10, 15, 22);
    private final int panel = Color.rgb(24, 33, 45);
    private final int accent = Color.rgb(55, 216, 149);
    private final int muted = Color.rgb(155, 171, 187);
    private TextView fileLabel;
    private TextView output;
    private byte[] loaded;
    private byte[] pendingSave;
    private String filename = "No file selected";
    private String currentReport = "Select a file to start.";

    private native String getNativeVersion();
    private native String analyzeBytes(byte[] data);
    private native String hexDump(byte[] data, int offset, int length);
    private native String extractStrings(byte[] data, int minimum);
    private native String entropy(byte[] data);
    private native String compareBytes(byte[] first, byte[] second);
    private native byte[] hexToBytes(String hex);
    private native String findHex(byte[] data, String pattern);
    private native String compileIsoNative(String source, String destination, String label);

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().setStatusBarColor(bg);
        getWindow().setNavigationBarColor(bg);
        showDashboard();
    }

    private GradientDrawable background(int color, int radius) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color);
        drawable.setCornerRadius(dp(radius));
        return drawable;
    }

    private int dp(int n) {
        return Math.round(n * getResources().getDisplayMetrics().density);
    }

    private TextView text(String value, int size, int color, boolean bold) {
        TextView view = new TextView(this);
        view.setText(value);
        view.setTextSize(size);
        view.setTextColor(color);
        if (bold) view.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        return view;
    }

    private void showDashboard() {
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setBackgroundColor(bg);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(20), dp(24), dp(20), dp(32));
        scroll.addView(root);

        TextView title = text("DecompileX", 32, accent, true);
        root.addView(title);
        TextView subtitle = text("BINARY ANALYSIS STUDIO  •  v" + getNativeVersion(), 12, muted, false);
        subtitle.setPadding(0, dp(4), 0, dp(24));
        root.addView(subtitle);

        LinearLayout workspace = new LinearLayout(this);
        workspace.setOrientation(LinearLayout.VERTICAL);
        workspace.setPadding(dp(16), dp(16), dp(16), dp(16));
        workspace.setBackground(background(panel, 16));
        root.addView(workspace);
        workspace.addView(text("BINARY WORKSPACE", 14, accent, true));
        fileLabel = text(filename, 14, Color.WHITE, false);
        fileLabel.setPadding(0, dp(12), 0, dp(12));
        workspace.addView(fileLabel);
        Button open = button("OPEN BINARY FILE", true);
        workspace.addView(open);
        open.setOnClickListener(v -> openDocument(OPEN_FILE));

        section(root, "BINARY FORENSICS");
        GridLayout grid = new GridLayout(this);
        grid.setColumnCount(2);
        root.addView(grid);
        tile(grid, "FILE INFO", () -> { byte[] data = loaded; analyzeAsync("FILE INFORMATION", () -> analyzeBytes(data)); });
        tile(grid, "HEX VIEWER", () -> askNumber("Hex offset (decimal)", "0", offset -> { byte[] data = loaded; analyzeAsync("HEX VIEWER", () -> hexDump(data, offset, 512)); }));
        tile(grid, "ASCII STRINGS", () -> { byte[] data = loaded; analyzeAsync("ASCII STRINGS", () -> extractStrings(data, 5)); });
        tile(grid, "ENTROPY", () -> { byte[] data = loaded; analyzeAsync("ENTROPY", () -> entropy(data)); });
        tile(grid, "CRC32", () -> { byte[] data = loaded; analyzeAsync("CRC32 / FILE INFO", () -> analyzeBytes(data)); });
        tile(grid, "COMPARE", () -> openDocument(OPEN_COMPARE));
        tile(grid, "FIND HEX", () -> askHexSearch());

        section(root, "BINARY BUILDER");
        Button bin = button("CREATE .BIN FROM HEX", false);
        root.addView(bin);
        bin.setOnClickListener(v -> createBinary());

        section(root, "ISO 9660 BUILDER");
        Button isoButton = button("CREATE ISO FROM FOLDER", false);
        root.addView(isoButton);
        isoButton.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            startActivityForResult(intent, PICK_ISO_TREE);
        });

        section(root, "REPORT TOOLS");
        Button saveReport = button("EXPORT REPORT AS .TXT", false);
        root.addView(saveReport);
        saveReport.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("text/plain");
            intent.putExtra(Intent.EXTRA_TITLE, "DecompileX-report.txt");
            startActivityForResult(intent, SAVE_REPORT);
        });

        section(root, "ANALYSIS OUTPUT");
        output = text(currentReport, 13, Color.rgb(221, 231, 240), false);
        output.setTypeface(Typeface.MONOSPACE);
        output.setTextIsSelectable(true);
        output.setPadding(dp(16), dp(16), dp(16), dp(16));
        output.setBackground(background(panel, 16));
        root.addView(output);

        TextView footer = text("DecompileX  •  C++17 / JNI  •  Offline", 11, muted, false);
        footer.setGravity(Gravity.CENTER);
        footer.setPadding(0, dp(26), 0, 0);
        root.addView(footer);
        setContentView(scroll);
    }

    private Button button(String label, boolean primary) {
        Button button = new Button(this);
        button.setText(label);
        button.setAllCaps(false);
        button.setTextColor(primary ? bg : accent);
        button.setBackground(background(primary ? accent : panel, 12));
        button.setPadding(dp(12), dp(10), dp(12), dp(10));
        return button;
    }

    private void section(LinearLayout root, String label) {
        TextView view = text(label, 13, accent, true);
        view.setPadding(0, dp(26), 0, dp(12));
        root.addView(view);
    }

    private void tile(GridLayout grid, String label, Runnable action) {
        Button tile = button(label, false);
        GridLayout.LayoutParams params = new GridLayout.LayoutParams();
        params.width = 0;
        params.height = dp(76);
        params.columnSpec = GridLayout.spec(GridLayout.UNDEFINED, 1f);
        params.setMargins(dp(3), dp(3), dp(3), dp(3));
        tile.setLayoutParams(params);
        grid.addView(tile);
        tile.setOnClickListener(v -> {
            if (loaded == null) {
                showMessage("Open a binary file first.");
                return;
            }
            action.run();
        });
    }

    private void showResult(String label, String value) {
        currentReport = label + "\n\n" + value;
        output.setText(currentReport);
    }

    private void analyzeAsync(String title, java.util.concurrent.Callable<String> operation) {
        currentReport = title + "\n\nAnalyzing...";
        output.setText(currentReport);
        worker.execute(() -> {
            try {
                String report = operation.call();
                runOnUiThread(() -> showResult(title, report));
            } catch (Exception e) {
                runOnUiThread(() -> showMessage("Analysis failed: " + e.getMessage()));
            }
        });
    }

    private void askHexSearch() {
        EditText field = new EditText(this);
        field.setHint("Example: 4D 5A");
        new AlertDialog.Builder(this).setTitle("Find hexadecimal bytes")
            .setView(field).setNegativeButton("Cancel", null)
            .setPositiveButton("Search", (dialog, which) -> {
                String pattern = field.getText().toString();
                byte[] snapshot = loaded;
                analyzeAsync("HEX SEARCH", () -> findHex(snapshot, pattern));
            }).show();
    }

    private String documentName(Uri uri) {
        try (Cursor cursor = getContentResolver().query(uri, null, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (index >= 0) return cursor.getString(index);
            }
        } catch (Exception ignored) { }
        return "Selected binary";
    }

    private void showMessage(String message) {
        new AlertDialog.Builder(this).setMessage(message).setPositiveButton("OK", null).show();
    }

    private void askNumber(String title, String defaultValue, java.util.function.IntConsumer callback) {
        EditText field = new EditText(this);
        field.setInputType(InputType.TYPE_CLASS_NUMBER);
        field.setText(defaultValue);
        new AlertDialog.Builder(this).setTitle(title).setView(field)
            .setNegativeButton("Cancel", null)
            .setPositiveButton("Show", (dialog, which) -> {
                try { callback.accept(Integer.parseInt(field.getText().toString())); }
                catch (NumberFormatException e) { showMessage("Invalid offset."); }
            }).show();
    }

    private void createBinary() {
        EditText field = new EditText(this);
        field.setHint("Example: 44 65 63 6F 6D 70 69 6C 65 58");
        field.setMinLines(3);
        new AlertDialog.Builder(this)
            .setTitle("Hex to BIN")
            .setView(field)
            .setNegativeButton("Cancel", null)
            .setPositiveButton("Save .bin", (dialog, which) -> {
                pendingSave = hexToBytes(field.getText().toString());
                if (pendingSave == null) { showMessage("Invalid hex or more than 1 MB."); return; }
                Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                intent.addCategory(Intent.CATEGORY_OPENABLE);
                intent.setType("application/octet-stream");
                intent.putExtra(Intent.EXTRA_TITLE, "DecompileX.bin");
                startActivityForResult(intent, SAVE_BIN);
            }).show();
    }

    private void requestIsoDestination(Uri tree) {
        isoTree = tree;
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/octet-stream");
        intent.putExtra(Intent.EXTRA_TITLE, "DecompileX.iso");
        startActivityForResult(intent, SAVE_ISO);
    }

    private static final class IsoCopyState {
        long bytes;
        int entries;
    }

    private void stageIsoFolder(Uri tree, String documentId, File target,
                                IsoCopyState state, int depth) throws Exception {
        if (depth > 32) throw new Exception("Folder nesting exceeds 32 levels.");
        if (!target.isDirectory() && !target.mkdirs()) throw new Exception("Cannot create staging folder.");
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, documentId);
        String[] projection = {Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME,
                Document.COLUMN_MIME_TYPE, Document.COLUMN_SIZE};
        Set<String> used = new HashSet<>();
        try (Cursor cursor = getContentResolver().query(children, projection, null, null, null)) {
            if (cursor == null) throw new Exception("Cannot list selected folder.");
            while (cursor.moveToNext()) {
                if (++state.entries > MAX_ISO_ENTRIES) throw new Exception("Too many files or folders.");
                String id = cursor.getString(0);
                String name = cursor.getString(1);
                String mime = cursor.getString(2);
                if (name == null || name.isEmpty() || name.equals(".") || name.equals("..") ||
                        name.contains("/") || name.contains("\\") || name.indexOf('\0') >= 0)
                    throw new Exception("Unsafe filename in selected folder.");
                if (!used.add(name)) throw new Exception("Duplicate filename: " + name);
                File child = new File(target, name);
                if (Document.MIME_TYPE_DIR.equals(mime)) {
                    stageIsoFolder(tree, id, child, state, depth + 1);
                } else {
                    Uri item = DocumentsContract.buildDocumentUriUsingTree(tree, id);
                    try (InputStream input = getContentResolver().openInputStream(item);
                         FileOutputStream output = new FileOutputStream(child)) {
                        if (input == null) throw new Exception("Cannot read " + name);
                        byte[] buffer = new byte[65536];
                        int count;
                        while ((count = input.read(buffer)) != -1) {
                            if (Thread.currentThread().isInterrupted()) throw new Exception("Operation interrupted.");
                            state.bytes += count;
                            if (state.bytes > MAX_ISO_INPUT) throw new Exception("ISO input exceeds 512 MB.");
                            output.write(buffer, 0, count);
                        }
                    }
                }
            }
        }
    }

    private void removeTemporary(File file) {
        if (file == null || !file.exists()) return;
        File[] children = file.listFiles();
        if (children != null) for (File child : children) removeTemporary(child);
        file.delete();
    }

    private void createIso(Uri tree, Uri destination) {
        showResult("ISO 9660", "Preparing files and generating ISO... Please wait.");
        worker.execute(() -> {
            File staging = null;
            try {
                staging = new File(getCacheDir(), "iso-" + java.util.UUID.randomUUID());
                File input = new File(staging, "source");
                File iso = new File(staging, "result.iso");
                if (!staging.mkdirs()) throw new Exception("Cannot create temporary workspace.");
                IsoCopyState state = new IsoCopyState();
                stageIsoFolder(tree, DocumentsContract.getTreeDocumentId(tree), input, state, 0);
                String nativeResult = compileIsoNative(input.getAbsolutePath(), iso.getAbsolutePath(), "DECOMPILEX");
                if (nativeResult == null || !nativeResult.startsWith("OK|"))
                    throw new Exception(nativeResult == null ? "Native ISO compiler failed." : nativeResult);
                try (InputStream source = new java.io.FileInputStream(iso);
                     OutputStream target = getContentResolver().openOutputStream(destination, "w")) {
                    if (target == null) throw new Exception("Cannot write selected destination.");
                    byte[] buffer = new byte[65536];
                    int count;
                    while ((count = source.read(buffer)) != -1) target.write(buffer, 0, count);
                    target.flush();
                }
                String message = nativeResult.substring(3) + "\nSource bytes: " + state.bytes;
                runOnUiThread(() -> showResult("ISO CREATED", message));
            } catch (Exception error) {
                String message = error.getMessage();
                runOnUiThread(() -> showResult("ISO ERROR", message == null ? "Unknown error" : message));
            } finally {
                removeTemporary(staging);
            }
        });
    }

    @Override
    protected void onDestroy() {
        worker.shutdownNow();
        super.onDestroy();
    }

    private void openDocument(int request) {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        startActivityForResult(intent, request);
    }

    private byte[] readDocument(Uri uri) throws Exception {
        try (InputStream stream = getContentResolver().openInputStream(uri);
             ByteArrayOutputStream buffer = new ByteArrayOutputStream()) {
            if (stream == null) throw new Exception("Unable to open document.");
            byte[] chunk = new byte[65536];
            int count;
            while ((count = stream.read(chunk)) != -1) {
                if (buffer.size() + count > MAX_BYTES) throw new Exception("Maximum file size is 16 MB.");
                buffer.write(chunk, 0, count);
            }
            return buffer.toByteArray();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        try {
            if (requestCode == PICK_ISO_TREE) {
                requestIsoDestination(data.getData());
                return;
            }
            if (requestCode == SAVE_ISO) {
                if (isoTree != null) createIso(isoTree, data.getData());
                isoTree = null;
                return;
            }
            if (requestCode == SAVE_REPORT) {
                byte[] report = currentReport.getBytes(StandardCharsets.UTF_8);
                try (OutputStream stream = getContentResolver().openOutputStream(data.getData(), "wt")) {
                    if (stream == null) throw new Exception("Unable to export report.");
                    stream.write(report);
                }
                showMessage("Report exported successfully.");
                return;
            }
            if (requestCode == SAVE_BIN) {
                if (pendingSave == null) return;
                try (OutputStream stream = getContentResolver().openOutputStream(data.getData(), "w")) {
                    if (stream == null) throw new Exception("Unable to save document.");
                    stream.write(pendingSave);
                    stream.flush();
                }
                showResult("BIN CREATED", "Saved " + pendingSave.length + " bytes.");
                pendingSave = null;
                return;
            }
            Uri uri = data.getData();
            worker.execute(() -> {
                try {
                    byte[] content = readDocument(uri);
                    if (requestCode == OPEN_FILE) {
                        String name = documentName(uri);
                        String report = analyzeBytes(content);
                        runOnUiThread(() -> {
                            loaded = content;
                            filename = name;
                            fileLabel.setText(filename);
                            showResult("FILE INFORMATION", report);
                        });
                    } else if (requestCode == OPEN_COMPARE) {
                        byte[] first = loaded;
                        if (first == null) return;
                        String report = compareBytes(first, content);
                        runOnUiThread(() -> showResult("BINARY COMPARISON", report));
                    }
                } catch (Exception e) {
                    runOnUiThread(() -> showMessage("Operation failed: " + e.getMessage()));
                }
            });
        } catch (Exception e) {
            showMessage("Operation failed: " + e.getMessage());
        }
    }
}
