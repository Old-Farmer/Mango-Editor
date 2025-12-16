#pragma once

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "exception.h"
#include "utils.h"
namespace mango {

// An RAII fd wrapper.
// This struct should not handle sth you should not close, like stdin stdout
// stderr.
struct Fd {
    int fd = -1;
    Fd() {}
    Fd(int _fd) : fd(_fd) {}
    MGO_DELETE_COPY(Fd);
    Fd(Fd&& other) noexcept {
        fd = other.fd;
        other.fd = -1;
    }
    Fd& operator=(Fd&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (fd != -1) {
            close(fd);
        }

        fd = other.fd;
        other.fd = -1;
        return *this;
    }
    // throw OSException
    ssize_t Read(void* buf, size_t size) {
        MGO_ASSERT(fd != -1);
        while (true) {
            ssize_t n = read(fd, buf, size);
            if (n >= 0) {
                return n;
            }
            if (errno == EINTR) {
                continue;
            }
            throw OSException(errno, "read error: {}", strerror(errno));
        }
    }

    // throw OSException
    void Write(const void* buf, size_t size) {
        MGO_ASSERT(fd != -1);
        while (true) {
            ssize_t n = write(fd, buf, size);
            if (n == static_cast<ssize_t>(size)) {
                return;
            }
            if (errno == EINTR) {
                continue;
            }
            throw OSException(errno, "write error: {}", strerror(errno));
        }
    }
    void Close() {
        if (fd != -1) {
            close(fd);
            fd = -1;
        }
    }
    ~Fd() { Close(); }
};

// throw OSException
inline void Pipe(Fd pipe_fd[2]) {
    int rc = pipe(reinterpret_cast<int*>(pipe_fd));
    if (rc == -1) {
        throw OSException(errno, "pipe error: {}", strerror(errno));
    }
}

// throw OSException
inline int Fork() {
    int pid = fork();
    if (pid == -1) {
        throw OSException(errno, "fork error: {}", strerror(errno));
    }
    return pid;
}

// throw OSException
inline void Dup2(const Fd& old_fd, int new_fd) {
    MGO_ASSERT(new_fd == STDIN_FILENO || new_fd == STDOUT_FILENO ||
               new_fd == STDERR_FILENO);
    while (true) {
        int rc = dup2(old_fd.fd, new_fd);
        if (rc == -1) {
            if (errno == EINTR) {
                continue;
            }
            throw OSException(errno, "dup2 error: {}", strerror(errno));
        }
        break;
    }
}

// throw OSException
inline Fd Open(const char* file, int oflags = O_RDWR, mode_t mode = 0644) {
    Fd fd;
    while (true) {
        if (oflags & O_CREAT) {
            fd.fd = open(file, oflags);
        } else {
            fd.fd = open(file, oflags, mode);
        }
        if (fd.fd >= 0) {
            return fd;
        }
        if (errno == EINTR) {
            continue;
        }
        throw OSException(errno, "open error: {}", strerror(errno));
    }
}

}  // namespace mango