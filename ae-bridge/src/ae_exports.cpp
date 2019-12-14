/**
 * Mupen64PlusAE, an N64 emulator for the Android platform
 *
 * Copyright (C) 2013 Paul Lamb
 *
 * This file is part of Mupen64PlusAE.
 *
 * Mupen64PlusAE is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Mupen64PlusAE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with Mupen64PlusAE. If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Paul Lamb, littleguy77
 */

#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <jni.h>
#include "SDL.h"
#include "m64p_types.h"
#include "ae_vidext.h"

#ifdef M64P_BIG_ENDIAN
  #define sl(mot) mot
#else
  #define sl(mot) (((mot & 0xFF) << 24) | ((mot & 0xFF00) <<  8) | ((mot & 0xFF0000) >>  8) | ((mot & 0xFF000000) >> 24))
#endif


/*******************************************************************************
 Variables and functions used internally
 *******************************************************************************/

// JNI objects
static JavaVM* mVm;
static void* mReserved;

// Library handles
static void *handleCore;     // libmupen64plus-core.so

// Function types
typedef jint        (*pJNI_OnLoad)      (JavaVM* vm, void* reserved);
typedef int         (*pAeiInit)         (JNIEnv* env, jclass cls);
typedef int         (*pAeiDestroy)      (JNIEnv* env);
typedef m64p_error  (*pCoreShutdown)    (void);
typedef m64p_error  (*pCoreDoCommand)   (m64p_command, int, void *);

// Function pointers
static pAeiInit         aeiInit         = NULL;
static pAeiDestroy      aeiDestroy      = NULL;
static pCoreDoCommand   coreDoCommand   = NULL;
static pCoreShutdown    coreShutdown    = NULL;

void checkLibraryError(const char* message)
{
    const char* error = dlerror();
    if (error)
        LOGE("%s: %s", message, error);
}

void* loadLibrary(const char* libName)
{
    char path[256];
    sprintf(path, "lib%s.so", libName);
    void* handle = dlopen(path, RTLD_NOW);
    if (!handle)
        LOGE("Failed to load lib%s.so", libName);
    checkLibraryError(libName);

    return handle;
}

int unloadLibrary(void* handle, const char* libName)
{
    if (!handle)
        return 0;

    int code = dlclose(handle);
    if (code)
        LOGE("Failed to unload lib%s.so", libName);
    checkLibraryError(libName);
    return code;
}

void* locateFunction(void* handle, const char* libName, const char* funcName)
{
    char message[256];
    sprintf(message, "Locating %s::%s", libName, funcName);
    void* func = dlsym(handle, funcName);
    if (!func)
        LOGE("Failed to locate %s::%s", libName, funcName);
    checkLibraryError(message);
    return func;
}

/*******************************************************************************
 Functions called automatically by JNI framework
 *******************************************************************************/

extern jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    mVm = vm;
    mReserved = reserved;
    return JNI_VERSION_1_6;
}

/*******************************************************************************
 Functions called by Java code
 *******************************************************************************/

extern "C" DECLSPEC void SDLCALL Java_paulscode_android_mupen64plusae_jni_NativeExports_loadLibraries(JNIEnv* env, jclass cls)
{
    LOGI("Loading native libraries");

    // Clear stale error messages
    dlerror();

    // Open shared libraries
    handleCore     = loadLibrary("mupen64plus-core");

    // Make sure we don't have any typos
    if (!handleCore )
    {
        LOGE("Could not load libraries: be sure the paths are correct");
    }

    // Find library functions
    coreDoCommand = (pCoreDoCommand) locateFunction(handleCore,  "mupen64plus-core",       "CoreDoCommand");
    coreShutdown  = (pCoreShutdown)  locateFunction(handleCore,  "mupen64plus-core",       "CoreShutdown");

    // Make sure we don't have any typos
    if (!aeiInit || !aeiDestroy || !coreDoCommand || !coreShutdown)
    {
        LOGE("Could not load library functions: be sure they are named and typedef'd correctly");
    } else {
		// Initialize dependencies
		jclass nativeImports = env->FindClass("paulscode/android/mupen64plusae/jni/NativeImports");
		aeiInit(env, nativeImports);
	}
}

extern "C" DECLSPEC void SDLCALL Java_paulscode_android_mupen64plusae_jni_NativeExports_unloadLibraries(JNIEnv* env, jclass cls)
{
    // Unload the libraries to ensure that static variables are re-initialized next time
    LOGI("Unloading native libraries");
    // Clear stale error messages
    dlerror();

    aeiDestroy(env);

    // Nullify function pointers so that they can no longer be used
    aeiInit         = NULL;
    aeiDestroy      = NULL;
    coreDoCommand   = NULL;

    // Close shared libraries
    unloadLibrary(handleCore,     "mupen64plus-core");

    // Nullify handles so that they can no longer be used
    handleCore     = NULL;
}

extern "C" DECLSPEC void Java_paulscode_android_mupen64plusae_jni_NativeExports_emuStop(JNIEnv* env, jclass cls)
{
    if (coreDoCommand) coreDoCommand(M64CMD_STOP, 0, NULL);
}

extern "C" DECLSPEC void Java_paulscode_android_mupen64plusae_jni_NativeExports_emuShutdown(JNIEnv* env, jclass cls)
{
    if (coreShutdown) coreShutdown();
}

extern "C" DECLSPEC void Java_paulscode_android_mupen64plusae_jni_NativeExports_emuResume(JNIEnv* env, jclass cls)
{
    resumeEmulator();
    if (coreDoCommand) coreDoCommand(M64CMD_RESUME, 0, NULL);
}

