#include "noh.h"
#include "hooks.h"

// Internal representations of NBI_Input_state, using dynamic arrays.

typedef struct {
    uint16 *elems;
    size_t count;
    size_t capacity;

    size_t device_index;
} NBI_Pressed_Keys_List;

typedef struct {
    NBI_Pressed_Keys_List *elems;
    size_t count;
    size_t capacity;
} NBI_Pressed_Keys_Lists;

typedef struct {
    int *elems;
    size_t count;
    size_t start; // Additional parameter for circular list.
    size_t capacity;

    int current_value;
    // The time at which the last update was peformed, used to determine the speed from the difference in value,
    // compared to to difference in time.
    struct timespec last_updated_at;

    int min;
    int max;
    bool is_absolute;

    size_t device_index;
    uint16 axis_id;
} NBI_Axis_History;

typedef struct {
    NBI_Axis_History *elems;
    size_t count;
    size_t capacity;
} NBI_Axis_Histories;

typedef struct {
    NBI_Pressed_Keys_Lists pressed_keys;
    NBI_Axis_Histories axes;
} NBI_Input_State;

// Copy the data from an NBI_Input_State to an NB_Input_State, using data from the provided arena.
// Execute this function in a mutex that prevents modification of state, since it assumes this data to be static.
NB_Input_State copy_nbi_state_to_nb_state(Noh_Arena *arena, NBI_Input_State *state) {
    // Determine the space needed.
    size_t needed_space = 0;
    size_t no_keys_lists = 0;
    size_t no_axes = 0;

    // Space for pressed keys.
    for (size_t i = 0; i < state->pressed_keys.count; i++) {
        NBI_Pressed_Keys_List *list = &state->pressed_keys.elems[i];

        // Reserve space for the struct and all elements in the list.
        no_keys_lists++;
        needed_space += sizeof(NB_Pressed_Keys_List);
        needed_space += list->count * sizeof(list->elems[0]);
    }

    // Space for axis histories.
    for (size_t i = 0; i < state->axes.count; i++) {
        NBI_Axis_History *history = &state->axes.elems[i];

        // Reserve space for the struct and all elements in the list.
        no_axes++;
        needed_space += sizeof(NB_Axis_History);
        needed_space += history->count * sizeof(history->elems[0]);
    }

    noh_arena_reserve(arena, needed_space);

    // Prepare result structure.
    NB_Pressed_Keys_Lists keys_lists = {
        .count = no_keys_lists,
        .elems = (NB_Pressed_Keys_List *) noh_arena_alloc(arena, no_keys_lists * sizeof(NB_Pressed_Keys_List))
    };
    NB_Axis_Histories axis_histories = {
        .count = no_axes,
        .elems = (NB_Axis_History *) noh_arena_alloc(arena, no_axes * sizeof(NB_Axis_History))
    };
    NB_Input_State result = { .pressed_keys = keys_lists, .axes = axis_histories };

    // Copy the keys lists.
    size_t j = 0;
    for (size_t i = 0; i < state->pressed_keys.count; i++) {
        NBI_Pressed_Keys_List *list = &state->pressed_keys.elems[i];

        size_t data_size = list->count * sizeof(list->elems[0]);
        NB_Pressed_Keys_List new_list = {
            .count = list->count,
            .elems = noh_arena_alloc(arena, data_size),

            .device_index = list->device_index
        };
        memcpy(new_list.elems, list->elems, data_size);
        result.pressed_keys.elems[j++] = new_list;
    }

    // Copy the axes.
    j = 0;
    for (size_t i = 0; i < state->axes.count; i++) {
        NBI_Axis_History *history = &state->axes.elems[i];

        int elem_size = sizeof(history->elems[0]);
        size_t data_size = history->count * elem_size;
        NB_Axis_History new_history = {
            .count = history->count,
            .elems = noh_arena_alloc(arena, data_size),

            .current_value = history->current_value,

            .min = history->min,
            .max = history->max,
            .is_absolute = history->is_absolute,

            .device_index = history->device_index,
            .axis_id = history->axis_id

        };

        if (history->count < history->capacity) {
            // Just copy the whole data from 0 to the count.
            memcpy(new_history.elems, history->elems, data_size);
        } else {
            size_t slice_1_count = history->count - history->start;
            // First slice to the start of the buffer, from the start pointer.
            if (slice_1_count > 0) {
                memcpy(
                    new_history.elems,
                    history->elems + history->start,
                    slice_1_count * elem_size);
            }

            size_t slice_2_count = history->start;
            // Second slice after the first slice, from the beginning of the original buffer to the start pointer.
            if (slice_2_count > 0) {
                memcpy(
                    new_history.elems + slice_1_count,
                    history->elems,
                    slice_2_count * elem_size);
            }
        }

        result.axes.elems[j++] = new_history;
    }

    return result;
}

// Define a new pressed keys list, and return a pointer to this list.
NBI_Pressed_Keys_List *hooks_define_key_list(NBI_Input_State *state, NB_Input_Device *dev) {
    noh_assert(state);
    noh_assert(dev);

    NBI_Pressed_Keys_List list = {
        .elems = NULL,
        .count = 0,
        .capacity = 0,
        .device_index = dev->index
    };

    noh_da_append(&state->pressed_keys, list);
    return &state->pressed_keys.elems[state->pressed_keys.count - 1];
}

