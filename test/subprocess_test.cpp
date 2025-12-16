#include "subprocess.h"

#include <gsl/util>

#include "catch2/catch_test_macros.hpp"
#include "logging.h"

using namespace mango;

TEST_CASE("test Exec") {
    LogInit("subprocess_test.log");
    auto _ = gsl::finally([] { LogDeinit(); });

    SECTION("Normal echo, test stdout") {
        Result res;
        std::string stdout_data;
        int exit_code;
        const char* const argv[] = {"echo", "hello,world!", nullptr};
        res = Exec(argv, nullptr, &stdout_data, nullptr, exit_code);
        REQUIRE(res == kOk);
        REQUIRE(exit_code == 0);
        REQUIRE(stdout_data == "hello,world!\n");
    }
    SECTION("cat, test stdin") {
        Result res;
        std::string stdout_data;
        std::string stdin_data = "hello, world!";
        int exit_code;
        const char* const argv[] = {"cat", nullptr};
        res = Exec(argv, &stdin_data, &stdout_data, nullptr, exit_code);
        REQUIRE(res == kOk);
        REQUIRE(exit_code == 0);
        REQUIRE(stdout_data == stdin_data);
    }
    SECTION("ls, test stderr") {
        Result res;
        std::string stderr_data;
        int exit_code;
        const char* const argv[] = {"ls", "not_exist_file", nullptr};
        res = Exec(argv, nullptr, nullptr, &stderr_data, exit_code);
        REQUIRE(res == kOk);
        REQUIRE(exit_code == 2);
        REQUIRE(
            stderr_data ==
            "ls: cannot access 'not_exist_file': No such file or directory\n");
    }
    SECTION("test not exit command") {
        Result res;
        int exit_code;
        const char* const argv[] = {"abcdefg", nullptr};
        res = Exec(argv, nullptr, nullptr, nullptr, exit_code, true);
        REQUIRE(res == mango::kOuterCommandExecuteFail);
    }
}
