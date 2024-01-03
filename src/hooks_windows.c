// Also includes noh.h
#include "hooks.c" // Common code used by all platforms.

// This arena is used all throughout this file for storing temporary data.
static Noh_Arena hooks_arena = {0};

// Lookup a device by the device id, returns null if not found.
NB_Input_Device *hooks_find_device_by_id(char *device_id) {
    (void)device_id;
    noh_assert(false && "Not yet implemented.");
    return (NB_Input_Device *)NULL;
}

bool hooks_initialize() {
    noh_assert(false && "Not yet implemented.");
    return true;
}

bool hooks_reinitialize() {
    hooks_shutdown();
    noh_arena_reset(&hooks_arena);
    return hooks_initialize();
}

NB_Input_State hooks_get_state(Noh_Arena *arena) {
    (void)arena;
    noh_assert(false && "Not yet implemented.");
    NB_Input_State state = {0};
    return state;
}

void hooks_shutdown() {
    noh_assert(false && "Not yet implemented.");
}
