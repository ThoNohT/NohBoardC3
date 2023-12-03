#ifndef NOH_H_
#define NOH_H_

#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/stat.h>

///////////////////////// Core stuff /////////////////////////

#define noh_array_len(array) (sizeof(array)/sizeof(array[0]))
#define noh_array_get(array, index) \
    (assert(index >= 0), assert(index < noh_array_get(array)), array[index])

// Allows returning a file after performing some deferred code.
// Usage:
// Define a result variable before the first call of this macro.
// Place a defer label at the end of the function where the work is done.
// Return result at the end of the deferred work.
#define noh_return_defer(value) do { result = (value); goto defer; } while(0)

// Reallocates some memory and crashes if it failed.
#define noh_realloc_check(target, size) noh_realloc_check_((void*)(target), (size))

// Returns the next argument as a c-string, moves the argv pointer to the next argument and decreases argc.
char *noh_shift_args(int *argc, char ***argv);

///////////////////////// Logging /////////////////////////

// Possible log levels.
typedef enum {
    NOH_INFO,
    NOH_WARNING,
    NOH_ERROR,
} Noh_Log_Level;

// Writes a formatted log message to stderr with the provided log level.
void noh_log(Noh_Log_Level level, const char *fmt, ...);

///////////////////////// Dynamic array /////////////////////////

#define NOH_DA_INIT_CAP 256

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
#define noh_da_append_multiple(da, new_elems, new_elems_count)                               \
do {                                                                                         \
    if ((da)->count + new_elems_count > (da)->capacity) {                                    \
        if ((da)->capacity == 0) (da)->capacity = NOH_DA_INIT_CAP;                           \
        while ((da)->count + new_elems_count > (da)->capacity) (da)->capacity *= 2;          \
        (da)->elems = noh_realloc_check((da)->elems, (da)->capacity * sizeof(*(da)->elems)); \
    }                                                                                        \
                                                                                             \
    memcpy((da)->elems + (da)->count, new_elems, new_elems_count * sizeof(*(da)->elems));    \
    (da)->count += new_elems_count;                                                          \
} while (0)

// Frees the elements in a dynamic array, and resets the count and capacity.
#define noh_da_free(da) \
do {                    \
    (da)->count = 0;    \
    (da)->capacity = 0; \
    free((da)->elems);  \
} while (0)


///////////////////////// Arena /////////////////////////  

#define NOH_ARENA_INIT_CAP 256

// Checkpoints in an arena.
typedef struct {
    size_t *elems;
    size_t count;
    size_t capacity;
} Noh_Arena_Checkpoints;

// An arena for storing temporary data.
typedef struct {
    char* data;
    Noh_Arena_Checkpoints checkpoints;
    size_t size;
    size_t capacity;
} Noh_Arena;

// Initialize an empty arena with the specified capacity.
Noh_Arena noh_arena_init(size_t capacity);

// Resets the size of an arena to 0.
void noh_arena_reset(Noh_Arena *arena);

// Frees all data in an arena, setting the count and capacity to 0.
void noh_arena_free(Noh_Arena *arena);

// Allocates data in an arena of the requested size, returns the start of the data.
void *noh_arena_alloc(Noh_Arena *arena, size_t size);

// Saves the current size of the arena.
void noh_arena_save(Noh_Arena *arena);

// Rewinds an arena to the last saved checkpoint.
void noh_arena_rewind(Noh_Arena *arena);

// Copies a c-string to the arena.
char *noh_arena_strdup(Noh_Arena *arena, const char *cstr);

// Prints the specified formatted string to the arena.
char *noh_arena_sprintf(Noh_Arena *arena, const char *format, ...);

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
#define noh_string_free(string) noh_da_free(string)

// Reads the contents of a file into a Noh_String.
bool noh_string_read_file(Noh_String *string, const char *filename);

///////////////////////// String view /////////////////////////  

// A view of a string, that does not own the data.
typedef struct {
    size_t count;
    const char *elems;
} Noh_String_View;

// Finds the first occurrence of the specified delimiter in a string view and returns the part of the string until 
// that delimiter. The string view itself is shrunk to start after the delimiter.
Noh_String_View noh_sv_chop_by_delim(Noh_String_View *sv, char delim);

// Trims the left part of a string view, until the provided function no longer holds on the current character.
void noh_sv_trim_left(Noh_String_View *sv, bool (*do_trim)(char));

