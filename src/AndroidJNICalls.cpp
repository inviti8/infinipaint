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

    void startTextInput(int input_type) {
        JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
        Logger::get().log("INFO", std::to_string((uint64_t)env) + " jni env value");
        jobject activity = (jobject)SDL_GetAndroidActivity();
        Logger::get().log("INFO", std::to_string((uint64_t)activity) + " activity value");
        jclass clazz = env->GetObjectClass(activity);
        jmethodID method_id = env->GetStaticMethodID(clazz, "startTextInput", "(I)V");
        Logger::get().log("INFO", std::to_string((uint64_t)method_id) + " method value");
        env->CallStaticVoidMethod(clazz, method_id, input_type);
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


// https://stackoverflow.com/questions/41820039/jstringjni-to-stdstringc-with-utf8-characters
std::string jstring2string(JNIEnv *env, jstring jStr) {
    if (!jStr)
        return "";

    const jclass stringClass = env->GetObjectClass(jStr);
    const jmethodID getBytes = env->GetMethodID(stringClass, "getBytes", "(Ljava/lang/String;)[B");
    const jbyteArray stringJbytes = (jbyteArray) env->CallObjectMethod(jStr, getBytes, env->NewStringUTF("UTF-8"));

    size_t length = (size_t) env->GetArrayLength(stringJbytes);
    jbyte* pBytes = env->GetByteArrayElements(stringJbytes, NULL);

    std::string ret = std::string((char *)pBytes, length);
    env->ReleaseByteArrayElements(stringJbytes, pBytes, JNI_ABORT);

    env->DeleteLocalRef(stringJbytes);
    env->DeleteLocalRef(stringClass);
    return ret;
}

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
Java_com_erroratline0_infinipaint_InfiniPaintTextBoxInputConnection_nativeSetTextBoxCursor(
        JNIEnv *env, jclass clazz, jint cursor_begin, jint cursor_end) {
    if(globalInputManager) {
        std::scoped_lock a{globalCallMutex};
        globalInputManager->main.input_android_text_box_set_cursor(cursor_begin, cursor_end);
    }
}

#endif