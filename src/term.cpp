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
    ret = tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);  // enable mouse
    if (ret != TB_OK) {
        MANGO_LOG_ERROR("%s", tb_strerror(ret));
        throw TermException("%s", tb_strerror(ret));
    }
}

void Terminal::Shutdown() {
    if (!shutdown_) {
        int ret;
        ret = tb_shutdown();
        assert(ret == TB_OK);

        // Restore cursor
        // NOTE: only work on some terminals
        printf("\e[0 q");
        fflush(stdout);

        shutdown_ = true;
    }
}

size_t Terminal::StringWidth(const std::string& str) {
    std::vector<uint32_t> character;
    size_t offset = 0;
    size_t width = 0;
    while (offset < str.size()) {
        int len, c_width;
        Result res = NextCharacterInUtf8(str, offset, character, len, c_width);
        assert(res == kOk);
        width += c_width;
        offset += len;
    }
    return width;
}

}  // namespace mango