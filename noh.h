#ifndef NOH_H_
#define NOH_H_

#define NOH_REALLOC realloc
#define NOH_ASSERT assert
#define NOH_FREE free

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

///////////////////////// Core stuff /////////////////////////

// Allows returning a file after performing some deferred code.
// Usage:
// Define a result variable before the first call of this macro.
// Place a defer label at the end of the function where the work is done.
// Return result at the end of the deferred work.
#define noh_return_defer(value) do { result = (value); goto defer; } while(0)

#define NOH_DA_INIT_CAP 256

void* noh_realloc_check_(void *target, size_t size);
// Reallocates some memory and crashes if it failed.
#define noh_realloc_check(target, size) noh_realloc_check_((void*)(target), (size))

// Appends an element to a dynamic array, allocates more memory and moves all elements to newly allocated memory
// if needed.
#define noh_da_append(da, elem)                                                              \
do {                                                                                         \
    if ((da)->count >= (da)->capacity) {                                                     \
        (da)->capacity = (da)->capacity == 0 ? NOH_DA_INIT_CAP : (da)->capacity * 2;         \
        (da)->elems = noh_realloc_check((da)->elems, (da)->capacity * sizeof(*(da)->elems)); \
    }                                                                                        \
                                                                                             \
    (da)->elems[(da)->count++] = (elem);                                                     \
} while(0)

// Appends multiple elements to a dynamic array. Allocates more memory and moves all elements to newly allocated memory
// if needed.
#define noh_da_append_multiple(da, new_elems, new_elems_count)                                   \
do {                                                                                             \
    if ((da)->count + new_elems_count > (da)->capacity) {                                        \
        if ((da)->capacity == 0) (da)->capacity = NOH_DA_INIT_CAP;                               \
        while ((da)->count + new_elems_count > (da)->capacity) (da)->capacity *= 2;              \
        (da)->elems = noh_realloc_check((da)->elems, (da)->capacity * sizeof(*(da)->elems));     \
    }                                                                                            \
                                                                                                 \
    memcpy((void*)(da)->elems + (da)->count, new_elems, new_elems_count * sizeof(*(da)->elems)); \
    (da)->count += new_elems_count;                                                              \
} while (0)

// Frees the elements in a dynamic array, and resets the count and capacity.
#define noh_da_reset(da) \
do {                     \
    (da)->count = 0;     \
    (da)->capacity = 0;  \
    free((da)->elems);   \
} while (0)

///////////////////////// Strings /////////////////////////  

// Defines a string that can be extended.
typedef struct {
    char *elems;
    size_t count;
    size_t capacity;
} Noh_String;

// Appends a null-terminated string into a Noh_String.
void noh_string_append_cstr(Noh_String *string, const char *cstr);

// Appends null into a Noh_String.
void noh_string_append_null(Noh_String *string);

// Resets a Noh_String, freeing the memory used and settings the count and capacity to 0.
#define noh_string_reset(string) noh_da_reset(string)

// Reads the contents of a file into a Noh_String.
bool noh_string_read_file(Noh_String *string, const char *filename);

///////////////////////// Commands /////////////////////////

// Defines a command that can be run.
typedef struct { 
     const char* *elems;
     size_t count;
     size_t capacity;
 } Noh_Cmd;

// Appends one or more strings to a command.
#define noh_cmd_append(cmd, ...) \
noh_da_append_multiple(          \
    cmd,                         \
    ((const char*[]){__VA_ARGS__}), (sizeof((const char*[]){__VA_ARGS__}) / sizeof(const char*)))

// Resets a command, freeing the memory used for its elements and setting the count and capacity to 0.
#define noh_cmd_reset(cmd) noh_da_reset(cmd)

#endif // NOH_H_

#ifdef NOH_IMPLEMENTATION

///////////////////////// Core /////////////////////////  

void* noh_realloc_check_(void *target, size_t size) {
    target = NOH_REALLOC(target, size);
    NOH_ASSERT(target != NULL && "Could not allocate enough memory");
    return target;
}

///////////////////////// Strings /////////////////////////

void noh_string_append_cstr(Noh_String *string, const char *cstr) {
    size_t len = strlen(cstr);
    noh_da_append_multiple(string, cstr, len);
}

void noh_string_append_null(Noh_String *string) {
    noh_da_append(string, '\0');
}

bool noh_string_read_file(Noh_String *string, const char *filename) {
    bool result = true;
    size_t buf_size = 32*1024;
    char *buf = NULL;
    buf = noh_realloc_check(buf, buf_size);

    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        // TODO: Change printf to proper logging implementaton.
        printf("Could not open file %s: %s.\n", filename, strerror(errno));
        noh_return_defer(false);
    }

    size_t n = fread(buf, 1, buf_size, f);
    while (n > 0) {
        noh_da_append_multiple(string, buf, n);
        n = fread(buf, 1, buf_size, f);
    }

    if (ferror(f)) {
        // TODO: Change printf to proper logging implementaton.
        printf("Could not read file %s: %s.\n", filename, strerror(errno));
        noh_return_defer(false);
    }

defer:
    NOH_FREE(buf);
    if (f) fclose(f);
    return result;
}

///////////////////////// Commands /////////////////////////

#endif // NOH_IMPLEMENTATION
