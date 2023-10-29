#ifndef CORE_H_
#define CORE_H_

#include <stdint.h>
#include <stddef.h>

//////////////////// Core stuff /////////////////////////

#define CORE_DA_INIT_CAP = 256;

// Defines a dynamic array of the specified element type.
// Usage example: DynArray(int) IntArray;
#define Core_DynArray(TYPE) typedef struct { \
     const TYPE *elems;                      \
     size_t count;                           \
     size_t capacity;                        \
 }

// Appends an element to a dynamic array, allocates more memory and moves all elements to newly allocated memory
// if needed.
#define core_da_append(da, elem)                                                                            \
    do {                                                                                                    \
        if ((da)->count >= (da)->capacity) {                                                                \
            (da)->capacity = (da)->capacity == 0 ? DA_INIT_CAP : (da)->capacity * 2;                        \
            (da)->elems = realloc((da)->elems, (da)->capacity * sizeof(*(da)->elems));                      \
            assert((da)->elems != NULL && "Could not allocate enough memory for dynamic array extension."); \
        }                                                                                                   \
                                                                                                            \
    (da)->elems[(da)->count++] = (elem);                                                                    \
    } while(0)

// Appends multiple elements to a dynamic array. Allocates more memory and moves all elements to newly allocated memory
// if needed.
#define core_da_append_multiple(da, new_elems, new_elems_count) 
    do {                                                                                                    \
        if ((da)->count + new_elems_count > (da)->capacity) {                                               \
            if ((da)->capacity == 0) (da)->capacity = CORE_DA_INIT_CAP;                                     \
            while ((da)->count + new_elems_count > (da)->capacity) (da)->capacity *= 2;                     \
            (da)->elems = realloc((da)->elems, (da)->capacity * sizeof(*(da)->elems));                      \
                                                                                                            \
            assert((da)->elems != NULL && "Could not allocate enough memory for dynamic array extension."); \
        }                                                                                                   \
        memcpy((da)->elems + (da)->count, new_elems, new_elems_count * sizeof(*(da)->elems));               \
        (da)->count += new_elems_count;                                                                     \
    } while (0)

#define core_da_free(da) free((da).elems)

//////////////////// Build tools /////////////////////////

// Defines a command that can be run.
Core_DynArray(*char) Core_Cmd;

// Appends one or more strings to a command.
#define core_cmd_append(cmd, ...) \
    core_da_append_multiple(      \
        cmd,                      \
        ((const char*[]){__VA_ARGS__}), (sizeof((const char*[]){__VA_ARGS__}) / sizeof(const char*)))

// Resets a command, freeing the memory used for its elements and setting the count and capacity to 0.
#define core_cmd_reset(cmd) \
    {                      \
        cmd.count = 0;     \
        cmd.capacity = 0;  \
        free(cmd.elems);   \
    }


//////////////////// Main /////////////////////////

int main() {
    return 0;
}

#endif // CORE_H_
