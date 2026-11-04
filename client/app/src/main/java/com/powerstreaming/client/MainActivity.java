package com.powerstreaming.client;

import android.app.Activity;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

public final class MainActivity extends Activity implements SurfaceHolder.Callback {
    private SurfaceView surface;
    private RtpH264Receiver receiver;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        immersive();
        surface = new SurfaceView(this);
        surface.getHolder().addCallback(this);
        setContentView(surface);
    }

    private void immersive() { getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
            View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE); }

    @Override public void surfaceCreated(SurfaceHolder holder) {
        receiver = new RtpH264Receiver(holder.getSurface(), "0.0.0.0", 5000);
        receiver.start();
    }
    @Override public void surfaceChanged(SurfaceHolder h, int f, int w, int ht) { }
    @Override public void surfaceDestroyed(SurfaceHolder holder) {
        if (receiver != null) receiver.shutdown();
        receiver = null;
    }
}
