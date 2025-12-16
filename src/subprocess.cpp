#include "subprocess.h"

#include <sys/wait.h>

#include "logging.h"

namespace mango {

Subprocess::Subprocess(const char* const argv[], bool check, bool need_stdin,
                       bool need_stdout, bool need_stderr)
    : check_(check) {
    Fd stdin_pipe[2], stdout_pipe[2], stderr_pipe[2], err_pipe[2];
    if (need_stdin) {
        Pipe(stdin_pipe);
    }
    if (need_stdout) {
        Pipe(stdout_pipe);
    }
    if (need_stderr) {
        Pipe(stderr_pipe);
    }
    Pipe(err_pipe);
    pid_ = Fork();
    if (pid_ == 0) {
        // Terminal::GetInstance().Shutdown();
        // catch all in child process
        try {
            Fd null_dev = Open("/dev/null");
            if (need_stdin) {
                stdin_pipe[1].Close();
                Dup2(stdin_pipe[0], STDIN_FILENO);
                stdin_pipe[0].Close();
            } else {
                Dup2(null_dev, STDIN_FILENO);
            }
            if (need_stdout) {
                stdout_pipe[0].Close();
                Dup2(stdout_pipe[1], STDOUT_FILENO);
                stdout_pipe[1].Close();
            } else {
                Dup2(null_dev, STDOUT_FILENO);
            }
            if (need_stderr) {
                stderr_pipe[0].Close();
                Dup2(stderr_pipe[1], STDERR_FILENO);
                stderr_pipe[1].Close();
            } else {
                Dup2(null_dev, STDERR_FILENO);
            }
            err_pipe[0].Close();
            null_dev.Close();

            if (-1 == execvp(argv[0], const_cast<char* const*>(argv))) {
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
        if (need_stdin) {
            stdin_pipe[0].Close();
            stdin_ = std::move(stdin_pipe[1]);
        }
        if (need_stdout) {
            stdout_pipe[1].Close();
            stdout_ = std::move(stdout_pipe[0]);
        }
        if (need_stderr) {
            stderr_pipe[1].Close();
            stderr_ = std::move(stderr_pipe[0]);
        }
        err_pipe[1].Close();
        err_ = std::move(err_pipe[0]);
    } catch (OSException& e) {
        // Fast clean if we met exception
        while (waitpid(pid_, nullptr, 0) == -1) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        throw;
    }
}

Subprocess::~Subprocess() noexcept {
    if (already_wait_) {
        return;
    }

    try {
        int exit_code;
        Wait(exit_code);
    } catch (std::exception& e) {
        MGO_LOG_ERROR("{}", e.what());
    }
}

Result Subprocess::Wait(int& exit_code) {
    already_wait_ = true;

    int status;
    while (waitpid(pid_, &status, 0) == -1) {
        if (errno == EINTR) {
            continue;
        }
        throw OSException(errno, "waitpid error: {}", strerror(errno));
    }
    exit_code = WEXITSTATUS(status);

    if (exit_code != EXIT_SUCCESS) {
        // Maybe not command error, but just before or when execvp.
        char buf[128 + 1];
        ssize_t n = err_.Read(buf, 128);
        if (n > 128) {
            n = 128;
        }
        buf[n] = '\0';
        if (n > 0) {
            if (check_) {
                MGO_LOG_INFO(
                    "Error occure before or when execvp in child process: {}",
                    buf);
            } else {
                MGO_LOG_ERROR(
                    "Error occure before or when execvp in child process: {}",
                    buf);
            }
            return kOuterCommandExecuteFail;
        }
    }
    return kOk;
}

Result Exec(const char* const argv[], const std::string* stdin_data,
            std::string* stdout_data, std::string* stderr_data, int& exit_code,
            bool check) {
    Subprocess subprocess(argv, check, stdin_data, stdout_data, stderr_data);
    if (stdin_data) {
        subprocess.GetStdin().Write(stdin_data->data(), stdin_data->size());
        subprocess.GetStdin().Close();  // Some apps need close to commit, we
                                        // unconditinally add this.
    }
    if (stdout_data) {
        char buf[4096];
        while (true) {
            ssize_t n = subprocess.GetStdout().Read(buf, 4096);
            if (n == 0) {
                break;
            }
            stdout_data->append(buf, n);
        }
    }
    if (stderr_data) {
        char buf[4096];
        while (true) {
            ssize_t n = subprocess.GetStderr().Read(buf, 4096);
            if (n == 0) {
                break;
            }
            stderr_data->append(buf, n);
        }
    }
    return subprocess.Wait(exit_code);
}

}  // namespace mango