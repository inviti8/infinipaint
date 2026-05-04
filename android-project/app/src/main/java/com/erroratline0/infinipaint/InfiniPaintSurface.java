package com.erroratline0.infinipaint;

import android.content.Context;
import android.graphics.Insets;
import android.os.Build;
import android.view.View;
import android.view.WindowInsets;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

public class InfiniPaintSurface extends SDLSurface {
    protected InfiniPaintSurface(Context context) {
        super(context);
    }

    public static native void nativeInfiniPaintUpdateIMESafeArea(int top, int bottom, int left, int right);

    // Window inset calculations are different from SDL defaults
    @Override
    public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
        if (Build.VERSION.SDK_INT >= 30 /* Android 11 (R) */) {
            Insets combined = insets.getInsets(WindowInsets.Type.systemBars() |
                    //WindowInsets.Type.systemGestures() |
                    //WindowInsets.Type.mandatorySystemGestures() |
                    WindowInsets.Type.tappableElement() |
                    WindowInsets.Type.displayCutout());

            Insets imeInsets = insets.getInsets(WindowInsets.Type.systemBars() |
                    //WindowInsets.Type.systemGestures() |
                    //WindowInsets.Type.mandatorySystemGestures() |
                    WindowInsets.Type.tappableElement() |
                    WindowInsets.Type.displayCutout() |
                    WindowInsets.Type.ime());

            nativeInfiniPaintUpdateIMESafeArea(imeInsets.top, imeInsets.bottom, imeInsets.left, imeInsets.right);

            SDLActivity.onNativeInsetsChanged(combined.left, combined.right, combined.top, combined.bottom);

            if (insets.isVisible(WindowInsets.Type.ime())) {
                if (!mKeyboardVisible) {
                    mKeyboardVisible = true;
                    SDLActivity.onNativeScreenKeyboardShown();
                }
            } else {
                if (mKeyboardVisible) {
                    mKeyboardVisible = false;
                    SDLActivity.onNativeScreenKeyboardHidden();
                }
            }
        }

        // Pass these to any child views in case they need them
        return insets;
    }
}
