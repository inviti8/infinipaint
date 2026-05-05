package com.erroratline0.infinipaint;

import android.content.*;
import android.text.InputType;
import android.view.*;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

public class InfiniPaintTextBoxView extends View
{
    InputConnection ic;

    InfiniPaintTextBoxView(Context context) {
        super(context);
        setFocusableInTouchMode(true);
        setFocusable(true);
    }

    @Override
    public boolean onCheckIsTextEditor() {
        return true;
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        ic = new InfiniPaintTextBoxInputConnection(this, true);

        outAttrs.inputType = InputType.TYPE_CLASS_TEXT;
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI |
                EditorInfo.IME_FLAG_NO_FULLSCREEN /* API 11 */ |
                EditorInfo.IME_ACTION_DONE;

        return ic;
    }
}

