#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "result.h"
#include "utils.h"

namespace charxed {

constexpr char kPathSeperator = '/';

enum class XDGPath { kConfig, kData, kState, kCache };

// TODO: windows support
class Path {
   public:
    Path();
    // relative or absolute path as str
    explicit Path(const std::string& str);

    friend inline bool operator==(const Path& p1, const Path& p2) {
        return p1.AbsolutePath() == p2.AbsolutePath();
    }
    friend inline bool operator!=(const Path& p1, const Path& p2) {
        return p1.AbsolutePath() != p2.AbsolutePath();
    }

    bool Empty() const noexcept { return absolute_path_.empty(); }

    zstring_view FileName() const noexcept;
    // ThisPath return a zstring_view for showing.
    // If you construct Path with a relative path,
    // return relative path,
    // else return absolute path.
    zstring_view ThisPath() noexcept;
    const std::string& AbsolutePath() const noexcept;
    // return a absolute dir path
    std::string_view Dir() const noexcept;

    // All dirs return from this class all have a slash at the end

    // return normalized path.
    static const std::string& GetCwd() noexcept;
    static const std::string& GetAppRoot() noexcept;
    static const std::string& GetHome() noexcept;

    // config path will not be created,
    // others will be created if not exist, best effort.
    // NOTE: must call after GetHomeSys
    // return normalized path.
    static std::string GetXDGPath(XDGPath p);

    // return normalized path.
    // throws FSException
    static const std::string& GetCwdSys();
    static const std::string& GetAppRootSys();
    // return normalized path.
    // throw Exception if home can't be detected
    static const std::string& GetHomeSys();

    static int64_t LastPathSeperator(std::string_view path);

    static bool IsAbsolutePath(std::string_view path);
    static bool HaveHomeSymbol(std::string_view path);
    // Must have home symbol
    static std::string ReplaceHomeSymbol(std::string_view path);

    // Join all with kPathSeperator
    template <typename... Rest>
    static std::string JoinPath(std::string_view first, Rest&&... rest) {
        std::string result(first);
        ((result += kPathSeperator, result += rest), ...);
        return result;
    }

    // path must be absolute path
    static std::string Normalize(const std::string& path);

   private:
    void GenRelativePath();

    std::string absolute_path_;
    std::string relative_path_;
    size_t file_name_len_;
    int64_t last_cwd_version_;

    static std::string home_;

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

// list all entries under this path,
// if a entry is a directory, append a kPathSeperator.
// throws FSException
// if path is not a dir, return empty vector.
std::vector<std::string> ListUnderDirectory(const std::string& path);

constexpr uint32_t kFMRead = 1 << 0;
constexpr uint32_t kFMWrite = 1 << 1;
constexpr uint32_t kFMExec = 1 << 2;

struct FileStat {
    uint32_t mode = 0;
    size_t size = 0;
};

// path shouldn't be empty
// throws FSException
// return kOk for success, kNotExist for not exist.
Result GetFileStat(const std::string& path, FileStat& file_stat);

// throw FSException
void CreateFile(const std::string& path);

// throw FSException
void RemoveFile(const std::string& path);

// throw FSException
void MakeDirectory(const std::string& path);

// Recursively remove directory
// path must have trailing path sepeator
// throw FSException
void RemoveDirectory(const std::string& path, bool recursive);

}  // namespace charxed
