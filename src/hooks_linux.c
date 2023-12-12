#include <unistd.h>
#include <stdint.h>
#include <semaphore.h>
#include <pthread.h>
#include <linux/input.h>

#include "noh.h"

static bool running = false;
static const char *hl_kb_path = NULL;
static const char *hl_mouse_path = NULL;

static sem_t cleanup_sem;
static pthread_t cleanup_thread;

static pthread_t run_thread;

// An input event from a /dev/input file stream.
typedef struct {
    struct timeval time;
    uint16 type;
    uint16 code;
    uint value;
} Input_Event;

/// Represents a list of pressed keys.
typedef struct {
    uint16 *elems;
    size_t count;
    size_t capacity;
} Pressed_Keys;

Pressed_Keys pressed_kb_keys = {0};
pthread_mutex_t pressed_kb_keys_mutex = PTHREAD_MUTEX_INITIALIZER;
Pressed_Keys pressed_mouse_keys = {0};
pthread_mutex_t pressed_mouse_keys_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct timespec get_delay(int seconds) {
    struct timespec timeout;
    if (clock_gettime(CLOCK_REALTIME, &timeout) == -1)
    {
        noh_log(NOH_ERROR, "Unable to determine semaphore timeout: %s", strerror(errno));
        exit(1);
    }
    timeout.tv_sec += seconds;
    return timeout;
}

static void *run() {
    FILE *fd = fopen(hl_kb_path, "rb");

    Input_Event event = {0};
    while (running) {
        size_t read = fread(&event, sizeof(event), 1, fd);
        if (read == 1) {
            if (event.type != EV_KEY) continue;
            if (event.value == 1) {
                pthread_mutex_lock(&pressed_kb_keys_mutex);

                // Check if key is already in the map.
                bool in_map = false;
                for (size_t i = 0; i < pressed_kb_keys.count; i++) {
                    if (pressed_kb_keys.elems[i] == event.code) in_map = true;
                }

                // If it is not in the map, add it.
                if (!in_map) noh_da_append(&pressed_kb_keys, event.code);

                pthread_mutex_unlock(&pressed_kb_keys_mutex);
            } else if (event.value == 0) {
                pthread_mutex_lock(&pressed_kb_keys_mutex);

                // Check if key is in the map.
                int in_map = -1;
                for (int i = 0; (size_t)i < pressed_kb_keys.count; i++) {
                    if (pressed_kb_keys.elems[i] == event.code) in_map = i;
                }

                // If it is in the map, remove it.
                if (in_map >= 0) noh_da_remove_at(&pressed_kb_keys, (size_t)in_map);

                pthread_mutex_unlock(&pressed_kb_keys_mutex);

            } // Ignore other values, since we only care about down=1 and up=o.
        } else {
            noh_log(NOH_ERROR, "Expected to read %zu bytes, but got %zu", sizeof(event), read);
        }
    }

    noh_log(NOH_INFO, "Run shutdown.");

    if (fclose(fd) != 0) {
        noh_log(NOH_ERROR, "Failed closing keyboard file: %s", strerror(errno));
    }

    pthread_exit(NULL);
}

static void* cleanup() {
    while (running) {
        struct timespec timeout = get_delay(2);

        noh_log(NOH_INFO, "In cleanup thread...");

        // Wait for next run, or stop if signalled earlier.
        int res = sem_timedwait(&cleanup_sem, &timeout);
        if (res < 0) {
            if (errno == ETIMEDOUT) continue;

            noh_log(NOH_ERROR, "Unable to wait for semaphore: %s", strerror(errno));
            running = false;
        }
    }

    noh_log(NOH_INFO, "Cleanup shutdown.");
    sem_destroy(&cleanup_sem);

    pthread_exit(NULL);
}

void hooks_shutdown() {
    running = false;
    sem_post(&cleanup_sem);
    pthread_join(cleanup_thread, NULL);
    pthread_join(run_thread, NULL);
}

void hooks_initialize(const char *kb_path, const char *mouse_path) {
    noh_log(NOH_INFO, "Initializing hooks.");
    noh_log(NOH_INFO, "KB_PATH: %s", hl_kb_path);
    hl_kb_path = kb_path;
    hl_mouse_path = mouse_path;

    running = true;

    pthread_create(&run_thread, NULL, run, NULL);
    sem_init(&cleanup_sem, 0, 0);
    pthread_create(&cleanup_thread, NULL, cleanup, NULL);
}

size_t hooks_get_pressed_kb_keys(Noh_Arena *arena, uint16 **keys) {
    pthread_mutex_lock(&pressed_kb_keys_mutex);
    size_t count = pressed_kb_keys.count;

    size_t size = sizeof(uint16) * count;
    *keys = noh_arena_alloc(arena, size);
    memcpy(*keys, pressed_kb_keys.elems, size);

    pthread_mutex_unlock(&pressed_kb_keys_mutex);

    return count;
}

size_t hooks_get_pressed_mouse_keys(Noh_Arena *arena, uint16 **keys) {
    pthread_mutex_lock(&pressed_mouse_keys_mutex);
    size_t count = pressed_mouse_keys.count;

    size_t size = sizeof(uint16) * count;
    *keys = noh_arena_alloc(arena, size);
    memcpy(*keys, pressed_mouse_keys.elems, size);

    pthread_mutex_unlock(&pressed_mouse_keys_mutex);

    return count;
}
