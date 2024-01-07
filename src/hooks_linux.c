#include <unistd.h>
#include <stdint.h>
#include <semaphore.h>
#include <pthread.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <fcntl.h>
#include <poll.h>
#include <dirent.h>

// Also includes noh.h
#include "hooks.c" // Common code used by all platforms.

static bool running = false;

// This arena is used all throughout this file for storing temporary data.
static Noh_Arena hooks_arena = {0};

NB_Input_Devices hooks_devices = {0};
struct pollfd *poll_fds;

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

NBI_Input_State input_state = {0};
pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;

static int load_capability_map(Noh_Arena *arena, NB_Input_Device *dev, uint8 **capabilities) {
    static size_t len = (EV_MAX / sizeof(uint8) + 1) * sizeof(uint8);

    // Allocate some zeroed memory.
    *capabilities = noh_arena_alloc(arena, len);
    memset(*capabilities, 0, len);

    // Load the keymap from the device.
    if (ioctl(dev->fd, EVIOCGBIT(0, EV_MAX), *capabilities) < 0) {
        noh_log(NOH_WARNING, "Could not determine capabilities of device %s.", dev->name);
        return -1;
    }

    // Return the length.
    return len;
}

static int load_keymap(Noh_Arena *arena, NB_Input_Device *dev, uint8 **keys) {
    static size_t len = (KEY_MAX / sizeof(uint8) + 1) * sizeof(uint8);

    // Allocate some zeroed memory.
    *keys = noh_arena_alloc(arena, len);
    memset(*keys, 0, len);

    // Load the keymap from the device.
    if (ioctl(dev->fd, EVIOCGKEY(len), *keys) < 0) {
        noh_log(NOH_WARNING, "Could not determine key map of device %s.", dev->name);
        return -1;
    }

    // Return the length;
    return len;
}

static int load_abs_map(Noh_Arena *arena, NB_Input_Device *dev, uint8 **abs_map) {
    static size_t len = (ABS_MAX / sizeof(uint8) + 1) * sizeof(uint8);

    // Allocate some zeroed memory.
    *abs_map = noh_arena_alloc(arena, len);
    memset(*abs_map, 0, len);

    // Load the absolute map from the device.
    if (ioctl(dev->fd, EVIOCGBIT(EV_ABS, len), *abs_map) < 0) {
        noh_log(NOH_WARNING, "Could not determine absolute map of device %s.", dev->name);
        return -1;
    }

    // Return the length.
    return len;
}

static int load_rel_map(Noh_Arena *arena, NB_Input_Device *dev, uint8 **rel_map) {
    static size_t len = (REL_MAX / sizeof(uint8) + 1) * sizeof(uint8);

    // Allocate some zeroed memory.
    *rel_map = noh_arena_alloc(arena, len);
    memset(*rel_map, 0, len);

    // Load the relative map from the device.
    if (ioctl(dev->fd, EVIOCGBIT(EV_REL, len), *rel_map) < 0) {
        noh_log(NOH_WARNING, "Could not determine relative map of device %s.", dev->name);
        return -1;
    }

    // Return the length.
    return len;
}

static int load_axis_info(NB_Input_Device *dev, size_t axis_id, struct input_absinfo *abs_feat) {
    if (ioctl(dev->fd, EVIOCGABS(axis_id), abs_feat)) {
        noh_log(NOH_WARNING, "Unable to get info about axis %zu of device %s.", axis_id, dev->name);
        return -1;
    }

    return 0;
}

static bool test_bit(uint8 *keymap, size_t keymap_len, uint16 key) {
    if (key / 8 > keymap_len) return false;

    uint8 index = key / 8;
    uint8 offset = key % 8;
    return (keymap[index] & (1 << offset)) > 0;
}

