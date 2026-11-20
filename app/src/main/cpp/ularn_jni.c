/* JNI bindings for com.ularn.android.NativeBridge */
#include <jni.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "spawn.h"
#include "shared_state.h"

/* JNIEXPORT already marks these default-visibility despite -fvisibility=hidden. */

static char **to_cstr_array(JNIEnv *env, jobjectArray arr, int *count)
{
    int n = arr ? (*env)->GetArrayLength(env, arr) : 0, i;
    char **out = calloc((size_t)n + 1, sizeof(char *));
    for (i = 0; i < n; i++) {
        jstring s = (jstring)(*env)->GetObjectArrayElement(env, arr, i);
        const char *c = (*env)->GetStringUTFChars(env, s, NULL);
        out[i] = strdup(c);           /* must outlive fork (putenv/argv) */
        (*env)->ReleaseStringUTFChars(env, s, c);
        (*env)->DeleteLocalRef(env, s);
    }
    if (count) *count = n;
    return out;
}

static void free_cstr_array(char **a)
{
    char **p;
    if (!a) return;
    for (p = a; *p; p++) free(*p);
    free(a);
}

/* Returns [status(0 or -errno), pid, inputFd, notifyFd]. */
JNIEXPORT jintArray JNICALL
Java_com_ularn_android_NativeBridge_spawn(JNIEnv *env, jclass cls,
        jstring jworkdir, jobjectArray jargs, jobjectArray jenv)
{
    const char *wd = (*env)->GetStringUTFChars(env, jworkdir, NULL);
    char *workdir = strdup(wd);
    int argc = 0, rc;
    char **argv, **envp;
    UlarnSpawn sp;
    jint result[4];
    jintArray out;
    (void)cls;

    (*env)->ReleaseStringUTFChars(env, jworkdir, wd);
    argv = to_cstr_array(env, jargs, &argc);
    envp = to_cstr_array(env, jenv, NULL);

    memset(&sp, 0, sizeof sp);
    rc = ularn_spawn(workdir, argc, argv, envp, &sp);

    free(workdir);
    free_cstr_array(argv);
    free_cstr_array(envp);

    result[0] = rc;
    result[1] = rc == 0 ? (jint) sp.pid : -1;
    result[2] = rc == 0 ? sp.input_fd : -1;
    result[3] = rc == 0 ? sp.notify_fd : -1;
    out = (*env)->NewIntArray(env, 4);
    (*env)->SetIntArrayRegion(env, out, 0, 4, result);
    return out;
}

JNIEXPORT jobject JNICALL
Java_com_ularn_android_NativeBridge_sharedState(JNIEnv *env, jclass cls)
{
    UlarnShared *s = ularn_shared_region();
    (void)cls;
    if (!s) return NULL;
    return (*env)->NewDirectByteBuffer(env, s, (jlong) sizeof(UlarnShared));
}

/* Field offsets, in the order of GameState.Field in Kotlin. */
JNIEXPORT jintArray JNICALL
Java_com_ularn_android_NativeBridge_layout(JNIEnv *env, jclass cls)
{
    jint v[] = {
        (jint) sizeof(UlarnShared),
        offsetof(UlarnShared, magic),
        offsetof(UlarnShared, seq),
        offsetof(UlarnShared, display_mode),
        offsetof(UlarnShared, input_mode),
        offsetof(UlarnShared, answers),
        offsetof(UlarnShared, caret),
        offsetof(UlarnShared, cursor_x),
        offsetof(UlarnShared, cursor_y),
        offsetof(UlarnShared, beep_count),
        offsetof(UlarnShared, player_x),
        offsetof(UlarnShared, player_y),
        offsetof(UlarnShared, player_cursor),
        offsetof(UlarnShared, tiles),
        offsetof(UlarnShared, overlay),
        offsetof(UlarnShared, status1),
        offsetof(UlarnShared, status2),
        offsetof(UlarnShared, hp),
        offsetof(UlarnShared, hpmax),
        offsetof(UlarnShared, spells),
        offsetof(UlarnShared, spellmax),
        offsetof(UlarnShared, gold),
        offsetof(UlarnShared, plevel),
        offsetof(UlarnShared, ac),
        offsetof(UlarnShared, wc),
        offsetof(UlarnShared, exp),
        offsetof(UlarnShared, dlevel),
        offsetof(UlarnShared, level_name),
        offsetof(UlarnShared, effects),
        offsetof(UlarnShared, class_num),
        offsetof(UlarnShared, sex),
        offsetof(UlarnShared, stats),
        offsetof(UlarnShared, level_title),
        offsetof(UlarnShared, game_time),
        offsetof(UlarnShared, hardness),
        offsetof(UlarnShared, msg_chr),
        offsetof(UlarnShared, msg_fmt),
        offsetof(UlarnShared, text_chr),
        offsetof(UlarnShared, text_fmt),
        /* dimensions */
        US_MAP_W, US_MAP_H, US_LINE_LEN, US_MSG_LINES, US_TEXT_LINES,
        /* minimap / side panel */
        offsetof(UlarnShared, mm_cat), offsetof(UlarnShared, inv_text),
        US_INV_SLOTS, US_INV_LEN,
        /* context action / spells */
        offsetof(UlarnShared, ctx_object),
        offsetof(UlarnShared, ctx_kind),
        offsetof(UlarnShared, ctx_flags),
        offsetof(UlarnShared, ctx_doors),
        offsetof(UlarnShared, ctx_name),
        offsetof(UlarnShared, spell_count),
        offsetof(UlarnShared, spell_known),
        offsetof(UlarnShared, spell_codes),
        US_CTX_NAME, US_SPELLS,
        /* danger cues / stats */
        offsetof(UlarnShared, run_id),
        offsetof(UlarnShared, kills),
        offsetof(UlarnShared, end_state),
        offsetof(UlarnShared, end_score),
        offsetof(UlarnShared, end_reason),
        /* Wild Edition */
        offsetof(UlarnShared, wild_flags),
        offsetof(UlarnShared, hat_tile),
        offsetof(UlarnShared, glitch),
        offsetof(UlarnShared, weather),
        offsetof(UlarnShared, coin_price),
        offsetof(UlarnShared, pet_x),
        offsetof(UlarnShared, pet_y),
        offsetof(UlarnShared, kill_counts),
        US_KILL_COUNTS,
        offsetof(UlarnShared, ev_seq),
        offsetof(UlarnShared, ev),
        US_EVENTS,
        offsetof(UlarnShared, pet_info),
        offsetof(UlarnShared, pet_level),
        offsetof(UlarnShared, bounty_info),
    };
    jintArray out;
    (void)cls;
    out = (*env)->NewIntArray(env, (jsize)(sizeof v / sizeof v[0]));
    (*env)->SetIntArrayRegion(env, out, 0, (jsize)(sizeof v / sizeof v[0]), v);
    return out;
}

JNIEXPORT jint JNICALL
Java_com_ularn_android_NativeBridge_waitFor(JNIEnv *env, jclass cls, jint pid)
{
    (void)env; (void)cls;
    return ularn_wait((pid_t)pid);
}

JNIEXPORT void JNICALL
Java_com_ularn_android_NativeBridge_sendSignal(JNIEnv *env, jclass cls,
        jint pid, jint sig)
{
    (void)env; (void)cls;
    if (pid > 0) kill((pid_t)pid, sig);
}
