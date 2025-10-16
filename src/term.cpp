#include "term.h"

#include <vector>

#include "coding.h"

namespace mango {

Terminal::Terminal() { Init(); }
Terminal::~Terminal() { Shutdown(); }

void Terminal::Init() {
    int ret = tb_init();
    if (ret != TB_OK) {
        MANGO_LOG_ERROR("%s", tb_strerror(ret));
        throw TermException("%s", tb_strerror(ret));
    }
    ret = tb_set_input_mode(TB_INPUT_MOUSE);  // enable mouse
    if (ret != TB_OK) {
        MANGO_LOG_ERROR("%s", tb_strerror(ret));
        throw TermException("%s", tb_strerror(ret));
    }
}

void Terminal::Shutdown() {
    if (!shutdown_) {
        // TODO: restore the cursor state for some special terminals(e.g.
        // alacritty)
        int ret = tb_shutdown();
        assert(ret == TB_OK);
        shutdown_ = true;
    }
}

int64_t Terminal::StringWidth(const std::string& str) {
    std::vector<uint32_t> character;
    int64_t offset = 0;
    int64_t width = 0;
    while (offset < str.size()) {
        int len, c_width;
        Result res = NextCharacterInUtf8(str, offset, character, len, c_width);
        assert(res == kOk);
        width += c_width;
    }
    return width;
}

}  // namespace mango