#include "text_tree.h"

#include <gsl/util>

#include "buffer_naive.h"
#include "catch2/catch_test_macros.hpp"
#include "logging.h"

using namespace charxed;

namespace charxed {

size_t LineCntForTest(const std::string& fname) {
    size_t cnt = 1;
    File f(fname, "r");
    Result res;
    do {
        char c;
        res = f.ReadByte(c);
        if (res == Result::kEof) {
            break;
        }
        if (c == '\n') cnt++;
    } while (true);
    return cnt;
}

void SameForTest(const TextTree& tree, BufferNaive& buffer) {
    REQUIRE(buffer.LineCnt() == tree.LineCnt());
    size_t line_cnt = buffer.LineCnt();
    std::string buf;
    for (size_t i = 0; i < line_cnt; i++) {
        auto view = tree.GetLine(i);
        REQUIRE(view.ToStringView(buf) == buffer.GetLine(i));
    }
}

void AddForTest(TextTree& tree, BufferNaive& buffer, Pos pos,
                std::string_view str) {
    Pos hint;
    buffer.Add(pos, str, nullptr, false, hint);
    tree.Add(tree.Find(pos), str);
    REQUIRE(tree.Check() == "");
    SameForTest(tree, buffer);
}

void DeleteForTest(TextTree& tree, BufferNaive& buffer, const Range& range) {
    Pos hint;
    buffer.Delete(range, nullptr, hint);
    tree.Delete(tree.Find(range.begin), tree.Find(range.end));
    REQUIRE(tree.Check() == "");
    SameForTest(tree, buffer);
}

}  // namespace charxed

namespace {
const std::string fname = "../test/test_sample.txt";
}

TEST_CASE("TextTree test") {
    LogInit("text_tree_test.log");
    auto _ = gsl::finally([] { LogDeinit(); });
    Path::GetCwdSys();

    TextTree tree;
    File f(fname, "r");
    EOLSeq eol_seq;
    tree.BulkLoad(f, eol_seq);
    REQUIRE(tree.Check() == "");

    BufferNaive buffer_naive(fname);
    buffer_naive.Load();

    SECTION("Load test") {
        REQUIRE(LineCntForTest(fname) == tree.LineCnt());
        SameForTest(tree, buffer_naive);
    }

    SECTION("OffsetToPos test") {
        const size_t offset = 100;
        Pos pos = tree.OffsetToPos(offset);
        // fmt::print("pos {}:{}", pos.line, pos.line);
        auto iter = tree.Find(pos);
        REQUIRE(iter != tree.End());
        REQUIRE(iter.offset() == offset);
    }

    // All cases below are carefully written to ensure trigger some speical data
    // structure state changes. It is a little bit counter-intuitive because we
    // use a real file. Do not change file content if you know what you are
    // doing.
    // TODO: more intuitive test case?

    SECTION("split node(split point after insert pos)") {
        // trigger spilt, split point is after the insert pos
        AddForTest(tree, buffer_naive, {0, 0}, "i");
        // just insert no, split
        AddForTest(tree, buffer_naive, {0, 1}, "i");
    }

    SECTION("split node(split point is before the insert pos)") {
        AddForTest(tree, buffer_naive, {23, 27}, "i");
    }

    SECTION("split node(split point is in the insert str)") {
        AddForTest(tree, buffer_naive, {12, 61}, std::string(100, 'a'));
    }

    SECTION("merge") {
        // trigger spilt, split point is after the insert pos
        AddForTest(tree, buffer_naive, {0, 0}, "i");
        // trigger merge
        DeleteForTest(tree, buffer_naive, {{0, 0}, {0, 17}});
    }

    SECTION("redistribute") {
        // trigger spilt, split point is after the insert pos
        AddForTest(tree, buffer_naive, {0, 0}, "i");
        // fill the first leaf
        AddForTest(tree, buffer_naive, {0, 0}, std::string(400, 'a'));
        // trigger redistribute
        DeleteForTest(tree, buffer_naive, {{20, 0}, {20, 20}});
    }

    SECTION("large range of deletion/insertion") {
        TextTree::TextView view = {tree.Begin(), tree.End()};
        auto all = view.ToString();
        DeleteForTest(
            tree, buffer_naive,
            {{0, 0},
             {buffer_naive.LineCnt() - 1,
              buffer_naive.GetLine(buffer_naive.LineCnt() - 1).size()}});
        AddForTest(tree, buffer_naive, {0, 0}, all);
    }

    // TODO: more cases
}

TEST_CASE("TextTree load str test") {
    LogInit("text_tree_test.log");
    auto _ = gsl::finally([] { LogDeinit(); });
    Path::GetCwdSys();

    TextTree tree2;
    BufferNaive buffer;
    buffer.Load();

    File f(fname, "r");
    EOLSeq eol_seq;
    auto str = f.ReadAll(eol_seq);

    tree2.BulkLoad(str);
    Pos hint;
    buffer.Add({0, 0}, str, nullptr, false, hint);
    SameForTest(tree2, buffer);
}
