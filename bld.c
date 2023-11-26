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
    return true;
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
