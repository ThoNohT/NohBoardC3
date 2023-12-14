#ifndef HOOKS_H_
#define HOOKS_H_

// Initialize hooks and start listening to keyboard and mouse events.
void hooks_initialize(Noh_Arena *arena, const char *kb_path, const char *mouse_path);

// Shutdown hooks and stop listening to keyboard and mouse events.
void hooks_shutdown();

// Retrieves a list of pressed keyboard keys. The result is the length of the list of keys, and the keys argument will
// point to the list of keys. The data will be allocated using the provided arena, it is the responsibility of the
// caller to free up the space used by this arena when it is no longer needed.
size_t hooks_get_pressed_kb_keys(Noh_Arena *arena, uint16 **keys);

// Retrieves a list of pressed mouse keys. The result is the length of the list of keys, and the keys argument will
// point to the list of keys. The data will be allocated using the provided arena, it is the responsibility of the
// caller to free up the space used by this arena when it is no longer needed.
size_t hooks_get_pressed_mouse_keys(Noh_Arena *arena, uint16 **keys);

#endif // HOOKS_H_
