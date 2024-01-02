#ifndef HOOKS_H_
#define HOOKS_H_

#define NB_INPUT_SMOOTH 5

///////////////////////// Input devices /////////////////////////

// The different recognized types of devices.
typedef enum {
    NB_Unknown,
    NB_Keyboard,
    NB_Mouse,
    NB_Touchpad
} NB_Input_Device_Type;

// Information about a specific input device.
// FUTURE: This type is not cross platform.
typedef struct {
    NB_Input_Device_Type type;

    int fd;
    char *path;
    char *name;
    char *physical_path;
} NB_Input_Device;

// A list of input devices.
typedef struct {
    NB_Input_Device *elems;
    size_t count;
    size_t capacity;

    // This will be detected, as the first device that got a typical keyboard key event.
    // Is -1 as long as no default has been detected.
    int default_kb_idx;
    // This will be detected, as the first device that got a typical mouse key event or relative movement.
    // Is -1 as long as no default has been detected.
    int default_mouse_idx;
} NB_Input_Devices;

///////////////////////// Keyboard state /////////////////////////

// The pressed keys of a specific device.
typedef struct {
    uint16 *elems; // The key codes of the currently pressed keys.
    size_t count; // The number of elements in elems.

    char *device_id; // A unique identifier for the device for which the pressed keys are recorded. 
} NB_Pressed_Keys_List;

// A list of lists of pressed keys, one per device. Every device has only one list of pressed keys.
typedef struct {
    NB_Pressed_Keys_List *elems; // The list of all known pressed keys lists.
    size_t count; // The number of elements in elems.
} NB_Pressed_Keys_Lists;

// The history of a specific axis on a specific device.
typedef struct {
    int *elems; // The recent history of the relative values or differences between absolute values of this axis.
    size_t count; // The number of elements in elems.

    // For absolute axes, the latest recorded absolute value. Is used to determine the difference for the next value,
    // and can also be used to draw the absolute value. Always 0 for relative axes.
    int current_value;

    int min; // The minimum value of this axis. Always 0 for relative axes.
    int max; // The maximum value of this axis. Always 0 for relative axes.
    bool is_absolute; // True when absolute, false when relative.

    char *device_id; // A unique identifier for the device for which the history is recorded.
    uint16 axis_id; // The identifier of the axis for which the history is recorded.
} NB_Axis_History;

// A list of histories of axes, one per device and axis combination.
// Every device may have multiple axes, there will be one Axis_History for every axis and every device that has
// registered input.
typedef struct {
    NB_Axis_History *elems;
    size_t count; // The number of elements in elems.
} NB_Axis_Histories;

// The complete input state.
typedef struct {
    NB_Pressed_Keys_Lists pressed_keys;
    NB_Axis_Histories axes;
} NB_Input_State;

///////////////////////// Functions /////////////////////////

// Returns the full current state of all monitored input devices.
// The arena is used to allocate all strings and unknown length lists in. When the arena is reset or freed,
// the returned NB_Input_State is no longer valid.
NB_Input_State hooks_get_state(Noh_Arena *arena);

// Initialize hooks and start listening to input events.
bool hooks_initialize();

// Re-initialize hooks and start listening to input events.
bool hooks_reinitialize();

// Shutdown hooks and stop listening to input events.
void hooks_shutdown();

#endif // HOOKS_H_
