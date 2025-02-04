// Ensure that noh.h is available.
#ifndef NOH_IMPLEMENTATION
#error "Please include noh.h with implementation!"
#else

#ifndef NOH_BLD_H_
#define NOH_BLD_H_

#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

///////////////////////// Processes /////////////////////////

// A collection of processes.
typedef struct {
    pid_t *elems;
    size_t count;
    size_t capacity;
} Noh_Procs;

// Waits for a single process.
bool noh_proc_wait(pid_t pid);

// Waits for a collection of processes.
bool noh_procs_wait(Noh_Procs procs);

// Frees the collection pocesses.
#define noh_procs_free(procs) noh_da_free(procs);

// Resets the collection of processes.
#define noh_procs_reset(procs) noh_da_reset(procs);

///////////////////////// Commands /////////////////////////

// Defines a command that can be run.
typedef struct { 
     const char **elems;
     size_t count;
     size_t capacity;
 } Noh_Cmd;

// Appends one or more strings to a command.
#define noh_cmd_append(cmd, ...) \
noh_da_append_multiple(          \
    cmd,                         \
    ((const char*[]){__VA_ARGS__}), (sizeof((const char*[]){__VA_ARGS__}) / sizeof(const char*)))

// Frees a command, freeing the memory used for its elements and setting the count and capacity to 0.
#define noh_cmd_free(cmd) noh_da_free(cmd)

// Resets a command, setting the count to 0.
#define noh_cmd_reset(cmd) noh_da_reset(cmd)

// Runs a command asynchronously and returns the process id.
pid_t noh_cmd_run_async(Noh_Cmd cmd);

// Runs a command synchronously.
bool noh_cmd_run_sync(Noh_Cmd cmd);

// Renders a textual representation of the command into the provided string.
void noh_cmd_render(Noh_Cmd cmd, Noh_String *string);

///////////////////////// Building /////////////////////////

// Call this macro at the start of a build script. It will check if the source file is newer than the executable
// that is running, and if so, rebuild the source and call the new executable.
#define noh_rebuild_if_needed(argc, argv)                                               \
    do {                                                                                \
        const char *source_path = __FILE__;                                             \
        noh_assert(argc >= 1);                                                          \
        const char *binary_path = argv[0];                                              \
                                                                                        \
        int rebuild_needed = noh_output_is_older(binary_path, (char**)&source_path, 1); \
        if (rebuild_needed < 0) exit(1);                                                \
        if (rebuild_needed) {                                                           \
            Noh_String backup_path = {0};                                               \
            noh_string_append_cstr(&backup_path, binary_path);                          \
            noh_string_append_cstr(&backup_path, ".old");                               \
            noh_string_append_null(&backup_path);                                       \
            if (!noh_rename(binary_path, backup_path.elems)) exit(1);                   \
                                                                                        \
            Noh_Cmd rebuild = {0};                                                      \
            noh_cmd_append(&rebuild, "cc", "-o", binary_path, source_path);             \
            bool rebuild_succeeded = noh_cmd_run_sync(rebuild);                         \
            noh_cmd_free(&rebuild);                                                     \
            if (!rebuild_succeeded) {                                                   \
                noh_rename(backup_path.elems, binary_path);                             \
                exit(1);                                                                \
            }                                                                           \
                                                                                        \
            noh_remove(backup_path.elems);                                              \
                                                                                        \
            Noh_Cmd actual_run = {0};                                                   \
            noh_da_append_multiple(&actual_run, argv, argc);                            \
            if (!noh_cmd_run_sync(actual_run)) exit(1);                                 \
            exit(0);                                                                    \
        }                                                                               \
    } while(0)

// Indicates whether the file at the output is older than any of the files at the input paths.
// 1 means it is older, 0 means it is not older, -1 means the check failed.
int noh_output_is_older(const char *output_path, char **input_paths, size_t input_paths_count);

#endif // NOH_BLD_H

#ifdef NOH_BLD_IMPLEMENTATION

///////////////////////// Processes /////////////////////////

