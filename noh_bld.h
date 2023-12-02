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

// Resets a command, freeing the memory used for its elements and setting the count and capacity to 0.
#define noh_cmd_free(cmd) noh_da_free(cmd)

// Runs a command asynchronously and returns the process id.
pid_t noh_cmd_run_async(Noh_Cmd cmd);

// Runs a command synchronously.
bool noh_cmd_run_sync(Noh_Cmd cmd);

// Renders a textual representation of the command into the provided string.
void noh_cmd_render(Noh_Cmd cmd, Noh_String *string);

///////////////////////// Building /////////////////////////

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
    for (size_t i = 0; i < procs.count ; i++) {
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
        assert(0 && "unreachable");
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

    int output_time = statbuf.st_mtim.tv_sec;

    for (size_t i = 0; i < input_paths_count; i++) {
        if (stat(input_paths[i], &statbuf) < 0) {
            // Non-existent input path means a source file does not exist, fail.
            noh_log(NOH_ERROR, "Could not stat '%s': %s", input_paths[i], strerror(errno));
            return -1;
        }

        // Any newer source file means the output is older.
        if (statbuf.st_mtim.tv_sec > output_time) return 1;
    }

    return 0;
}

#endif // NOH_BLD_IMPLEMENTATION
#endif // NOH_IMPLEMENTATION
