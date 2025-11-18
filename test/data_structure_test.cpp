#include "catch2/catch_test_macros.hpp"
#include "trie.h"

using namespace mango;

TEST_CASE("trie tree") {
    Trie trie;

    trie.Add("int");
    REQUIRE(trie.PrefixWith("i").size() == 1);
    trie.Add("int");
    REQUIRE(trie.PrefixWith("i").size() == 1);
    trie.Add("int");
    REQUIRE(trie.PrefixWith("i").size() == 1);
    trie.Add("int64_t");
    REQUIRE(trie.PrefixWith("i").size() == 2);
    trie.Add("long");
    REQUIRE(trie.PrefixWith("lo").size() == 1);
    trie.Add("short");
    REQUIRE(trie.PrefixWith("shor").size() == 1);
    REQUIRE(trie.PrefixWith("a").empty());
    trie.Delete("int");
    trie.Delete("int");
    trie.Delete("int");
    REQUIRE(trie.PrefixWith("i").size() == 1);
    trie.Delete("int64_t");
    REQUIRE(trie.PrefixWith("i").size() == 0);
}