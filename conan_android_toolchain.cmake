# Wrapper toolchain that delegates to the per-arch Conan-generated
# toolchain. Always pulls the Release variant regardless of the
# app's CMAKE_BUILD_TYPE — see android-project/app/build.gradle for
# rationale (one-build-type-of-natives pattern).

# During multiple stages of CMake configuration, the toolchain file is
# processed and command-line variables may not be always available. The
# script exits prematurely if essential variables are absent.
if(NOT ANDROID_ABI)
    return()
endif()

set(_NATIVE_BUILD_TYPE Release)  # pinned — only Release native libs are produced (see app/build.gradle)

if(${ANDROID_ABI} STREQUAL "x86_64")
    include("${CMAKE_CURRENT_LIST_DIR}/build/x86_64/${_NATIVE_BUILD_TYPE}/generators/conan_toolchain.cmake")
elseif(${ANDROID_ABI} STREQUAL "x86")
    include("${CMAKE_CURRENT_LIST_DIR}/build/x86/${_NATIVE_BUILD_TYPE}/generators/conan_toolchain.cmake")
elseif(${ANDROID_ABI} STREQUAL "arm64-v8a")
    include("${CMAKE_CURRENT_LIST_DIR}/build/armv8/${_NATIVE_BUILD_TYPE}/generators/conan_toolchain.cmake")
elseif(${ANDROID_ABI} STREQUAL "armeabi-v7a")
    include("${CMAKE_CURRENT_LIST_DIR}/build/armv7/${_NATIVE_BUILD_TYPE}/generators/conan_toolchain.cmake")
else()
    message(FATAL_ERROR "Unsupported ANDROID_ABI: ${ANDROID_ABI}")
endif()
