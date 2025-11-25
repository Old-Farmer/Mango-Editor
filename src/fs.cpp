#include "fs.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>

#include "exception.h"
#include "linux/limits.h"
#include "logging.h"
#include "utils.h"

namespace mango {

static constexpr const char* kXDGConfigHomeEnv = "XDG_CONFIG_HOME";
static constexpr const char* kXDGCacheHomeEnv = "XDG_CACHE_HOME";

Path::Path() {}

Path::Path(std::string str) {
    MGO_ASSERT(!str.empty());
    if (str[0] == kPathSeperator) {
        absolute_path_ = std::move(str);
        size_t pos = absolute_path_.find(cwd_);
        in_cwd_ = pos != std::string::npos;
    } else {
        absolute_path_ = cwd_ + std::move(str);
        in_cwd_ = true;
    }

    std::string::size_type pos = absolute_path_.find_last_of(kPathSeperator);
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
    return zstring_view(absolute_path_.c_str() + cwd_.size(),
                        absolute_path_.size() - cwd_.size());
}

const std::string& Path::AbsolutePath() const noexcept {
    return absolute_path_;
}

const std::string& Path::GetCwd() noexcept { return cwd_; }

const std::string& Path::GetAppRoot() noexcept { return app_root_; }

std::string Path::GetConfig() {
    const char* env = getenv(kXDGConfigHomeEnv);
    std::string path;
    if (env == nullptr) {
        path = GetHome() + kPathSeperator + ".config" + kPathSeperator;
    } else {
        path = std::string(env) + kPathSeperator;
    }
    mkdir(path.c_str(), 755);  // best effort, ignore ret
    return path;
}

std::string Path::GetCache() {
    static std::string path = [] {
        const char* env = getenv(kXDGCacheHomeEnv);
        std::string path;
        if (env == nullptr) {
            path = GetHome() + kPathSeperator + ".cache" + kPathSeperator;
        } else {
            path = std::string(env) + kPathSeperator;
        }
        mkdir(path.c_str(), 755);  // best effort, ignore ret
        return path;
    }();
    return path;
}

std::string Path::GetHome() {
    const char* env = getenv("HOME");
    // TODO: better home detection?
    if (env == nullptr) {
        throw Exception("%s", "HOME detection fail");
    }
    return env;
}

const std::string& Path::GetCwdSys() {
    cwd_version_++;
    char buf[PATH_MAX + 1];
    char* ret = getcwd(buf, PATH_MAX + 1);
    if (ret == nullptr) {
        throw FSException("Getcwd Error: %s", strerror(errno));
    }
    cwd_ = std::string(buf) + kPathSeperator;
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
    auto pos = app_root_.find_last_of(kPathSeperator, app_root_.size() - 1);
    if (pos == std::string::npos) {
        throw FSException("%",
                          "GetAppRootSys Error: find_last_of can't find a /");
    }
    pos = app_root_.find_last_of(kPathSeperator, pos - 1);
    if (pos == std::string::npos) {
        throw FSException(
            "%", "GetAppRootSys Error: find_last_of can't find another /");
    }
    app_root_.resize(pos + 1);
    return app_root_;
}

int64_t Path::LastPathSeperator(std::string_view path) {
    size_t loc = path.find_last_of(kPathSeperator);
    return loc == std::string_view::npos ? -1 : loc;
}

std::vector<std::string> Path::ListUnderPath(const std::string& path) {
    std::vector<std::string> ret;

    DIR* dir = opendir(path.c_str());
    if (dir == nullptr) {
        if (errno == ENOTDIR) {
            return {};
        }
        throw FSException("opendir error: %s", strerror(errno));
    }
    struct dirent* ent;
    errno = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        if (ent->d_type == DT_DIR) {
            auto child_path = std::string(ent->d_name) + kPathSeperator;
            ret.push_back(std::move(child_path));
        } else if (ent->d_type == DT_REG) {
            auto child_path = std::string(ent->d_name);
            ret.push_back(std::move(child_path));
        }
        // We ignore other type file.
    }
    closedir(dir);  // If dir is not bad, closedir will never fail, so it will
                    // not effect errno.
    if (errno != 0) {
        throw FSException("readdir error: %s", strerror(errno));
    }
    return ret;
}

std::string Path::cwd_ = "";
int64_t Path::cwd_version_ = 0;

std::string Path::app_root_ = "";

}  // namespace mango