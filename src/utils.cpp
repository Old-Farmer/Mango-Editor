#include "utils.h"

#include <sys/wait.h>

#include <cstdio>
#include <cstdlib>

#include "os.h"
#include "term.h"

namespace mango {

void AssertFail(const char* __assertion, const char* __file,
                unsigned int __line, const char* __function) {
    mango::Terminal::GetInstance().Shutdown();
    __assert_fail(__assertion, __file, __line, __function);
}

Result Exec(const char* file, char* const argv[], const std::string* stdin_data,
            std::string* stdout_data, std::string* stderr_data, int& exit_code,
            bool check) {
    Fd stdin_pipe[2], stdout_pipe[2], stderr_pipe[2], err_pipe[2];
    if (stdin_data) {
        Pipe(stdin_pipe);
    }
    if (stdout_data) {
        Pipe(stdout_pipe);
    }
    if (stderr_data) {
        Pipe(stderr_pipe);
    }
    Pipe(err_pipe);

    int pid = Fork();
    if (pid == 0) {
        // Terminal::GetInstance().Shutdown();
        // catch all in child process
        try {
            Fd null_dev = Open("/dev/null");
            if (stdin_data) {
                stdin_pipe[1].Close();
                Dup2(stdin_pipe[0], STDIN_FILENO);
                stdin_pipe[0].Close();
            } else {
                Dup2(null_dev, STDIN_FILENO);
            }
            if (stdout_data) {
                stdout_pipe[0].Close();
                Dup2(stdout_pipe[1], STDOUT_FILENO);
                stdout_pipe[1].Close();
            } else {
                Dup2(null_dev, STDOUT_FILENO);
            }
            if (stderr_data) {
                stderr_pipe[0].Close();
                Dup2(stderr_pipe[1], STDERR_FILENO);
                stderr_pipe[1].Close();
            } else {
                Dup2(null_dev, STDERR_FILENO);
            }
            err_pipe[0].Close();
            null_dev.Close();

            if (-1 == execvp(file, argv)) {
                char buf[128];
                int n = snprintf(buf, 128, "execvp error: %s", strerror(errno));
                err_pipe[1].Write(buf, n);
                exit(EXIT_FAILURE);
            }
        } catch (Exception& e) {
            err_pipe[1].Write(e.what(), strlen(e.what()));
            exit(EXIT_FAILURE);
        }
    }

    try {
        if (stdin_data) {
            stdin_pipe[0].Close();
            stdin_pipe[1].Write(stdin_data->data(), stdin_data->size());
            stdin_pipe[1].Close();
        }
        if (stdout_data) {
            stdout_pipe[1].Close();
            char buf[4096];
            while (true) {
                ssize_t n = stdout_pipe[0].Read(buf, 4096);
                if (n == 0) {
                    break;
                }
                stdout_data->append(buf, n);
            }
            stdout_pipe[0].Close();
        }
        if (stderr_data) {
            stderr_pipe[1].Close();
            char buf[4096];
            while (true) {
                ssize_t n = stderr_pipe[0].Read(buf, 4096);
                if (n == 0) {
                    break;
                }
                stderr_data->append(buf, n);
            }
            stderr_pipe[0].Close();
        }
    } catch (OSException& e) {
        // Fast clean if we met exception
        while (waitpid(pid, nullptr, 0) == -1) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        throw;
    }

    int status;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) {
            continue;
        }
        throw OSException(errno, "waitpid error: %s", strerror(errno));
    }
    exit_code = WEXITSTATUS(status);

    if (exit_code != EXIT_SUCCESS) {
        // Maybe not command error, but just before or when execvp.
        err_pipe[1].Close();
        char buf[128 + 1];
        ssize_t n = err_pipe[0].Read(buf, 128);
        if (n > 128) {
            n = 128;
        }
        buf[n] = '\0';
        err_pipe[0].Close();
        if (n > 0) {
            if (check) {
                MGO_LOG_INFO(
                    "Error occure before or when execvp in child process: %s",
                    buf);
            } else {
                MGO_LOG_ERROR(
                    "Error occure before or when execvp in child process: %s",
                    buf);
            }
            return kOuterCommandExecuteFail;
        }
    }
    return kOk;
}

}  // namespace mango