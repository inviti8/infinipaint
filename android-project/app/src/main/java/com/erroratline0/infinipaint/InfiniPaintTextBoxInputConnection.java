package com.erroratline0.infinipaint;

import static android.text.TextUtils.CAP_MODE_SENTENCES;

import android.content.*;
import android.graphics.RectF;
import android.os.Build;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.Handler;
import android.text.Editable;
import android.text.SpannableStringBuilder;
import android.view.*;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.CorrectionInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.HandwritingGesture;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputContentInfo;
import android.view.inputmethod.PreviewableHandwritingGesture;
import android.view.inputmethod.SurroundingText;
import android.view.inputmethod.TextAttribute;
import android.view.inputmethod.TextBoundsInfoResult;
import android.view.inputmethod.TextSnapshot;

import java.util.concurrent.Executor;
import java.util.function.Consumer;
import java.util.function.IntConsumer;

import android.content.*;
import android.os.Build;
import android.text.Editable;
import android.view.*;
import android.view.inputmethod.BaseInputConnection;
import android.widget.EditText;

import org.libsdl.app.SDL;
import org.libsdl.app.SDLActivity;

public class InfiniPaintTextBoxInputConnection extends BaseInputConnection {
    public static native void nativeCommitText(int textboxID, String text, int newCursorPosition);
    public static native void nativeDeleteSurroundingText(int textboxID, int beforeLength, int afterLength);
    public static native void nativeSetSelection(int textboxID, int start, int end);

    protected EditText mEditText;
    protected int mTextBoxID;

    InfiniPaintTextBoxInputConnection(View targetView, boolean fullEditor) {
        super(targetView, fullEditor);
        mEditText = new EditText(SDL.getContext());
    }

    public void setInitialData(int textboxID, String initialText, int begin, int end) {
        mTextBoxID = textboxID;
        mEditText.setText(initialText);
        mEditText.setSelection(begin, end);
    }

    @Override
    public Editable getEditable() {
        return mEditText.getEditableText();
    }

    @Override
    public boolean sendKeyEvent(KeyEvent event) {
        /*
         * This used to handle the keycodes from soft keyboard (and IME-translated input from hardkeyboard)
         * However, as of Ice Cream Sandwich and later, almost all soft keyboard doesn't generate key presses
         * and so we need to generate them ourselves in commitText.  To avoid duplicates on the handful of keys
         * that still do, we empty this out.
         */

        /*
         * Return DOES still generate a key event, however. So rather than using it as the 'click a button' key
         * as we do with physical keyboards, let's just use it to hide the keyboard.
         */

        if (event.getKeyCode() == KeyEvent.KEYCODE_ENTER) {
            if (SDLActivity.onNativeSoftReturnKey()) {
                return true;
            }
        }

        return super.sendKeyEvent(event);
    }

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
        if (!super.commitText(text, newCursorPosition))
            return false;
        nativeCommitText(mTextBoxID, text.toString(), newCursorPosition);
        return true;
    }

    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
        if (!super.setComposingText(text, newCursorPosition))
            return false;
        nativeCommitText(mTextBoxID, text.toString(), newCursorPosition);
        return true;
    }

    @Override
    public boolean deleteSurroundingText(int beforeLength, int afterLength) {
        if (!super.deleteSurroundingText(beforeLength, afterLength))
            return false;
        nativeDeleteSurroundingText(mTextBoxID, beforeLength, afterLength);
        return true;
    }

    @Override
    public boolean setSelection(int start, int end) {
        if(!super.setSelection(start, end))
            return false;
        nativeSetSelection(mTextBoxID, start, end);
        return true;
    }
}

