#include "term.h"

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

}  // namespace mango