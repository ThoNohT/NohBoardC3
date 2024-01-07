#define NOH_IMPLEMENTATION
#include "src/noh.h"
#define NOH_BLD_IMPLEMENTATION
#include "noh_bld.h"

#define RAYLIB_PATH "./raylib-5.0"

#ifdef _WIN32
    #define NOH_COMPILER "gcc"
#else
    #define NOH_COMPILER "clang"
#endif // _WIN32

bool build_nohboard() {
    bool result = true;
    Noh_Arena arena = noh_arena_init(10 KB);

    Noh_Cmd cmd = {0};
    Noh_File_Paths input_paths = {0};
    noh_da_append(&input_paths, "./src/noh.h");
    noh_da_append(&input_paths, "./src/main.c");
    noh_da_append(&input_paths, "./src/hooks.c");
#ifdef _WIN32
    noh_da_append(&input_paths, "./src/hooks_windows.c");
#else
    noh_da_append(&input_paths, "./src/hooks_linux.c");
#endif // _WIN32
    noh_da_append(&input_paths, "./src/hooks.h");
    noh_da_append(&input_paths, "./build/raylib/libraylib.a");

    int needs_rebuild = noh_output_is_older("./build/NohBoard", input_paths.elems, input_paths.count);
    if (needs_rebuild < 0) noh_return_defer(false);
    if (needs_rebuild == 0) {
        noh_log(NOH_INFO, "NohBoard is up to date.");
        noh_return_defer(true);
    }

    noh_cmd_append(&cmd, NOH_COMPILER);

    // c-flags
    noh_cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");

    char *raylib_link = noh_arena_sprintf(&arena, "-I%s/src", RAYLIB_PATH);
    noh_cmd_append(&cmd, raylib_link);

    // Output
    noh_cmd_append(&cmd, "-o", "./build/NohBoard");

    // Source
    noh_cmd_append(&cmd, "./src/main.c");
#ifdef _WIN32
    noh_da_append(&cmd, "./src/hooks_windows.c");
#else
    noh_da_append(&cmd, "./src/hooks_linux.c");
#endif // _WIN32

    // Linker
    noh_cmd_append(&cmd, "-L./build/raylib", "-l:libraylib.a");
#ifdef _WIN32
    noh_cmd_append(&cmd, "-lm", "-lwinmm", "-lgdi32", "-ldinput8", "-ldxguid");
    noh_cmd_append(&cmd, "-static");
#else
    noh_cmd_append(&cmd, "-lm", "-ldl", "-lpthread", "-lrt");
#endif // _WIN32

    if (!noh_cmd_run_sync(cmd)) noh_return_defer(false);

defer:
    noh_cmd_free(&cmd);
    noh_da_free(&input_paths);
    noh_arena_free(&arena);
    return result;
}

bool build_raylib() {
    bool result = true;

    noh_mkdir_if_needed("./build/raylib/");

    // Compile individual libraries.
    Noh_Cmd cmd = {0};

    Noh_Procs procs = {0};
    char *files[] = { "raudio", "rcore", "rglfw", "rmodels", "rshapes", "rtext", "rtextures", "utils" };

    bool updated = false;
    Noh_Arena arena = noh_arena_init(5 KB);
    for (size_t i = 0; i < noh_array_len(files); i++) {
        char *source_path = noh_arena_sprintf(&arena, "%s/src/%s.c", RAYLIB_PATH, files[i]);
        char *output_path = noh_arena_sprintf(&arena, "./build/raylib/%s.o", files[i]);

        int needs_rebuild = noh_output_is_older(output_path, &source_path, 1);
        if (needs_rebuild < 0) noh_return_defer(false);
        if (needs_rebuild == 0) continue;

        updated = true;

        noh_cmd_append(&cmd, NOH_COMPILER);
        noh_cmd_append(&cmd, "-Wno-everything"); // We don't care about warnings in the raylib source.
        noh_cmd_append(&cmd, "-ggdb", "-DPLATFORM_DESKTOP");
        char *glfw_include_path = noh_arena_sprintf(&arena, "-I%s/src/external/glfw/include", RAYLIB_PATH);
        noh_cmd_append(&cmd, glfw_include_path);
        noh_cmd_append(&cmd, "-c", source_path);
        noh_cmd_append(&cmd, "-o", output_path);

        Noh_Pid pid = noh_cmd_run_async(cmd);
        noh_da_append(&procs, pid);

        cmd.count = 0;
        noh_arena_reset(&arena);
    }

    if (!noh_procs_wait(procs)) noh_return_defer(false);

    // Combine libraries if any was updated.
    if (!updated) {
        noh_log(NOH_INFO, "Raylib is up to date.");
        noh_return_defer(true);
    }

    noh_cmd_append(&cmd, "ar", "-crs", "./build/raylib/libraylib.a");
    for (size_t i = 0; i < noh_array_len(files); i++) {
        char *input_path = noh_arena_sprintf(&arena, "./build/raylib/%s.o", files[i]);
        noh_cmd_append(&cmd, input_path);
    }
    if (!noh_cmd_run_sync(cmd)) noh_return_defer(false);
    cmd.count = 0;

defer:
    noh_arena_free(&arena);
    noh_cmd_free(&cmd);
    noh_procs_free(&procs);
    return result;
}

void print_usage(char *program) {
    noh_log(NOH_INFO, "Usage: %s <command>", program);
    noh_log(NOH_INFO, "Available commands:");
    noh_log(NOH_INFO, "- build: build NohBoard (default).");
    noh_log(NOH_INFO, "- run: build and run NohBoard.");
    noh_log(NOH_INFO, "- clean: clean all build artifacts.");
}

int main(int argc, char **argv) {
    noh_rebuild_if_needed(argc, argv);
    char *program = noh_shift_args(&argc, &argv);

    // Determine command.
    char *command = NULL;
    if (argc == 0) {
        command = "build";
    } else {
        command = noh_shift_args(&argc, &argv);
    }

    // Ensure build directory exists.
    if (!noh_mkdir_if_needed("./build")) return 1;

    if (strcmp(command, "build") == 0) {
        // Only build.
        if (!build_raylib()) return 1;
        if (!build_nohboard()) return 1;

    } else if (strcmp(command, "run") == 0) {
        // Build and run.
        if (!build_raylib()) return 1;
        if (!build_nohboard()) return 1;

        Noh_Cmd cmd = {0};
        noh_cmd_append(&cmd, "./build/NohBoard");
        if (!noh_cmd_run_sync(cmd)) return 1;
        noh_cmd_free(&cmd);

    } else if (strcmp(command, "test") == 0) {
        // Build and debug.
        if (!build_raylib()) return 1;
        if (!build_nohboard()) return 1;

        Noh_Cmd cmd = {0};
        noh_cmd_append(&cmd, "gf2", "./build/NohBoard");
        if (!noh_cmd_run_sync(cmd)) return 1;
        noh_cmd_free(&cmd);

    } else if (strcmp(command, "clean") == 0) {
        Noh_Cmd cmd = {0};
        noh_cmd_append(&cmd, "rm", "-rf", "./build/");
        if (!noh_cmd_run_sync(cmd)) return 1;
        noh_cmd_free(&cmd);

    } else {
        print_usage(program);
        noh_log(NOH_ERROR, "Invalid command: '%s'", command);
        return 1;

    }
} 