// Trims the right part of a string view, until the provided function no longer holds on the current character.
void noh_sv_trim_right(Noh_String_View *sv, bool (*do_trim)(char));

// Trims both sides of a string view, until the provided function no longer holds on the current character.
void noh_sv_trim(Noh_String_View *sv, bool (*do_trim)(char));

// Trims spaces from the left part of a string view.
void noh_sv_trim_space_left(Noh_String_View *sv);

// Trims spaces from the right part of a string view.
void noh_sv_trim_space_right(Noh_String_View *sv);

// Trims spaces from both sides of a string view.
void noh_sv_space_trim(Noh_String_View *sv);

// Creates a string view from a c-string.
Noh_String_View noh_sv_from_cstr(const char *cstr);

// Checks whether to string views contain the same string.
bool noh_sv_eq(Noh_String_View a, Noh_String_View b);

// Creates a cstring in an arena from a string view.
const char *noh_sv_to_arena_cstr(Noh_Arena *arena, Noh_String_View sv);

// printf macros for Noh_String_View
#define Nsv_Fmt "%.*s"
#define Nsv_Arg(sv) (int) (sv).count, (sv).data
// USAGE:
//   Noh_String_View name = ...;
//   printf("Name: "Nsv_Fmt"\n", Nsv_Arg(name));

///////////////////////// Files and directories /////////////////////////

// File paths.
typedef struct {
    char **elems;
    size_t count;
    size_t capacity;
} Noh_File_Paths;

// Creates the path at the specified directory if it does not exist.
// Does not create any missing parent directories.
bool noh_mkdir_if_needed(const char *path);

// Renames a file.
bool noh_rename(const char *path, const char *new_path);

// Removes a file.
bool noh_remove(const char *path);

#endif // NOH_H_

#ifdef NOH_IMPLEMENTATION

///////////////////////// Core stuff /////////////////////////  

void* noh_realloc_check_(void *target, size_t size) {
    target = realloc(target, size);
    assert(target != NULL && "Could not allocate enough memory");
    return target;
}

char *noh_shift_args(int *argc, char ***argv) {
    assert(*argc > 0 && "No more arguments");

    char *result = **argv;
    (*argv)++;
    (*argc)--;

    return result;
}

///////////////////////// Logging /////////////////////////  

