/*
 * uv_barrier shim for Electron builds.
 *
 * Electron's node.lib does not export uv_barrier_init / uv_barrier_wait /
 * uv_barrier_destroy even though the headers declare them.  Node.js's
 * node.lib DOES export them.
 *
 * Because uv.h marks these __declspec(dllimport), callers reference
 * __imp_uv_barrier_*.  We provide those as function-pointer variables
 * under alternate names, and /ALTERNATENAME tells the linker to use
 * ours only when node.lib does not supply the real ones.
 *
 * Only compiled on Windows; other platforms link libuv directly.
 */

#ifdef _WIN32

#include <uv.h>
#include <windows.h>

static int njsShim_uv_barrier_init(uv_barrier_t* barrier,
                                    unsigned int count) {
    int err;
    if (count == 0)
        return UV_EINVAL;
    barrier->threshold = count;
    barrier->in = 0;
    barrier->out = 0;
    err = uv_mutex_init(&barrier->mutex);
    if (err)
        return err;
    InitializeConditionVariable(&barrier->cond);
    return 0;
}

static int njsShim_uv_barrier_wait(uv_barrier_t* barrier) {
    int serial;

    uv_mutex_lock(&barrier->mutex);

    while (barrier->out > 0)
        SleepConditionVariableCS(&barrier->cond,
                                (PCRITICAL_SECTION) &barrier->mutex,
                                INFINITE);

    if (++barrier->in == barrier->threshold) {
        barrier->in = 0;
        barrier->out = barrier->threshold;
        WakeAllConditionVariable(&barrier->cond);
        uv_mutex_unlock(&barrier->mutex);
        return 1;
    }

    while (barrier->in > 0)
        SleepConditionVariableCS(&barrier->cond,
                                (PCRITICAL_SECTION) &barrier->mutex,
                                INFINITE);

    serial = (--barrier->out == 0);
    if (serial)
        WakeAllConditionVariable(&barrier->cond);

    uv_mutex_unlock(&barrier->mutex);
    return serial;
}

static void njsShim_uv_barrier_destroy(uv_barrier_t* barrier) {
    uv_mutex_destroy(&barrier->mutex);
}

/* __imp_ function-pointer variables under alternate names */
int  (*njsFallback__imp_uv_barrier_init)(uv_barrier_t*, unsigned int)
    = njsShim_uv_barrier_init;
int  (*njsFallback__imp_uv_barrier_wait)(uv_barrier_t*)
    = njsShim_uv_barrier_wait;
void (*njsFallback__imp_uv_barrier_destroy)(uv_barrier_t*)
    = njsShim_uv_barrier_destroy;

#pragma comment(linker, "/alternatename:__imp_uv_barrier_init=njsFallback__imp_uv_barrier_init")
#pragma comment(linker, "/alternatename:__imp_uv_barrier_wait=njsFallback__imp_uv_barrier_wait")
#pragma comment(linker, "/alternatename:__imp_uv_barrier_destroy=njsFallback__imp_uv_barrier_destroy")

#endif /* _WIN32 */