static void *run() {
    Input_Event event = {0};
    NB_Input_Devices *devices = &hooks_devices;

    while (running) {
        int poll_result = poll(poll_fds, devices->count, 500);
        if (poll_result == -1) {
            noh_log(NOH_ERROR, "Failed to poll input files: %s", strerror(errno));
            goto defer;
        }

        // 0 result means a timeout, so check if we should still be running.
        if (poll_result == 0) continue;

        for (size_t i = 0; i < devices->count; i++) {
            // Find the device and the poll_fd for this index.
            NB_Input_Device *dev = &devices->elems[i];
            struct pollfd *poll_fd = &poll_fds[i];

            if (poll_fd->revents & POLLIN) {
                int bytes_read = read(poll_fd->fd, &event, sizeof(event));
                if (bytes_read < 0) {
                    noh_log(NOH_ERROR, "error: %s", strerror(errno));
                    continue;
                } else if (bytes_read != sizeof(event)) {
                    noh_log(NOH_ERROR, "Expected to read %zu bytes, but got %zu", sizeof(event), read);
                    continue;
                }

                // We got a valid event, handle it.
                switch(event.type) {
                    case EV_KEY:
                        // Only check up and down events.
                        if (event.value != 0 && event.value != 1) break;

                        if (devices->default_kb_idx == -1 && 
                            event.code >= KEY_ESC && event.code <= KEY_COMPOSE) {
                            // Mark this device as default keyboard.
                            devices->default_kb_idx = i;
                        }

                        if (devices->default_mouse_idx == -1 &&
                            (event.code == BTN_LEFT || event.code == BTN_RIGHT || event.code == BTN_MIDDLE)) {
                            // Mark this device as default mouse.
                            devices->default_mouse_idx = i;
                        }

                        pthread_mutex_lock(&input_mutex);
                        hooks_add_key(&input_state, dev->index, event.code, event.value == 1); 
                        pthread_mutex_unlock(&input_mutex);
                        break;
                    case EV_ABS:
                        {
                            struct timespec tim = noh_get_time_in(0, 0);
                            pthread_mutex_lock(&input_mutex);
                            hooks_add_abs_value(&input_state, dev->index, event.code, &tim, event.value);
                            pthread_mutex_unlock(&input_mutex);
                            break;
                        }
                    case EV_REL:
                        if (devices->default_mouse_idx == -1) {
                            // Mark this device as default mouse.
                            devices->default_mouse_idx = i;
                        }

                        struct timespec time = noh_get_time_in(0, 0);
                        pthread_mutex_lock(&input_mutex);
                        hooks_add_rel_value(&input_state, dev->index, event.code, &time, event.value);
                        pthread_mutex_unlock(&input_mutex);
                        break;
                    default:
                       break;
                }

            } else if (poll_fd->revents & (POLLERR | POLLHUP)) {
                // We got a signal that the file descriptor is no longer valid, and we need to close it.
                // The device will still be listed until the hooks are reset, but no input will come from it anymore.
                noh_log(NOH_WARNING, "closing fd %d\n", poll_fd->fd);
                close(poll_fd->fd);
                poll_fd->events *= -1;
            }
        }
    }

defer:
    noh_log(NOH_INFO, "Run shutdown.");

    for (size_t i = 0; i < devices->count; i++) {
        NB_Input_Device *dev = &devices->elems[i];
        if (close(dev->fd) != 0) {
            noh_log(NOH_WARNING, "Failed closing input device %s: %s", dev->name, strerror(errno));
        }
    }

    pthread_exit(NULL);
}

// Find the device at the specified index, returns NULL if out of bounds.
NB_Input_Device *hooks_find_device_by_index(size_t device_index) {
    if (device_index >= hooks_devices.count) return NULL;
    return &hooks_devices.elems[device_index];
}


#define CLEANUP_INTERVAL 1000
#define SMOOTH_INTERVAL 100

