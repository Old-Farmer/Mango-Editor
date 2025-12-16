#include "logging.h"

#include <unistd.h>

#include "catch2/catch_test_macros.hpp"
#include "sys/stat.h"

using namespace mango;

TEST_CASE("logging test") {
    logging_file = fopen("logging_test.log", "w+");
    REQUIRE(logging_file);
    MGO_LOG_ERROR("{} {}", "hello", 1);

    struct stat my_stat;
    REQUIRE(fstat(fileno(logging_file), &my_stat) == 0);
    REQUIRE(my_stat.st_size == 125);

    fclose(logging_file);
}