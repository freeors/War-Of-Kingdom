/*
    SDL_android_main.c, placed in the public domain by Sam Lantinga  3/13/14
*/
#include "../../SDL_internal.h"

#ifdef __ANDROID__

/* Include the SDL main definition header */
#include "SDL_main.h"

/*******************************************************************************
                 Functions called by JNI
*******************************************************************************/
#include <jni.h>
#include <android/log.h>

/* Called before SDL_main() to initialize JNI bindings in SDL library */
extern void SDL_Android_Init(JNIEnv* env, jclass cls);

/* Start up the SDL app */
// void Java_com_freeors_kingdom_SDLActivity_nativeInit(JNIEnv* env, jclass cls, jobject obj)
void Java_com_freeors_kingdom_SDLActivity_nativeInit(JNIEnv* env, jclass cls, jstring res_dir)
{
	const char* utf8;
	// release the Java string and UTF-8
	char* utf8_back;
	utf8 = (*env)->GetStringUTFChars(env, res_dir, NULL);
	utf8_back = strdup(utf8);
	(*env)->ReleaseStringUTFChars(env, res_dir, utf8);

	__android_log_print(ANDROID_LOG_VERBOSE, "SDL", "SDLActivity_nativeInit, res_dir: (%u)%s", strlen(utf8_back), utf8_back);

    /* This interface could expand with ABI negotiation, calbacks, etc. */
    SDL_Android_Init(env, cls);

    SDL_SetMainReady();

    /* Run the application code! */
    int status;
    char *argv[2];
    // argv[0] = SDL_strdup("SDL_app");
	argv[0] = utf8_back;
    argv[1] = NULL;
    status = SDL_main(1, argv);

	free(utf8_back);
	__android_log_print(ANDROID_LOG_INFO, "SDL", "SDLActivity_nativeInit, finished!");

    /* Do not issue an exit or the whole application will terminate instead of just the SDL thread */
    /* exit(status); */
}

#endif /* __ANDROID__ */

/* vi: set ts=4 sw=4 expandtab: */
