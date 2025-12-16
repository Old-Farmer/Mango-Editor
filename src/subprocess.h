#pragma once

#include "os.h"
#include "result.h"

namespace mango {

// A subprocess is a child process with some pipe(Unix concept) resource.
class Subprocess {
   public:
    // When check == true, LOG level will be info instead of error.
    // argv[] should be null terminated, and the first element should be
    // file path, read execvp for more info.
    // Thorw OSException when som syscalls fail.
    Subprocess(const char* const argv[], bool check = false,
               bool need_stdin = true, bool need_stdout = true,
               bool need_stderr = true);
    MGO_DELETE_COPY(Subprocess);
    MGO_DEFAULT_MOVE(Subprocess);
    // Will call Wait if wait hasn't called before.
    ~Subprocess() noexcept;

    // If need_xxx == false, Getxxx will get a invalid fd.
    // Don't use a invalid fd.
    Fd& GetStdin() { return stdin_; }
    Fd& GetStdout() { return stdout_; }
    Fd& GetStderr() { return stderr_; }

    // Wait the subprocess.
    // exit_code will be set to the exit code.
    // Throw OSException.
    // Return kOk if ok;
    // Return kOuterCommandExecuteFail if outer command fail
    // before or when execvp.
    Result Wait(int& exit_code);

   private:
    int pid_;
    Fd stdin_;
    Fd stdout_;
    Fd stderr_;
    Fd err_;
    bool already_wait_ = false;
    bool check_;
};

// Thorw OSException when som syscalls fail.
// Return kOk if ok, kOuterCommandExecuteFail if outer command fail before or
// when execvp.
// When check == true, LOG level will be info instead of error.
// argv[] should be null terminated, and the first element should be file path,
// read execvp for more info.
// Stdxxx_data will pipe to the child process fd, if
// null, corresponding fd will be dupped to /dev/null.
Result Exec(const char* const argv[], const std::string* stdin_data,
            std::string* stdout_data, std::string* stderr_data, int& exit_code,
            bool check = false);

}  // namespace mango