void noh_log(Noh_Log_Level level, const char *fmt, ...)
{
    switch (level) {
    case NOH_INFO:
        fprintf(stderr, "[INFO] ");
        break;
    case NOH_WARNING:
        fprintf(stderr, "[WARNING] ");
        break;
    case NOH_ERROR:
        fprintf(stderr, "[ERROR] ");
        break;
    default:
        assert(false && "Invalid log level");
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

///////////////////////// Arena /////////////////////////  

Noh_Arena noh_arena_init(size_t size) {
    Noh_Arena arena = {0};

    arena.data = noh_realloc_check(arena.data, size);
    Noh_Arena_Checkpoints checkpoints = {0};
    arena.checkpoints = checkpoints;
    arena.size = 0;
    arena.capacity = size;

    return arena;
}

void noh_arena_reset(Noh_Arena *arena) {
    arena->size = 0;
    arena->checkpoints.count = 0;
}

void noh_arena_free(Noh_Arena *arena) {
    free(arena->data);
    noh_da_free(&arena->checkpoints);
    arena->capacity = 0;
    arena->size = 0;
}

void *noh_arena_alloc(Noh_Arena *arena, size_t size) {
    size_t new_size = arena->size + size;

    // If the new size would exceed the capacity, extend the arena size.
    if (new_size > arena->capacity) {
        size_t new_cap = arena->capacity == 0 ? NOH_ARENA_INIT_CAP : arena->capacity * 2;
        // Double capacity until the new size fits.
        while (new_cap < new_size) new_cap *= 2;

        // Re-allocate data, free old data and set new capacity.
        arena->data = noh_realloc_check(arena->data, new_cap);
        arena->capacity = new_cap;
    }

    // Return pointer to start of returned memory block
    void *result = &arena->data[arena->size];
 
    // Move size to beyond requested size.
    arena->size = new_size;
    return result;
}

void noh_arena_save(Noh_Arena *arena) {
    noh_da_append(&arena->checkpoints, arena->size);
}

void noh_arena_rewind(Noh_Arena *arena) {
    assert(arena->checkpoints.count > 0 && "No history to rewind");

    size_t checkpoint = arena->checkpoints.elems[arena->checkpoints.count - 1];
    arena->checkpoints.count -= 1;
    arena->size = checkpoint;
}

char *noh_arena_strdup(Noh_Arena *arena, const char *cstr) {
    size_t len = strlen(cstr);
    char *result = noh_arena_alloc(arena, len + 1);
    memcpy(result, cstr, len);
    result[len] = '\0';
    return result;
}

char *noh_arena_sprintf(Noh_Arena *arena, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int n = vsnprintf(NULL, 0, format, args);
    va_end(args);

    assert(n >= 0);
    char *result = noh_arena_alloc(arena, n + 1);
    va_start(args, format);
    vsnprintf(result, n + 1, format, args);
    va_end(args);

    return result;
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
    free(buf);
    if (f) fclose(f);
    return result;
}

///////////////////////// String view /////////////////////////  

void increase_sv_position(Noh_String_View *sv, size_t distance) {
    if (distance <- sv->count) {
        sv->count -= distance;
        sv->elems += distance;
    } else {
        sv->count = 0;
        sv->elems += sv->count;
    }
}

Noh_String_View noh_sv_chop_by_delim(Noh_String_View *sv, char delim) {
    size_t i = 0;
    // Find the character, or the end of the string view.
    while (i < sv->count && sv->elems[i] != delim) i++;

    // The data until the delimiter is returned.
    Noh_String_View result = { .count = i, .elems = sv->elems };

    // Update the current string view beyond the delimiter.
    increase_sv_position(sv, i + 1);

    return result;
}

void noh_sv_trim_left(Noh_String_View *sv, bool (*do_trim)(char)) {
    size_t i = 0;
    while (i < sv->count && (*do_trim)(sv->elems[i])) i++;
    increase_sv_position(sv, i);
}

void noh_sv_trim_right(Noh_String_View *sv, bool (*do_trim)(char)) {
    size_t i = sv->count;
    while (i > 0 && (*do_trim)(sv->elems[i-1])) i--;
    sv->count = i;
}

void noh_sv_trim(Noh_String_View *sv, bool (*do_trim)(char)) {
    noh_sv_trim_left(sv, do_trim);
    noh_sv_trim_right(sv, do_trim);
}

bool is_space(char c) {
    return isspace(c) > 0;
}

void noh_sv_trim_space_left(Noh_String_View *sv) {
    noh_sv_trim_left(sv, &is_space);
}

void noh_sv_trim_space_right(Noh_String_View *sv) {
    noh_sv_trim_right(sv, &is_space);
}

void noh_sv_trim_space(Noh_String_View *sv) {
    noh_sv_trim(sv, &is_space);
}

Noh_String_View noh_sv_from_cstr(const char *cstr) {
    Noh_String_View result = {0};
    result.elems = cstr;
    result.count = strlen(cstr);
    return result;
}

bool noh_sv_eq(Noh_String_View a, Noh_String_View b) {
    if (a.count != b.count) return false;

    for (size_t i = 0; i < a.count; i++) {
        if (a.elems[i] != b.elems[i]) return false;
    }

    return  true;
}

const char *noh_sv_to_arena_cstr(Noh_Arena *arena, Noh_String_View sv)
{
    char *result = noh_arena_alloc(arena, sv.count + 1);
    memcpy(result, sv.elems, sv.count);
    result[sv.count] = '\0';
    return result;
}

///////////////////////// Files and directories /////////////////////////

bool noh_mkdir_if_needed(const char *path) {
    int result = mkdir(path, 0755);

    if (result == 0) {
        noh_log(NOH_INFO, "Created directory '%s'.", path);
        return true;
    }

    if (errno == EEXIST) {
        noh_log(NOH_INFO, "Directory '%s' already exists.", path);
        return true;
    }

    noh_log(NOH_ERROR, "Could not create directory '%s': %s", path, strerror(errno));
    return false;
}

bool noh_rename(const char *path, const char *new_path)
{
    noh_log(NOH_INFO, "Renaming '%s' to '%s'.", path, new_path);
    if (rename(path, new_path) < 0) {
        noh_log(NOH_ERROR, "Rename failed: %s", strerror(errno));
        return false;
    }

    return true;
}

bool noh_remove(const char *path) {
    noh_log(NOH_INFO, "Removing '%s'.", path);
    if (remove(path) < 0) { 
        noh_log(NOH_ERROR, "Remove failed: %s", strerror(errno));
        return false;
    }

    return true;
}

#endif // NOH_IMPLEMENTATION
