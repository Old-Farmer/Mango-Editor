#include "fs.h"

#include "catch2/catch_test_macros.hpp"

using namespace charxed;

TEST_CASE("Path Normalize") {
    if (kPathSeperator == '/') {
        const std::string p = "/a/b/c/../.././/x/";
        REQUIRE(Path::Normalize(p) == "/a/x/");
    }
}

TEST_CASE("FS rmdir recursively ") {
    const std::string test_root = "__charxed_rm_dir_test";
    MakeDirectory(test_root);
    MakeDirectory(Path::JoinPath(test_root, "my_dir"));

    MakeDirectory(Path::JoinPath(test_root, "my_dir", "my_dir_inner"));
    MakeDirectory(
        Path::JoinPath(test_root, "my_dir", "my_dir_inner", "my_file"));
    CreateFile(Path::JoinPath(test_root, "my_dir", "my_file"));

    RemoveDirectory(Path::JoinPath(test_root, "my_dir"), true);

    REQUIRE(ListUnderDirectory(Path::JoinPath(test_root)).empty());
    RemoveDirectory(test_root, false);
}
