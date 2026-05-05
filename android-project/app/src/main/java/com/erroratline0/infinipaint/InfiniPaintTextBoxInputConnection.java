package com.erroratline0.infinipaint;

import static android.text.TextUtils.CAP_MODE_SENTENCES;

import android.content.*;
import android.graphics.RectF;
import android.os.Build;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.Handler;
import android.text.Editable;
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

class InfiniPaintTextBoxInputConnection extends BaseInputConnection
{
    protected EditText mEditText;
    protected String mCommittedText = "";

    InfiniPaintTextBoxInputConnection(View targetView, boolean fullEditor) {
        super(targetView, fullEditor);
        mEditText = new EditText(SDL.getContext());
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
         * Return DOES still generate a key event, however.  So rather than using it as the 'click a button' key
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
        if (!super.commitText(text, newCursorPosition)) {
            return false;
        }
        updateText();
        return true;
    }

    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
        if (!super.setComposingText(text, newCursorPosition)) {
            return false;
        }
        updateText();
        return true;
    }

    @Override
    public boolean deleteSurroundingText(int beforeLength, int afterLength) {
        // Workaround to capture backspace key. Ref: http://stackoverflow.com/questions>/14560344/android-backspace-in-webview-baseinputconnection
        // and https://bugzilla.libsdl.org/show_bug.cgi?id=2265
        if (beforeLength > 0 && afterLength == 0) {
            // backspace(s)
            while (beforeLength-- > 0) {
            }
            return true;
        }

        if (!super.deleteSurroundingText(beforeLength, afterLength)) {
            return false;
        }
        updateText();
        return true;
    }

    protected void updateText() {
        final Editable content = getEditable();
        if (content == null) {
            return;
        }

        String text = content.toString();
        int compareLength = Math.min(text.length(), mCommittedText.length());
        int matchLength, offset;

        /* Backspace over characters that are no longer in the string */
        for (matchLength = 0; matchLength < compareLength; ) {
            int codePoint = mCommittedText.codePointAt(matchLength);
            if (codePoint != text.codePointAt(matchLength)) {
                break;
            }
            matchLength += Character.charCount(codePoint);
        }
        /* FIXME: This doesn't handle graphemes, like '🌬️' */
        for (offset = matchLength; offset < mCommittedText.length(); ) {
            int codePoint = mCommittedText.codePointAt(offset);
            offset += Character.charCount(codePoint);
        }

        if (matchLength < text.length()) {
            String pendingText = text.subSequence(matchLength, text.length()).toString();
            if (!SDLActivity.dispatchingKeyEvent()) {
                for (offset = 0; offset < pendingText.length(); ) {
                    int codePoint = pendingText.codePointAt(offset);
                    if (codePoint == '\n') {
                        if (SDLActivity.onNativeSoftReturnKey()) {
                            return;
                        }
                    }
                    /* Higher code points don't generate simulated scancodes */
                    if (codePoint > 0 && codePoint < 128) {
                    }
                    offset += Character.charCount(codePoint);
                }
            }
        }
        mCommittedText = text;
    }
}

