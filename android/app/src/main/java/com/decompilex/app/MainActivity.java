package com.decompilex.app;

import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.net.Uri;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.InputStream;
import java.io.ByteArrayOutputStream;

public class MainActivity extends Activity {
    private static final int PICK_FILE = 1001;

    static {
        System.loadLibrary("DecompileX");
    }

    private TextView resultText;

    public native String getNativeVersion();
    public native String analyzeBytes(byte[] data);

    private int dp(int value) {
        return (int) (value * getResources().getDisplayMetrics().density);
    }

    private GradientDrawable background(int color, int radius) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color);
        drawable.setCornerRadius(dp(radius));
        return drawable;
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);

        getWindow().setStatusBarColor(Color.rgb(12, 16, 21));
        getWindow().setNavigationBarColor(Color.rgb(12, 16, 21));

        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setBackgroundColor(Color.rgb(12, 16, 21));

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER_HORIZONTAL);
        root.setPadding(dp(24), dp(48), dp(24), dp(24));

        TextView title = new TextView(this);
        title.setText("DecompileX");
        title.setTextSize(34);
        title.setTextColor(Color.rgb(67, 220, 150));
        title.setTypeface(null, Typeface.BOLD);
        title.setGravity(Gravity.CENTER);

        root.addView(title);

        TextView subtitle = new TextView(this);
        subtitle.setText("Game Binary Analyzer");
        subtitle.setTextSize(16);
        subtitle.setTextColor(Color.LTGRAY);
        subtitle.setGravity(Gravity.CENTER);
        subtitle.setPadding(0, dp(8), 0, dp(28));

        root.addView(subtitle);

        TextView version = new TextView(this);
        version.setText(getNativeVersion());
        version.setTextSize(14);
        version.setTextColor(Color.GRAY);
        version.setGravity(Gravity.CENTER);

        root.addView(version);

        Button openButton = new Button(this);
        openButton.setText("OPEN FILE");
        openButton.setTextColor(Color.BLACK);
        openButton.setAllCaps(false);
        openButton.setBackground(
            background(Color.rgb(67, 220, 150), 12)
        );

        LinearLayout.LayoutParams buttonParams =
            new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                dp(56)
            );

        buttonParams.topMargin = dp(36);
        root.addView(openButton, buttonParams);

        resultText = new TextView(this);
        resultText.setText("Select a file to analyze.");
        resultText.setTextSize(15);
        resultText.setTextColor(Color.WHITE);
        resultText.setTypeface(Typeface.MONOSPACE);
        resultText.setPadding(
            dp(18), dp(18), dp(18), dp(18)
        );
        resultText.setBackground(
            background(Color.rgb(25, 32, 40), 12)
        );

        LinearLayout.LayoutParams resultParams =
            new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            );

        resultParams.topMargin = dp(24);
        root.addView(resultText, resultParams);

        openButton.setOnClickListener(v -> openFile());

        scroll.addView(root);
        setContentView(scroll);
    }

    private void openFile() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.setType("*/*");
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        startActivityForResult(intent, PICK_FILE);
    }

    @Override
    protected void onActivityResult(
        int requestCode,
        int resultCode,
        Intent data
    ) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode != PICK_FILE ||
            resultCode != RESULT_OK ||
            data == null)
            return;

        Uri uri = data.getData();

        if (uri == null)
            return;

        try (InputStream stream =
                 getContentResolver().openInputStream(uri)) {

            if (stream == null)
                throw new Exception("Unable to open file");

            ByteArrayOutputStream output =
                new ByteArrayOutputStream();

            byte[] buffer = new byte[4096];
            int count;

            while ((count = stream.read(buffer)) != -1 &&
                   output.size() < 4096) {
                int remaining = 4096 - output.size();
                output.write(
                    buffer, 0, Math.min(count, remaining)
                );
            }

            byte[] bytes = output.toByteArray();
            String format = analyzeBytes(bytes);

            resultText.setText(
                "File: " + uri.getLastPathSegment() +
                "\n\nFormat: " + format +
                "\n\nHeader bytes read: " + bytes.length +
                "\n\nAnalysis completed."
            );

        } catch (Exception exception) {
            resultText.setText(
                "Error: " + exception.getMessage()
            );
        }
    }
}
