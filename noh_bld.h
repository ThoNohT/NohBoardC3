#ifndef NOH_BLD_H_
#define NOH_BLD_H_

// Ensure that noh.h is available.
#ifndef NOH_IMPLEMENTATION
#error "Please include noh.h with implementation!"
#else

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

#endif // NOH_IMPLEMENTATION
#endif // NOH_BLD_H

