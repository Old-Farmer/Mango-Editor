#include "logging.h"

#include <iostream>
#include <vector>

#include "exception.h"

using namespace mango;

int main() {
    LogInit();
    MANGO_LOG_ERROR("%s %d", "hello", 1);

    std::vector<std::string> lines;

    auto f = [&lines] -> const std::vector<std::string>& { return lines; };
    const std::vector<std::string>& x = f();
    std::cout << x.size() << std::endl;

    return 0;
}