//class InfiniPaintTextBoxInputConnection extends BaseInputConnection {
//    InfiniPaintTextBoxInputConnection(View targetView, boolean fullEditor) {
//        super(targetView, fullEditor);
//    }
//
//    public native void nativeBeginBatchEdit();
//    public native void nativeClearMetaKeyStates(int states);
//    public native void nativeSetComposingText(CharSequence text, int newCursorPosition);
//    public native void nativeSetComposingRegion(int start, int end);
//    public native void nativeSetSelection(int start, int end);
//    public native void nativeCommitCorrection(int offset, CharSequence oldText, CharSequence newText);
//    public native void nativeCommitText(CharSequence text, int newCursorPosition);
//    public native void nativeDeleteSurroundingText(int beforeLength, int afterLength);
//    public native void nativeDeleteSurroundingTextInCodePoints(int beforeLength, int afterLength);
//    public native void nativeEndBatchEdit();
//    public native void nativeFinishComposingText();
//    public native CharSequence nativeGetSelectedText();
//    public native CharSequence nativeGetTextAfterCursor(int n);
//    public native CharSequence nativeGetTextBeforeCursor(int n);
//    public native void nativePerformContextMenuAction(int action);
//    public native void nativePerformEditorAction(int action);
//
//    @Override
//    public boolean beginBatchEdit() {
//        nativeBeginBatchEdit();
//        return true;
//    }
//
//    @Override
//    public boolean clearMetaKeyStates(int states) {
//        nativeClearMetaKeyStates(states);
//        return true;
//    }
//
//    @Override
//    public boolean performSpellCheck() {
//        return true;
//    }
//
//    @Override
//    public TextSnapshot takeSnapshot() {
//        return null;
//    }
//
//    @Override
//    public CharSequence getTextBeforeCursor(int n, int flags) {
//        return nativeGetTextBeforeCursor(n);
//    }
//
//    @Override
//    public CharSequence getTextAfterCursor(int n, int flags) {
//        return nativeGetTextAfterCursor(n);
//    }
//
//    @Override
//    public CharSequence getSelectedText(int i) {
//        return nativeGetSelectedText();
//    }
//
//    @Override
//    public int getCursorCapsMode(int i) {
//        return CAP_MODE_SENTENCES;
//    }
//
//    @Override
//    public ExtractedText getExtractedText(ExtractedTextRequest extractedTextRequest, int i) {
//        return null;
//    }
//
//    @Override
//    public boolean deleteSurroundingText(int i, int i1) {
//        nativeDeleteSurroundingText(i, i1);
//        return true;
//    }
//
//    @Override
//    public boolean deleteSurroundingTextInCodePoints(int beforeLength, int afterLength) {
//        nativeDeleteSurroundingTextInCodePoints(beforeLength, afterLength);
//        return true;
//    }
//
//    @Override
//    public boolean setComposingRegion(int i, int i1) {
//        nativeSetComposingRegion(i, i1);
//        return true;
//    }
//
//    @Override
//    public boolean finishComposingText() {
//        nativeFinishComposingText();
//        return true;
//    }
//
//    @Override
//    public boolean commitCompletion(CompletionInfo completionInfo) {
//        return true;
//    }
//
//    @Override
//    public boolean commitCorrection(CorrectionInfo correctionInfo) {
//        nativeCommitCorrection(correctionInfo.getOffset(), correctionInfo.getOldText(), correctionInfo.getNewText());
//        return true;
//    }
//
//    @Override
//    public boolean setSelection(int i, int i1) {
//        nativeSetSelection(i, i1);
//        return true;
//    }
//
//    @Override
//    public boolean performEditorAction(int i) {
//        nativePerformEditorAction(i);
//        return true;
//    }
//
//    @Override
//    public boolean performContextMenuAction(int i) {
//        nativePerformContextMenuAction(i);
//        return true;
//    }
//
//    @Override
//    public boolean endBatchEdit() {
//        nativeEndBatchEdit();
//        return true;
//    }
//
//    @Override
//    public boolean sendKeyEvent(KeyEvent keyEvent) {
//        return true;
//    }
//
//    @Override
//    public boolean reportFullscreenMode(boolean b) {
//        return true;
//    }
//
//    @Override
//    public boolean performPrivateCommand(String s, Bundle bundle) {
//        return false;
//    }
//
//    @Override
//    public boolean requestCursorUpdates(int i) {
//        return true;
//    }
//
//    @Override
//    public Handler getHandler() {
//        return null;
//    }
//
//    @Override
//    public boolean commitContent(InputContentInfo inputContentInfo, int i, Bundle bundle) {
//        return true;
//    }
//
//    @Override
//    public boolean commitText(CharSequence text, int newCursorPosition) {
//        nativeCommitText(text, newCursorPosition);
//        return true;
//    }
//
//    @Override
//    public boolean setComposingText(CharSequence text, int newCursorPosition) {
//        nativeSetComposingText(text, newCursorPosition);
//        return true;
//    }
//}

