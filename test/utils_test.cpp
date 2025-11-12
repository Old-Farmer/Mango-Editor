#include "utils.h"

#include <gsl/util>

#include "catch2/catch_test_macros.hpp"
#include "logging.h"

using namespace mango;

TEST_CASE("test Exec") {
    LogInit("utils_test.log");
    auto _ = gsl::finally([] { LogDeinit(); });

    SECTION("Normal echo, test stdout") {
        Result res;
        std::string stdout_data;
        int exit_code;
        char* const argv[] = {const_cast<char*>("echo"),
                              const_cast<char*>("hello,world!"), NULL};
        res = Exec(argv[0], argv, nullptr, &stdout_data, nullptr, exit_code);
        REQUIRE(res == kOk);
        REQUIRE(exit_code == 0);
        REQUIRE(stdout_data == "hello,world!\n");
    }
    SECTION("cat, test stdin") {
        Result res;
        std::string stdout_data;
        std::string stdin_data = "hello, world!";
        int exit_code;
        char* const argv[] = {const_cast<char*>("cat"), NULL};
        res =
            Exec(argv[0], argv, &stdin_data, &stdout_data, nullptr, exit_code);
        REQUIRE(res == kOk);
        REQUIRE(exit_code == 0);
        REQUIRE(stdout_data == stdin_data);
    }
    SECTION("ls, test stderr") {
        Result res;
        std::string stderr_data;
        int exit_code;
        char* const argv[] = {const_cast<char*>("ls"),
                              const_cast<char*>("not_exist_file"), NULL};
        res = Exec(argv[0], argv, nullptr, nullptr, &stderr_data, exit_code);
        REQUIRE(res == kOk);
        REQUIRE(exit_code == 2);
        REQUIRE(
            stderr_data ==
            "ls: cannot access 'not_exist_file': No such file or directory\n");
    }
    SECTION("test not exit command") {
        Result res;
        int exit_code;
        char* const argv[] = {const_cast<char*>("abcdefg"), NULL};
        res = Exec(argv[0], argv, nullptr, nullptr, nullptr, exit_code, true);
        REQUIRE(res == mango::kOuterCommandExecuteFail);
    }
}
