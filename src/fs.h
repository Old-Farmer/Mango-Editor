#pragma once

#include <string>

#include "utils.h"

namespace mango {

class Path {
   public:
    Path();
    // relative or absolute path as str
    explicit Path(std::string str);

    bool Empty() const noexcept { return absolute_path_.empty(); }

    zstring_view FileName() const noexcept;
    zstring_view ThisPath() noexcept;
    const std::string& AbsolutePath() const noexcept;

    // Cwd and AppRoot all have a slash at the end

    static const std::string& GetCwd() noexcept;
    static const std::string& GetAppRoot() noexcept;

    // throws FSException
    static const std::string& GetCwdSys();
    static const std::string& GetAppRootSys();

   private:
    std::string absolute_path_;
    size_t file_name_len_;
    bool in_cwd_;
    int64_t last_cwd_version_;

    static std::string cwd_;
    static int64_t cwd_version_;  // changing or getting cwd by syscall need
                                  // bump up the version

    static std::string app_root_;  // NOTE: app root is the root directory of
                                   // the application. The binary is always put
                                   // in the <root>/bin if it is packaged. If it
                                   // is compiled from source, make sure
                                   // that executable is in <project-root>/xxx,
                                   // where xxx can build, build-debug whatever.
};

}  // namespace mango
