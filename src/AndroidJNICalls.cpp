#ifdef __ANDROID__
#include "AndroidJNICalls.hpp"
#include <jni.h>
#include "InputManager.hpp"
#include "MainProgram.hpp"
#include <SDL3/SDL_system.h>
#include <Helpers/Logger.hpp>
#include <string>

namespace AndroidJNICalls {
    InputManager* globalInputManager;
    std::mutex globalCallMutex;

    void startTextInput() {
        JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
        Logger::get().log("INFO", std::to_string((uint64_t)env) + " jni env value");
        jobject activity = (jobject)SDL_GetAndroidActivity();
        Logger::get().log("INFO", std::to_string((uint64_t)activity) + " activity value");
        jclass clazz = env->GetObjectClass(activity);
        jmethodID method_id = env->GetStaticMethodID(clazz, "startTextInput", "()V");
        Logger::get().log("INFO", std::to_string((uint64_t)method_id) + " method value");
        env->CallStaticVoidMethod(clazz, method_id);
        env->DeleteLocalRef(activity);
        env->DeleteLocalRef(clazz);
    }

    void stopTextInput() {

    }
}

jobject activity;
jmethodID startTextInputMethod;
JNIEnv* jniEnv;

using namespace AndroidJNICalls;

extern "C"
JNIEXPORT void JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintSurface_nativeInfiniPaintUpdateIMESafeArea(JNIEnv *env, jclass clazz, jint top, jint bottom, jint left, jint right) {
    if(globalInputManager) {
        std::scoped_lock a{globalInputManager->safeAreaWithoutIMEMutex};
        globalInputManager->safeAreaWithoutIME = {{left, top}, {globalInputManager->main.window.size.x() - right, globalInputManager->main.window.size.y() - bottom}};
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativeBeginBatchEdit(
        JNIEnv *env, jobject jobject1) {
    // TODO: implement nativeBeginBatchEdit()
}
extern "C"
JNIEXPORT void JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativeClearMetaKeyStates(
        JNIEnv *env, jobject jobject1, jint states) {
    // TODO: implement nativeClearMetaKeyStates()
}
extern "C"
JNIEXPORT void JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativeSetComposingText(
        JNIEnv *env, jobject jobject1, jobject text, jint new_cursor_position) {
    // TODO: implement nativeSetComposingText()
}
extern "C"
JNIEXPORT void JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativeSetComposingRegion(
        JNIEnv *env, jobject jobject1, jint start, jint end) {
    // TODO: implement nativeSetComposingRegion()
}
extern "C"
JNIEXPORT void JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativeSetSelection(JNIEnv *env,
                                                                                       jobject jobject1,
                                                                                       jint start,
                                                                                       jint end) {
    // TODO: implement nativeSetSelection()
}
extern "C"
JNIEXPORT void JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativeCommitCorrection(
        JNIEnv *env, jobject jobject1, jint offset, jobject old_text, jobject new_text) {
    // TODO: implement nativeCommitCorrection()
}
extern "C"
JNIEXPORT void JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativeCommitText(JNIEnv *env,
                                                                                     jobject jobject1,
                                                                                     jobject text,
                                                                                     jint new_cursor_position) {
    // TODO: implement nativeCommitText()
}
extern "C"
JNIEXPORT void JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativeDeleteSurroundingText(
        JNIEnv *env, jobject jobject1, jint before_length, jint after_length) {
    // TODO: implement nativeDeleteSurroundingText()
}
extern "C"
JNIEXPORT void JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativeDeleteSurroundingTextInCodePoints(
        JNIEnv *env, jobject jobject1, jint before_length, jint after_length) {
    // TODO: implement nativeDeleteSurroundingTextInCodePoints()
}
extern "C"
JNIEXPORT void JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativeEndBatchEdit(JNIEnv *env,
                                                                                       jobject jobject1) {
    // TODO: implement nativeEndBatchEdit()
}
extern "C"
JNIEXPORT void JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativeFinishComposingText(
        JNIEnv *env, jobject jobject1) {
    // TODO: implement nativeFinishComposingText()
}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativeGetSelectedText(
        JNIEnv *env, jobject jobject1) {
    // TODO: implement nativeGetSelectedText()
}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativeGetTextAfterCursor(
        JNIEnv *env, jobject jobject1, jint n) {
    // TODO: implement nativeGetTextAfterCursor()
}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativeGetTextBeforeCursor(
        JNIEnv *env, jobject jobject1, jint n) {
    // TODO: implement nativeGetTextBeforeCursor()
}
extern "C"
JNIEXPORT void JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativePerformContextMenuAction(
        JNIEnv *env, jobject jobject1, jint action) {
    // TODO: implement nativePerformContextMenuAction()
}
extern "C"
JNIEXPORT void JNICALL
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativePerformEditorAction(
        JNIEnv *env, jobject jobject1, jint action) {
    // TODO: implement nativePerformEditorAction()
}

#endif
