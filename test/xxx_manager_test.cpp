#include "catch2/catch_test_macros.hpp"
#include "command_manager.h"
#include "keyseq_manager.h"

using namespace mango;

TEST_CASE("keyseq_manager test") {
    Mode mode = Mode::kEdit;
    KeyseqManager manager(mode);

    Keyseq h([] { throw "hey"; });

    std::string seq = "<c-a><c-b><c-c>";
    manager.AddKeyseq(seq, h);
    auto a = Terminal::KeyInfo::CreateSpecialKey(Terminal::SpecialKey::kCtrlA,
                                                 Terminal::kCtrl);
    auto b = Terminal::KeyInfo::CreateSpecialKey(Terminal::SpecialKey::kCtrlB,
                                                 Terminal::kCtrl);
    auto c = Terminal::KeyInfo::CreateSpecialKey(Terminal::SpecialKey::kCtrlC,
                                                 Terminal::kCtrl);
    Keyseq* h2;

    SECTION("FeedKey test") {
        Result res;
        res = manager.FeedKey(a, h2);
        REQUIRE(res == mango::kKeyseqMatched);
        res = manager.FeedKey(b, h2);
        REQUIRE(res == mango::kKeyseqMatched);
        res = manager.FeedKey(c, h2);
        REQUIRE(res == mango::kKeyseqDone);
        REQUIRE_THROWS(h2->f());

        res = manager.FeedKey(a, h2);
        REQUIRE(res == mango::kKeyseqMatched);
        mode = mango::Mode::kFind;
        res = manager.FeedKey(a, h2);
        REQUIRE(res == mango::kKeyseqError);
    }
    SECTION("FeedKey test") {
        Result res;
        res = manager.FeedKey(a, h2);
        REQUIRE(res == mango::kKeyseqMatched);
        res = manager.FeedKey(b, h2);
        REQUIRE(res == mango::kKeyseqMatched);
        res = manager.FeedKey(c, h2);
        REQUIRE(res == mango::kKeyseqDone);
        REQUIRE_THROWS(h2->f());

        res = manager.FeedKey(a, h2);
        REQUIRE(res == mango::kKeyseqMatched);
        mode = mango::Mode::kFind;
        res = manager.FeedKey(a, h2);
        REQUIRE(res == mango::kKeyseqError);
    }


    SECTION("Remove test") {
        manager.RemoveKeyseq(seq);

        Result res;
        res = manager.FeedKey(a, h2);
        REQUIRE(res == mango::kKeyseqError);
    }

    SECTION("override test") {
        manager.AddKeyseq("<c-a>", h);

        Result res;
        res = manager.FeedKey(a, h2);
        REQUIRE(res == mango::kKeyseqDone);
    }

    SECTION("override test 2") {
        manager.AddKeyseq("<c-k><c-i>", h);
        manager.AddKeyseq("<tab>", h);

        auto c_k = Terminal::KeyInfo::CreateSpecialKey(
            Terminal::SpecialKey::kCtrlK, Terminal::kCtrl);
        auto c_i = Terminal::KeyInfo::CreateSpecialKey(
            Terminal::SpecialKey::kCtrlI, Terminal::kCtrl);

        Result res;
        res = manager.FeedKey(c_k, h2);
        REQUIRE(res == mango::kKeyseqMatched);
        res = manager.FeedKey(c_i, h2);
        REQUIRE(res == mango::kKeyseqDone);
    }
}

TEST_CASE("command_manager test") {
    CommandManager m;

    bool a;
    int64_t b;
    std::string c;
    m.AddCommand({"my_command",
                  "",
                  {Type::kBool, Type::kInteger, Type::kString},
                  [&a, &b, &c](CommandArgs args) {
                      a = std::get<bool>(args[0]);
                      b = std::get<int64_t>(args[1]);
                      c = std::get<std::string>(args[2]);
                  },
                  3});

    Command* co;
    CommandArgs args;
    Result res = m.EvalCommand("my_command true 1024 hello", args, co);
    REQUIRE(res == kOk);
    co->f(args);
    REQUIRE(a);
    REQUIRE(b == 1024);
    REQUIRE(c == "hello");

    res = m.EvalCommand("my_command tru 1024 hello", args, co);
    REQUIRE(res == kCommandInvalidArgs);

    res = m.EvalCommand("my_command true a1024 hello", args, co);
    REQUIRE(res == kCommandInvalidArgs);

    res = m.EvalCommand("my_command true 1024 hello dffd", args, co);
    REQUIRE(res == kCommandInvalidArgs);

    m.RemoveCommand("my_command");

    res = m.EvalCommand("my_command true 1024 hello", args, co);
    REQUIRE(res == mango::kCommandNotExist);
}