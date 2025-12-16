#include <inttypes.h>

#include "catch2/catch_test_macros.hpp"
#include "character.h"
#include "fmt/core.h"

using namespace mango;

TEST_CASE("test string display width") {
    CHECK(StringWidth("你好12") == 4 + 2);
    CHECK(StringWidth(" a é न 🇺🇸 👩‍👩‍👧 🏳️‍🌈 👨‍⚕️ "
                      "👩‍🚀 "
                      "💖 "
                      "z") == 26);
    CHECK(StringWidth("A á ❤️ ☝︎ ✊🏿 👨‍👩‍👧‍👦 👩‍❤️‍💋‍👩 🇨🇳 "
                      "1️⃣ "
                      "🏳️‍🌈 ❤︎‍🔥 🧑‍🍼 ǟ̋") ==
          33);
}

TEST_CASE("bound class test") {
    int byte_eat;
    Codepoint c;
    REQUIRE(Utf8ToUnicode("a", 1, byte_eat, c) == kOk);
    REQUIRE(utf8proc_get_property(c)->boundclass == UTF8PROC_BOUNDCLASS_OTHER);

    REQUIRE(Utf8ToUnicode("柴", strlen("柴"), byte_eat, c) == kOk);
    REQUIRE(utf8proc_get_property(c)->boundclass == UTF8PROC_BOUNDCLASS_OTHER);
}

TEST_CASE("grepheme") {
    Character c;
    int byte_len;
    ThisCharacter("🐦‍🔥", 0, c, byte_len);
    for (size_t i = 0; i < c.CodePointCount(); i++) {
        fmt::println("\\U{:08X}", c.Codepoints()[i]);
    }
    ThisCharacter("🐦", 0, c, byte_len);
    for (size_t i = 0; i < c.CodePointCount(); i++) {
        fmt::println("\\U{:08X}", c.Codepoints()[i]);
    }
    ThisCharacter("🔥", 0, c, byte_len);
    for (size_t i = 0; i < c.CodePointCount(); i++) {
        fmt::println("\\U{:08X}", c.Codepoints()[i]);
    }
}