static void* cleanup() {
    Noh_Arena cleanup_arena = noh_arena_init(2 KB);

    struct timespec last_cleanup = noh_get_time_in(0, 0);

    while (running) {
        struct timespec time = noh_get_time_in(0, 0);
        struct timespec timeout = time;
        noh_time_add(&timeout, 0, SMOOTH_INTERVAL);

        // Fill relative and absolute histories with zeroes, so they tend back to 0.
        for (size_t i = 0; i < input_state.axes.count; i++) {
            NBI_Axis_History *history = &input_state.axes.elems[i];
            // Add a 0 if the last update was at least half a second before.
            if (noh_diff_timespec_ms(&history->last_updated_at, &time) > -SMOOTH_INTERVAL) continue;

            // Fill in relative even for absolute, so no absolute value is overwritten.
            hooks_add_rel_value_(history, &time, 0);
        }

        if (noh_diff_timespec_ms(&time, &last_cleanup) < CLEANUP_INTERVAL) continue;
        last_cleanup = noh_get_time_in(0, 0);

        // Cleanup pressed keys using load_keymap.
        noh_arena_save(&cleanup_arena);
        pthread_mutex_lock(&input_mutex);
        for (size_t i = 0; i < input_state.pressed_keys.count; i++) {
            NBI_Pressed_Keys_List *list = &input_state.pressed_keys.elems[i];
            if (list->count <= 0) continue; // No pressed keys to cleanup.

            // There are pressed keys, get a new keymap.
            NB_Input_Device *dev = hooks_find_device_by_index(list->device_index);
            if (dev == NULL) continue; // Could not find device.

            uint8 *currently_pressed;
            size_t keymap_len = load_keymap(&cleanup_arena, dev, &currently_pressed);
            if (keymap_len < 0) continue; // Could not load keymap.
            if (keymap_len == 0) {
                noh_da_reset(list); // Remove all keys
            } else {
                // Some keys are still pressed, check all of them against the loaded keymap.
                // Remove keys from the back forward so we don't mess with the indexes of keys still to check.
                // Use a long and not size_t for j, since we need it to be able to go below 0 to exit the loop.
                for (long j = list->count - 1; j >= 0; j--) {
                    if (!test_bit(currently_pressed, keymap_len, list->elems[j])) {
                        noh_da_remove_at(list, (size_t)j);
                    }
                }
            }
        }

        pthread_mutex_unlock(&input_mutex);
        noh_arena_rewind(&cleanup_arena);

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

// Helper to fill in the list of known devices.
// Uses the provided arena to fill up all data needed in the devices parameter. Only free up when this parameter is no
// longer needed.
static bool init_devices(Noh_Arena *arena, NB_Input_Devices *devices) {
    noh_assert(devices);
    devices->default_kb_idx = -1;
    devices->default_mouse_idx = -1;

    #define INPUT_BASE_PATH "/dev/input"
    noh_arena_reserve(arena, 10 KB);

    DIR *input_dir;
    struct dirent *dir;
    input_dir = opendir(INPUT_BASE_PATH);
    if (!input_dir) {
        noh_log(NOH_ERROR, "Could not load input files directory: %s", strerror(errno));
        return false;
    }

    Noh_String device_path = {0};
    struct stat statbuf;

    while ((dir = readdir(input_dir)) != NULL) {
        Noh_String_View dir_sv = noh_sv_from_cstr(dir->d_name);
        if (noh_sv_eq(dir_sv, noh_sv_from_cstr("."))) continue;
        if (noh_sv_eq(dir_sv, noh_sv_from_cstr(".."))) continue;
        if (!noh_sv_starts_with(dir_sv, noh_sv_from_cstr("event"))) continue;

        // Build up full path to device.
        noh_string_reset(&device_path);
        noh_string_append_cstr(&device_path, INPUT_BASE_PATH);
        noh_string_append_cstr(&device_path, "/");
        noh_string_append_cstr(&device_path, dir->d_name);
        noh_string_append_null(&device_path);

        // Check that it is not a directory.
        if (stat(device_path.elems, &statbuf) == -1) continue;
        if (S_ISDIR(statbuf.st_mode)) continue;

        int fd;
        if ((fd = open(device_path.elems, O_RDONLY | O_NONBLOCK)) < 0) {
            continue;
        }

        // Determine the device name.
        char *name = noh_arena_alloc(arena, 256);
        if(ioctl(fd, EVIOCGNAME(256), name) < 0) {
            noh_log(NOH_WARNING, "Could not get device name for device %s.", device_path.elems);
            continue;
        }

        // Determine the physical device path.
        char *phys = noh_arena_alloc(arena, 256);
        if(ioctl(fd, EVIOCGPHYS(256), phys) < 0) {
            noh_log(NOH_WARNING, "Could not get physical path for device %s.", device_path.elems);
            continue;
        }

        NB_Input_Device device = {0};
        device.type = NB_Unknown;
        device.fd = fd;
        device.path = noh_arena_strdup(arena, device_path.elems);
        device.name = name;
        device.physical_path = phys;

        // Try to determine the device type based on the name.
        // Later we can use key and relative events to add another way to determine.
        // FUTURE: Is there not a better way to find this out?
        if (noh_sv_contains_ci(noh_sv_from_cstr(device.name), noh_sv_from_cstr("mouse")))
            device.type = NB_Mouse;
        if (noh_sv_contains_ci(noh_sv_from_cstr(device.name), noh_sv_from_cstr("keyboard")))
            device.type = NB_Keyboard;
        if (noh_sv_contains_ci(noh_sv_from_cstr(device.name), noh_sv_from_cstr("touchpad")))
            device.type = NB_Touchpad;

        device.index = devices->count; // The current count will be the index of this device.
        noh_da_append(devices, device);
    }

    closedir(input_dir);
    noh_string_free(&device_path);

    return 1;
}

// Helper to fill in the currently pressed keys for all inputs that have keys.
// Any data used in the arena will be rewound at the end of this function.
// Strings used in the returned NBI_Input_State will share their memory with the provided NB_Input_Devices.
static NBI_Input_State fill_current_state(Noh_Arena *arena, NB_Input_Devices *devices) {
    NBI_Input_State state = {0};

    // Fill in the keys that are pressed when starting.
    for (size_t i = 0; i < devices->count; i++) {
        NB_Input_Device *dev = &devices->elems[i];

        // Check the device's capabilities.
        uint8 *capabilities;
        noh_arena_save(arena);
        int cap_len = load_capability_map(arena, dev, &capabilities);
        if (cap_len < 0) continue;

        // Fill in pressed keys if keys are supported.
        if (test_bit(capabilities, cap_len, EV_KEY)) {
            NBI_Pressed_Keys_List *list = hooks_define_key_list(&state, dev);
            uint8 *pressed_at_start;
            size_t keymap_len = load_keymap(arena, dev, &pressed_at_start);
            if (keymap_len > 0) {
                for (uint16 key = 1; key < KEY_MAX; key++) {
                    if (test_bit(pressed_at_start, keymap_len, key)) {
                        hooks_add_key_(list, key, true);
                    }
                }
            }
        }

        struct timespec time = noh_get_time_in(0, 0);

        // Fill in absolute axes if abs is supported.
        if (test_bit(capabilities, cap_len, EV_ABS)) {
            struct input_absinfo abs_feat;
            uint8 *abs_map;
            noh_arena_save(arena);

            int abs_len = load_abs_map(arena, dev, &abs_map);
            if (abs_len > 0) {
                for (uint16 axis_id = 0; axis_id < ABS_MAX; axis_id++) {
                    if (test_bit(abs_map, abs_len, axis_id)) {
                        // This is a supported axis, get its state.
                        if (load_axis_info(dev, axis_id, &abs_feat) >= 0) {
                            // FUTURE: What are abs_feat.flat and abs_feat.fuzz?
                            hooks_define_abs_axis(&state, dev, axis_id, &time, abs_feat.value, abs_feat.minimum, abs_feat.maximum);
                        }
                    }
                }
            }

            noh_arena_rewind(arena);
        }

        // Fill in relative axes if rel is supported.
        if (test_bit(capabilities, cap_len, EV_REL)) {
            uint8 *rel_map;
            noh_arena_save(arena);

            int rel_len = load_rel_map(arena, dev, &rel_map);
            if (rel_len > 0) {
                for (uint16 axis_id = 0; axis_id < REL_MAX; axis_id++) {
                    if (test_bit(rel_map, rel_len, axis_id)) {
                        hooks_define_rel_axis(&state, dev, axis_id, &time);
                    }
                }
            }

            noh_arena_rewind(arena);
        }
    }

    noh_arena_rewind(arena);

    return state;
}

// Creates a polfd struct with POLLIN for all the file descriptors of all devices.
static struct pollfd *create_poll_fds(Noh_Arena *arena, NB_Input_Devices *devices) {
    struct pollfd *poll_fds = noh_arena_alloc(arena, sizeof(struct pollfd) * devices->count);
    for (size_t i = 0; i < devices->count; i++) {
        NB_Input_Device *dev = &devices->elems[i];
        struct pollfd poll_fd = { .fd = dev->fd, .events = POLLIN };
        poll_fds[i] = poll_fd;
    }

    return poll_fds;
}

bool hooks_initialize() {
    noh_log(NOH_INFO, "Initializing hooks.");

    if (hooks_arena.blocks.count > 0) {
        noh_arena_reset(&hooks_arena);
    } else {
        hooks_arena = noh_arena_init(20 KB);
    }

    // (Re)initialize the devices.
    if (!init_devices(&hooks_arena, &hooks_devices)) {
        return false;
    }
    // The hooks arena now contains information about all devices.

    // Reset the input state to only the currently pressed values and axis offsets.
    noh_arena_save(&hooks_arena);
    pthread_mutex_lock(&input_mutex);
    input_state = fill_current_state(&hooks_arena, &hooks_devices);
    pthread_mutex_unlock(&input_mutex);
    noh_arena_rewind(&hooks_arena);

    poll_fds = create_poll_fds(&hooks_arena, &hooks_devices);
    // The hooks arena now also contains the poll_fds.

    // Start running.
    running = true;
    pthread_create(&run_thread, NULL, run, NULL);

    // Start cleanup.
    sem_init(&cleanup_sem, 0, 0);
    pthread_create(&cleanup_thread, NULL, cleanup, NULL);

    return true;
}

bool hooks_reinitialize() {
    hooks_shutdown();
    noh_arena_reset(&hooks_arena);
    return hooks_initialize();
}

NB_Input_State hooks_get_state(Noh_Arena *arena) {
    pthread_mutex_lock(&input_mutex);
    NB_Input_State result = copy_nbi_state_to_nb_state(arena, &input_state);
    pthread_mutex_unlock(&input_mutex);
    return result;
}
