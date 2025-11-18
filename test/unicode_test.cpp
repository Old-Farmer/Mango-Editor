#include "catch2/catch_test_macros.hpp"
#include "character.h"

using namespace mango;

TEST_CASE("test string display width") {
    REQUIRE(StringWidth("你好12") == 4 + 2);
    REQUIRE(StringWidth(" a é न 🇺🇸 👩‍👩‍👧 🏳️‍🌈 👨‍⚕️ 👩‍🚀 💖 z") == 26);
}

TEST_CASE("bound class test") {
    int byte_eat;
    Codepoint c;
    REQUIRE(Utf8ToUnicode("a", 1, byte_eat, c) == kOk);
    REQUIRE(utf8proc_get_property(c)->boundclass == UTF8PROC_BOUNDCLASS_OTHER);

    REQUIRE(Utf8ToUnicode("柴", strlen("柴"), byte_eat, c) == kOk);
    REQUIRE(utf8proc_get_property(c)->boundclass == UTF8PROC_BOUNDCLASS_OTHER);
}

TEST_CASE("Grapheme detection") {
    Character c;
    int byte_len;
    const char* str;
    str = "💖";
    ThisCharacterInUtf8(str, 0, c, byte_len);
    CHECK(byte_len == strlen(str));
    CHECK(c.Width() == 2);

    str = "🇺🇸";
    ThisCharacterInUtf8(str, 0, c, byte_len);
    CHECK(byte_len == strlen(str));
    CHECK(c.Width() == 2);

    str = "🏳️‍🌈";
    ThisCharacterInUtf8(str, 0, c, byte_len);
    CHECK(byte_len == strlen(str));
    CHECK(c.Width() == 2);

    str = "👩‍👩‍👧";
    ThisCharacterInUtf8(str, 0, c, byte_len);
    CHECK(byte_len == strlen(str));
    CHECK(c.Width() == 2);

    str = "é";
    ThisCharacterInUtf8(str, 0, c, byte_len);
    CHECK(byte_len == strlen(str));
    CHECK(c.Width() == 1);
}