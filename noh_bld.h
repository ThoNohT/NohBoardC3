// Ensure that noh.h is available.
#ifndef NOH_IMPLEMENTATION
#error "Please include noh.h with implementation!"
#else

#ifndef NOH_BLD_H_
#define NOH_BLD_H_

#include <unistd.h>
#include <sys/wait.h>

///////////////////////// Processes /////////////////////////

bool noh_proc_wait(pid_t pid);

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
#define noh_cmd_free(cmd) noh_da_free(cmd)

// Runs a command asynchronously and returns the process id.
pid_t noh_cmd_run_async(Noh_Cmd cmd);

// Runs a command synchronously.
bool noh_cmd_run_sync(Noh_Cmd cmd);

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

///////////////////////// Commands /////////////////////////

pid_t noh_cmd_run_async(Noh_Cmd cmd) {
    if (cmd.count < 1) {
        noh_log(NOH_ERROR, "Cannot run an empty command.");
        return -1;
    }

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

#endif // NOH_BLD_IMPLEMENTATION
#endif // NOH_IMPLEMENTATION