extern "C" DECLSPEC void Java_paulscode_android_mupen64plusae_jni_NativeExports_emuPause(JNIEnv* env, jclass cls)
{
    pauseEmulator();
    if (coreDoCommand) coreDoCommand(M64CMD_PAUSE, 0, NULL);
}

extern "C" DECLSPEC void Java_paulscode_android_mupen64plusae_jni_NativeExports_emuAdvanceFrame(JNIEnv* env, jclass cls)
{
    resumeEmulator();
    if (coreDoCommand) coreDoCommand(M64CMD_ADVANCE_FRAME, 0, NULL);
}

extern "C" DECLSPEC void Java_paulscode_android_mupen64plusae_jni_NativeExports_emuSetSpeed(JNIEnv* env, jclass cls, jint percent)
{
    int speed_factor = (int) percent;
    if (coreDoCommand) coreDoCommand(M64CMD_CORE_STATE_SET, M64CORE_SPEED_FACTOR, &speed_factor);
}

extern "C" DECLSPEC void Java_paulscode_android_mupen64plusae_jni_NativeExports_emuSetFramelimiter(JNIEnv* env, jclass cls, jboolean enabled)
{
    int e = enabled == JNI_TRUE ? 1 : 0;
    if (coreDoCommand) coreDoCommand(M64CMD_CORE_STATE_SET, M64CORE_SPEED_LIMITER, &e);
}

extern "C" DECLSPEC void Java_paulscode_android_mupen64plusae_jni_NativeExports_emuSetSlot(JNIEnv* env, jclass cls, jint slotID)
{
    if (coreDoCommand) coreDoCommand(M64CMD_STATE_SET_SLOT, (int) slotID, NULL);
}

extern "C" DECLSPEC void Java_paulscode_android_mupen64plusae_jni_NativeExports_emuLoadSlot(JNIEnv* env, jclass cls)
{
    if (coreDoCommand) coreDoCommand(M64CMD_STATE_LOAD, 0, NULL);
}

extern "C" DECLSPEC void Java_paulscode_android_mupen64plusae_jni_NativeExports_emuSaveSlot(JNIEnv* env, jclass cls)
{
    if (coreDoCommand) coreDoCommand(M64CMD_STATE_SAVE, 1, NULL);
}

extern "C" DECLSPEC void Java_paulscode_android_mupen64plusae_jni_NativeExports_emuLoadFile(JNIEnv* env, jclass cls, jstring filename)
{
    const char *nativeString = env->GetStringUTFChars(filename, 0);
    if (coreDoCommand) coreDoCommand(M64CMD_STATE_LOAD, 0, (void *) nativeString);
    env->ReleaseStringUTFChars(filename, nativeString);
}

extern "C" DECLSPEC void Java_paulscode_android_mupen64plusae_jni_NativeExports_emuSaveFile(JNIEnv* env, jclass cls, jstring filename)
{
    const char *nativeString = env->GetStringUTFChars(filename, 0);
    if (coreDoCommand) coreDoCommand(M64CMD_STATE_SAVE, 1, (void *) nativeString);
    env->ReleaseStringUTFChars(filename, nativeString);
}

extern "C" DECLSPEC void Java_paulscode_android_mupen64plusae_jni_NativeExports_emuScreenshot(JNIEnv* env, jclass cls)
{
    if (coreDoCommand) coreDoCommand(M64CMD_TAKE_NEXT_SCREENSHOT, 0, NULL);
}

extern "C" DECLSPEC void Java_paulscode_android_mupen64plusae_jni_NativeExports_emuGameShark(JNIEnv* env, jclass cls, jboolean pressed)
{
    int p = pressed == JNI_TRUE ? 1 : 0;
    if (coreDoCommand) coreDoCommand(M64CMD_CORE_STATE_SET, M64CORE_INPUT_GAMESHARK, &p);
}

extern "C" DECLSPEC jint Java_paulscode_android_mupen64plusae_jni_NativeExports_emuGetState(JNIEnv* env, jclass cls)
{
    int state = 0;
    if (coreDoCommand) coreDoCommand(M64CMD_CORE_STATE_QUERY, M64CORE_EMU_STATE, &state);
    if (state == M64EMU_STOPPED)
        return (jint) 1;
    else if (state == M64EMU_RUNNING)
        return (jint) 2;
    else if (state == M64EMU_PAUSED)
        return (jint) 3;
    else
        return (jint) 0;
}

extern "C" DECLSPEC jint Java_paulscode_android_mupen64plusae_jni_NativeExports_emuGetSpeed(JNIEnv* env, jclass cls)
{
    int speed = 100;
    if (coreDoCommand) coreDoCommand(M64CMD_CORE_STATE_QUERY, M64CORE_SPEED_FACTOR, &speed);
    return (jint) speed;
}

extern "C" DECLSPEC jboolean Java_paulscode_android_mupen64plusae_jni_NativeExports_emuGetFramelimiter(JNIEnv* env, jclass cls)
{
    int e = 1;
    if (coreDoCommand) coreDoCommand(M64CMD_CORE_STATE_QUERY, M64CORE_SPEED_LIMITER, &e);
    return (jboolean) (e ? JNI_TRUE : JNI_FALSE);
}

extern "C" DECLSPEC jint Java_paulscode_android_mupen64plusae_jni_NativeExports_emuGetSlot(JNIEnv* env, jclass cls)
{
    int slot = 0;
    if (coreDoCommand) coreDoCommand(M64CMD_CORE_STATE_QUERY, M64CORE_SAVESTATE_SLOT, &slot);
    return (jint) slot;
}

extern "C" DECLSPEC void Java_paulscode_android_mupen64plusae_jni_NativeExports_emuReset(JNIEnv* env, jclass cls)
{
    if (coreDoCommand) coreDoCommand(M64CMD_RESET, 0, NULL);
}
