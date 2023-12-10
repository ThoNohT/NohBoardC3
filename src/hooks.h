#ifndef HOOKS_H_
#define HOOKS_H_

// Initialize hooks and start listening to keyboard and mouse events.
void hooks_initialize(const char *kb_path, const char *mouse_path);

// Shutdown hooks and stop listening to keyboard and mouse events.
void hooks_shutdown();

#endif // HOOKS_H_
