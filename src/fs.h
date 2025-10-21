#pragma once

#include <string>

#include "utils.h"

namespace mango {

class Path {
   public:
    Path();
    // relative or absolute path as str
    Path(std::string str);

    bool Empty() const noexcept { return absolute_path_.empty(); }

    zstring_view FileName() const noexcept;
    zstring_view ThisPath() noexcept;
    const std::string& AbsolutePath() const noexcept;

    static const std::string& GetCwd() noexcept;

    // throws FSException
    static const std::string& GetCwdSys();

   private:
    std::string absolute_path_;
    size_t file_name_len_;
    bool in_cwd_;
    int64_t last_cwd_version_;

    static std::string cwd_;
    static int64_t cwd_version_;  // changing or getting cwd by syscall need
                                  // bump up the version
};

}  // namespace mango
