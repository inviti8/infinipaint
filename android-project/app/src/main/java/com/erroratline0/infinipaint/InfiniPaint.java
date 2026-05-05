package com.erroratline0.infinipaint;

import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.RelativeLayout;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLDummyEdit;
import org.libsdl.app.SDLSurface;

/**
 * A sample wrapper class that just calls SDLActivity
 */

public class InfiniPaint extends SDLActivity {

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Window window = this.getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        window.clearFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
        window.clearFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION);
        window.setStatusBarColor(android.graphics.Color.TRANSPARENT);
        window.setNavigationBarColor(android.graphics.Color.TRANSPARENT);
        // Transparent navigation
        //if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
        //    window.setNavigationBarContrastEnforced(false);
    }

    protected SDLSurface createSDLSurface(Context context) {
        return new InfiniPaintSurface(context);
    }

    static class StartInfiniPaintTextInputTask implements Runnable {
        public StartInfiniPaintTextInputTask() {}

        @Override
        public void run() {
            RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(1, 1);

            if (mTextEdit == null) {
                mTextEdit = new InfiniPaintTextBoxView(getContext());
                mLayout.addView(mTextEdit, params);
            }
            else
                mTextEdit.setLayoutParams(params);

            mTextEdit.setVisibility(View.VISIBLE);
            mTextEdit.requestFocus();
            InputMethodManager imm = (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
            imm.showSoftInput(mTextEdit, 0);

            if (imm.isAcceptingText())
                onNativeScreenKeyboardShown();
        }
    }

    static public void startTextInput() {
        mSingleton.commandHandler.post(new StartInfiniPaintTextInputTask());
    }

    static InfiniPaintTextBoxView mTextEdit;
}
