#include <stdio.h>

#define NOH_IMPLEMENTATION
#include "src/noh.h"
#define NOH_BLD_IMPLEMENTATION
#include "noh_bld.h"

bool build_nohboard() {
    bool result = true;

    Noh_Cmd cmd ={0};

    noh_cmd_append(&cmd, "clang");

    // c-flags
    noh_cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    noh_cmd_append(&cmd, "-I./raylib/src");

    // Output
    noh_cmd_append(&cmd, "-o", "./build/NohBoard");

    // Source
    noh_cmd_append(&cmd, "src/main.c");

    // Linker
    noh_cmd_append(&cmd, "-lm", "-ldl", "-lpthread");
    noh_cmd_append(&cmd, "-L./build/raylib", "-l:libraylib.a");

    if (!noh_cmd_run_sync(cmd)) noh_return_defer(false);

defer:
    noh_cmd_free(&cmd);
    return result;
}

bool build_raylib() {
    bool result = true;

    Noh_Cmd cmd = {0};

    // TODO: Replace with code for creating directory.
    // Recreate the raylib directory empty.
    noh_cmd_append(&cmd, "mkdir", "-p","./build/raylib/");
    if (!noh_cmd_run_sync(cmd)) noh_return_defer(false);
    cmd.count = 0;

    noh_cmd_append(&cmd, "rm", "-r","./build/raylib/");
    if (!noh_cmd_run_sync(cmd)) noh_return_defer(false);
    cmd.count = 0;

    noh_cmd_append(&cmd, "mkdir", "-p","./build/raylib/");
    if (!noh_cmd_run_sync(cmd)) noh_return_defer(false);
    cmd.count = 0;

    char *files[] = { "raudio", "rcore", "rglfw", "rmodels", "rshapes", "rtext", "rtextures", "utils" };

    // Compile individual libraries.
    Noh_Arena arena = {0};
    for (size_t i = 0; i < noh_array_len(files); i++) {
        noh_cmd_append(&cmd, "clang");

        // c-flags
        noh_cmd_append(&cmd, "-ggdb", "-DPLATFORM_DESKTOP");
        noh_cmd_append(&cmd, "-I./raylib/src/external/glfw/include");

        // Source
        char *source_path = noh_arena_sprintf(&arena, "./raylib/src/%s.c", files[i]);
        noh_cmd_append(&cmd, "-c", source_path);

        // output
        char *output_path = noh_arena_sprintf(&arena, "build/raylib/%s.o", files[i]);
        noh_cmd_append(&cmd, "-o", output_path);

        if (!noh_cmd_run_sync(cmd)) noh_return_defer(false);
        cmd.count = 0;

        noh_arena_reset(&arena);
    }

    // Combine libraries.
    noh_cmd_append(&cmd, "ar", "-crs", "build/raylib/libraylib.a");
    for (size_t i = 0; i < noh_array_len(files); i++) {
        char *input_path = noh_arena_sprintf(&arena, "build/raylib/%s.o", files[i]);
        noh_cmd_append(&cmd, input_path);
    }
    if (!noh_cmd_run_sync(cmd)) noh_return_defer(false);
    cmd.count = 0;

    noh_arena_reset(&arena);

defer:
    noh_cmd_free(&cmd);
    return result;
}

void print_usage(char *program) {
    noh_log(NOH_INFO, "Usage: %s", program);
}

int main(int argc, char **argv) {
    char *program = noh_shift_args(&argc, &argv);

    // Determine command.
    char *command = NULL;
    if (argc == 0) {
        command = "build";
    } else {
        command = noh_shift_args(&argc, &argv);
    }

    if (strcmp(command, "build") == 0) {
        if (!build_nohboard()) return 1;
    } else if (strcmp(command, "raylib") == 0) {
        if (!build_raylib()) return 1;
    } else if (strcmp(command, "run") == 0) {
    } else {
        noh_log(NOH_ERROR, "Invalid command: '%s'", command);
        print_usage(program);
    }
} 