// Define a new absolute axis for the specified device and axis id, with the provided value.
// Returns a pointer to this axis.
NBI_Axis_History *hooks_define_abs_axis(NBI_Input_State *state, NB_Input_Device *dev, uint16 axis_id, const struct timespec *time, int value, int minimum, int maximum) {
    noh_assert(state);
    noh_assert(dev);
    noh_assert(time);

    NBI_Axis_History history = {
        .current_value = value,
        .last_updated_at = *time,

        .min = minimum,
        .max = maximum,
        .is_absolute = true,

        .device_index = dev->index,
        .axis_id = axis_id
    };
    noh_cb_initialize(&history, NB_INPUT_SMOOTH);

    noh_da_append(&state->axes, history);
    return &state->axes.elems[state->axes.count - 1];
}

// Define a new relative axis for the specified device and axis it.
// Returns a pointer to this axis.
NBI_Axis_History *hooks_define_rel_axis(NBI_Input_State *state, NB_Input_Device *dev, uint16 axis_id, const struct timespec *time) {
    noh_assert(state);
    noh_assert(dev);
    noh_assert(time);
    (void)axis_id;

    NBI_Axis_History history = {
        .last_updated_at = *time,

        .is_absolute = false,

        .device_index = dev->index,
        .axis_id = axis_id
    };
    noh_cb_initialize(&history, NB_INPUT_SMOOTH);

    noh_da_append(&state->axes, history);
    return &state->axes.elems[state->axes.count - 1];
}

// Register a keypress or release for the secified device and key.
// This function assumes a pointer to the relevant pressed keys list is already available.
void hooks_add_key_(NBI_Pressed_Keys_List *list, uint16 key, bool down) {
    noh_assert(list);

    // Check if the key exists.
    int index = -1;
    for (size_t i = 0; i < list->count; i++) {
        if (list->elems[i] == key) {
            index = i;
            break;
        }
    }

    if (down && index < 0) {
        // Add the key.
        noh_da_append(list, key);
    } else if (!down && index >= 0) {
        // Remove the key.
        noh_da_remove_at(list, (size_t)index);
    }
    // Otherwise, the key can remain in or out of the list.
}

// Register a keypress or release for the secified device and key.
// This function looks up the pressed keys list by device index.
void hooks_add_key(NBI_Input_State *state, size_t device_index, uint16 key, bool down) {
    noh_assert(state);

    for (size_t i = 0; i < state->pressed_keys.count; i++) {
        NBI_Pressed_Keys_List *list = &state->pressed_keys.elems[i];
        if (list->device_index == device_index) {
            hooks_add_key_(list, key, down);
            return;
        }
    }

    // The list was not found, log a warning.
    noh_log(NOH_WARNING, "Could not find key list of device %zu for entering keypress.", device_index);
}

// Add a new absolute value to an axis history.
// Updates the value and pushes into the circular history buffer.
// This function assumes a pointer to the relevant axis history is already available.
void hooks_add_abs_value_(NBI_Axis_History *history, const struct timespec *time, int value) {
    noh_assert(history);
    noh_assert(time);

    long ms_diff = noh_diff_timespec_ms(&history->last_updated_at, time);
    if (ms_diff < 1) ms_diff = 1; // FUTURE: Do we need to be more precise with the time differences?

    int diff = (value - history->current_value) / ms_diff;

    history->last_updated_at = *time;
    history->current_value = value;

    noh_cb_insert(history, diff);
}

// Add a new absolute value to an axis history.
// Updates the value and pushes into the circular history buffer.
// This function looks up the axis history by device index and axis id.
void hooks_add_abs_value(NBI_Input_State *state, size_t device_index, uint16 axis_id, const struct timespec *time, int value) {
    noh_assert(state);

    for (size_t i = 0; i < state->axes.count; i++) {
        NBI_Axis_History *history = &state->axes.elems[i];
        if (axis_id == history->axis_id && history->device_index == device_index && history->is_absolute) {
            hooks_add_abs_value_(history, time, value);
            return;
        }
    }

    // The axis was not found, log a warning.
    noh_log(NOH_WARNING, "Could not find axis %hu of device %zu for entering abs value.", axis_id, device_index);
}

// Add a new relative value to an axis history. Does not update the absolute value.
// Can still be used for an absolute value when pushing 0s to revert the relative history to 0.
// This function assumes a pointer to the relevant axis history is already available.
void hooks_add_rel_value_(NBI_Axis_History *history, const struct timespec *time, int value) {
    noh_assert(history);
    noh_assert(time);

    history->last_updated_at = *time;
    noh_cb_insert(history, value);
}

// Add a new relative value to an axis history. Does not update the absolute value.
// Can still be used for an absolute value when pushing 0s to revert the relative history to 0.
// This function looks up the axis history by device index and axis id.
void hooks_add_rel_value(NBI_Input_State *state, size_t device_index, uint16 axis_id, const struct timespec *time, int value) {
    noh_assert(state);

    for (size_t i = 0; i < state->axes.count; i++) {
        NBI_Axis_History *history = &state->axes.elems[i];
        if (axis_id == history->axis_id && history->device_index == device_index && !history->is_absolute) {
            hooks_add_rel_value_(history, time, value);
            return;
        }
    }

    // The axis was not found, log a warning.
    noh_log(NOH_WARNING, "Could not find axis %hu of device %zu for entering rel value.", axis_id, device_index);
}