bool noh_proc_wait(pid_t pid)
{
    if (pid == -1) return false;

    for (;;) {
        int wstatus = 0;
        if (waitpid(pid, &wstatus, 0) < 0) {
            noh_log(NOH_ERROR, "Could not wait for command (pid %d): %s", pid, strerror(errno));
            return false;
        }

        if (WIFEXITED(wstatus)) {
            int exit_status = WEXITSTATUS(wstatus);
            if (exit_status != 0) {
                noh_log(NOH_ERROR, "Command exited with exit code %d", exit_status);
                return false;
            }

            break;
        }

        if (WIFSIGNALED(wstatus)) {
            noh_log(NOH_ERROR, "Command process was terminated by %s", strsignal(WTERMSIG(wstatus)));
            return false;
        }
    }

    return true;
}

bool noh_procs_wait(Noh_Procs procs) {
    bool success = true;
    for (size_t i = 0; i < procs.count; i++) {
        success = noh_proc_wait(procs.elems[i]) && success;
    }

    return success;
}

///////////////////////// Commands /////////////////////////

// Adds a c string to a string, surrounding it with single quotes if it contains any spaces.
void noh_quote_if_needed(const char *value, Noh_String *string) {
    if (!strchr(value, ' ')) {
        noh_string_append_cstr(string, value);
    } else {
        noh_da_append(string, '\'');
        noh_string_append_cstr(string, value);
        noh_da_append(string, '\'');
    }
}

void noh_cmd_render(Noh_Cmd cmd, Noh_String *string) {
    for (size_t i = 0; i < cmd.count; ++i) {
        const char *arg = cmd.elems[i];
        if (arg == NULL) break;
        if (i > 0) noh_string_append_cstr(string, " ");
        noh_quote_if_needed(arg, string);
    }
}

pid_t noh_cmd_run_async(Noh_Cmd cmd) {
    if (cmd.count < 1) {
        noh_log(NOH_ERROR, "Cannot run an empty command.");
        return -1;
    }

    // Log the command.
    Noh_String sb = {0};
    noh_cmd_render(cmd, &sb);
    noh_da_append(&sb, '\0');
    noh_log(NOH_INFO, "CMD: %s", sb.elems);
    noh_string_free(&sb);

    pid_t cpid = fork();
    if (cpid < 0) {
        noh_log(NOH_ERROR, "Could not fork child process: %s", strerror(errno));
        return -1;
    }

    if (cpid == 0) {
        // NOTE: This leaks a bit of memory in the child process.
        // But do we actually care? It's a one off leak anyway...
        // Create a command that is null terminated.
        Noh_Cmd cmd_null = {0};
        noh_da_append_multiple(&cmd_null, cmd.elems, cmd.count);
        noh_cmd_append(&cmd_null, NULL);

        if (execvp(cmd.elems[0], (char * const*) cmd_null.elems) < 0) {
            noh_log(NOH_ERROR, "Could not execute child process: %s", strerror(errno));
            exit(1);
        }
        noh_assert(0 && "unreachable");
    }

    return cpid;
}

bool noh_cmd_run_sync(Noh_Cmd cmd) {
    pid_t pid = noh_cmd_run_async(cmd);
    if (pid == -1) return false;

    return noh_proc_wait(pid);
}

///////////////////////// Building /////////////////////////

int noh_output_is_older(const char *output_path, char **input_paths, size_t input_paths_count) {
    struct stat statbuf = {0};

    if (stat(output_path, &statbuf) < 0) {
        if (errno == ENOENT) return 1; // The output path doesn't exist, consider that older.
        noh_log(NOH_ERROR, "Could not stat '%s': %s", output_path, strerror(errno));
        return -1;
    }

    struct timespec output_time = statbuf.st_mtim;

    for (size_t i = 0; i < input_paths_count; i++) {
        if (stat(input_paths[i], &statbuf) < 0) {
            // Non-existent input path means a source file does not exist, fail.
            noh_log(NOH_ERROR, "Could not stat '%s': %s", input_paths[i], strerror(errno));
            return -1;
        }

        // Any newer source file means the output is older.
        if (noh_diff_timespec_ms(&statbuf.st_mtim, &output_time) > 0) return 1;
    }

    return 0;
}

#endif // NOH_BLD_IMPLEMENTATION
#endif // NOH_IMPLEMENTATION
