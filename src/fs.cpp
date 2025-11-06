#include "fs.h"

#include "character.h"
#include "exception.h"
#include "utils.h"

namespace mango {

Path::Path() {}

Path::Path(std::string str) {
    MGO_ASSERT(!str.empty());
    if (str[0] == kSlashChar) {
        absolute_path_ = std::move(str);
        size_t pos = absolute_path_.find(cwd_);
        in_cwd_ = pos != std::string::npos;
    } else {
        absolute_path_ = cwd_ + std::move(str);
        in_cwd_ = true;
    }

    std::string::size_type pos = absolute_path_.find_last_of(kSlashChar);
    MGO_ASSERT(pos != std::string::npos);
    file_name_len_ = absolute_path_.size() - pos - 1;
    last_cwd_version_ = cwd_version_;
}

zstring_view Path::FileName() const noexcept {
    return zstring_view(
        absolute_path_.c_str() + absolute_path_.size() - file_name_len_,
        file_name_len_);
}

zstring_view Path::ThisPath() noexcept {
    if (last_cwd_version_ != cwd_version_) {
        in_cwd_ = absolute_path_.find(cwd_) != std::string::npos;
    }
    if (!in_cwd_) {
        return zstring_view(absolute_path_);
    }
    // cwd_ doesn't have /
    return zstring_view(absolute_path_.c_str() + cwd_.size() + 1,
                        absolute_path_.size() - cwd_.size() - 1);
}

const std::string& Path::AbsolutePath() const noexcept {
    return absolute_path_;
}

const std::string& Path::GetCwd() noexcept { return cwd_; }

const std::string& Path::GetAppRoot() noexcept { return app_root_; }

const std::string& Path::GetCwdSys() {
    cwd_version_++;
    char buf[PATH_MAX + 1];
    char* ret = getcwd(buf, PATH_MAX + 1);
    if (ret == nullptr) {
        throw FSException("Getcwd Error: %s", strerror(errno));
    }
    cwd_ = std::string(buf) + kSlash;
    return cwd_;
}

const std::string& Path::GetAppRootSys() {
    char buf[PATH_MAX + 1];
    ssize_t len = readlink("/proc/self/exe", buf, PATH_MAX + 1);
    if (len == -1) {
        throw FSException("GetAppRootSys Error: %s", strerror(errno));
    }
    buf[len] = '\0';
    app_root_ = std::string(buf);
    auto pos = app_root_.find_last_of(kSlashChar, app_root_.size() - 1);
    if (pos == std::string::npos) {
        throw FSException("%",
                          "GetAppRootSys Error: find_last_of can't find a /");
    }
    pos = app_root_.find_last_of(kSlashChar, pos - 1);
    if (pos == std::string::npos) {
        throw FSException(
            "%", "GetAppRootSys Error: find_last_of can't find another /");
    }
    app_root_.resize(pos + 1);
    return app_root_;
}

std::string Path::cwd_ = "";
int64_t Path::cwd_version_ = 0;

std::string Path::app_root_ = "";

}  // namespace mango