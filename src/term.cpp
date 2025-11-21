#include "term.h"

#include <vector>

#include "keyseq_manager.h"
#include "options.h"

namespace mango {

Terminal::~Terminal() { Shutdown(); }

void Terminal::InitEscKeyseq(KeyseqManager& m) {
    m.AddKeyseq("[200~", {[this] {
                    event_.type =
                        static_cast<uint8_t>(EventType::kBracketedPasteOpen);
                }},
                {Mode::kNone});
    m.AddKeyseq("[201~", {[this] {
                    event_.type =
                        static_cast<uint8_t>(EventType::kBracketedPasteClose);
                }},
                {Mode::kNone});
}

int my_tb_wcswidth_fn(uint32_t* ch, size_t nch) {
    return CharacterWidth(
        const_cast<Codepoint*>(reinterpret_cast<const Codepoint*>(ch)), nch);
}

void Terminal::Init(GlobalOpts* global_opts) {
    global_opts_ = global_opts;

    esc_keyseq_manager_ = new KeyseqManager(mode_);
    InitEscKeyseq(*esc_keyseq_manager_);

    int ret = tb_init();
    if (ret != TB_OK) {
        MGO_LOG_ERROR("%s", tb_strerror(ret));
        throw TermException("%s", tb_strerror(ret));
    }
    // NOTE: We use esc mode, so pure codepoint will not have any mod.
    // And enable mouse.
    ret = tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);
    if (ret != TB_OK) {
        MGO_LOG_ERROR("%s", tb_strerror(ret));
        throw TermException("%s", tb_strerror(ret));
    }

    tb_set_wcswidth_fn(my_tb_wcswidth_fn);

    // Enable Bracketed Paste
    tb_sendf("\e[?2004h");
    tb_present();
    init_ = true;
}

void Terminal::Shutdown() {
    if (esc_keyseq_manager_) {
        delete esc_keyseq_manager_;
    }

    if (init_) {
        tb_shutdown();

        // Restore cursor
        // NOTE: only work on some terminals
        printf("\e[0 q");
        // Disable Bracketed Paste
        printf("\e[?2004l");
        fflush(stdout);

        init_ = false;
        esc_keyseq_manager_ = nullptr;
    }
}

bool Terminal::PollInner(int timeout_ms) {
    while (true) {
        int ret;
        ret = tb_peek_event(&event_, timeout_ms);
        if (ret == TB_ERR_NO_EVENT) {
            return false;
        } else if (ret == TB_ERR_POLL && tb_last_errno() == EINTR) {
            continue;
        } else if (ret != TB_OK) {
            MGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
        return true;
    }
}

bool Terminal::Poll(int timeout_ms) {
    // Try left events.
    if (!left_events_.empty()) {
        event_ = left_events_[0];
        left_events_.erase(left_events_.begin());
        return true;
    }

    bool res = PollInner(timeout_ms);
    if (!res) {
        return false;
    }
    if (event_.type != TB_EVENT_KEY) {
        return true;
    }

    // We implement a mechenism for custom escape seq detection on termbox2.
    if (event_.key != TB_KEY_ESC) {
        return true;
    }

    left_events_.push_back(event_);

    // Try the first following event
    res = PollInner(0);
    if (!res) {
        // pop up esc
        left_events_.erase(left_events_.begin());
        return true;
    }

    // Interrupt by mouse event and resize event.
    // Although a resize event always return first when tty
    // and resize event all arrived, we don't care this because we believe we
    // will have chars in buffer if we encounter escape sequence.
    if (event_.type != TB_EVENT_KEY) {
        // pop up esc
        event_ = left_events_[0];
        left_events_.erase(left_events_.begin());
        return true;
    }
    left_events_.push_back(event_);

    // We have esc event and a key event in left_events
    // and event_ is the key event.
    while (true) {
        Keyseq* handler;
        if (event_.ch > CHAR_MAX) {
            // pop up esc
            event_ = left_events_[0];
            left_events_.erase(left_events_.begin());
            return true;
        }
        Result res = esc_keyseq_manager_->FeedKey(EventKeyInfo(), handler);
        if (res == kKeyseqError) {
            // pop up esc
            event_ = left_events_[0];
            left_events_.erase(left_events_.begin());
            return true;
        } else if (res == kKeyseqDone) {
            handler->f();
            for (auto iter = left_events_.begin();
                 iter != left_events_.end();) {
                iter = left_events_.erase(iter);
            }
            return true;
        }

        // escape seq should be ready in the user space termbox2 buffer
        bool poll_res = PollInner(0);
        if (!poll_res) {
            // pop up esc
            event_ = left_events_[0];
            left_events_.erase(left_events_.begin());
            return true;
        }

        left_events_.push_back(event_);

        if (event_.type != TB_EVENT_KEY) {
            // pop up esc
            event_ = left_events_[0];
            left_events_.erase(left_events_.begin());
            return true;
        }
    }
    return true;
}

}  // namespace mango