#include "term.h"

#include <vector>

#include "character.h"
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

void Terminal::Init(GlobalOpts* global_opts) {
    global_opts_ = global_opts;

    esc_keyseq_manager_ = new KeyseqManager(mode_);
    InitEscKeyseq(*esc_keyseq_manager_);

    int ret = tb_init();
    if (ret != TB_OK) {
        MGO_LOG_ERROR("%s", tb_strerror(ret));
        throw TermException("%s", tb_strerror(ret));
    }
    ret = tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);  // enable mouse
    if (ret != TB_OK) {
        MGO_LOG_ERROR("%s", tb_strerror(ret));
        throw TermException("%s", tb_strerror(ret));
    }

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
    int ret;
    if (timeout_ms == -1) {
        ret = tb_poll_event(&event_);
    } else {
        ret = tb_peek_event(&event_, timeout_ms);
    }
    if (ret == TB_ERR_NO_EVENT) {
        return false;
    } else if (ret == TB_ERR_POLL && tb_last_errno() == EINTR) {
        return false;
    } else if (ret != TB_OK) {
        MGO_LOG_ERROR("%s", tb_strerror(ret));
        throw TermException("%s", tb_strerror(ret));
    }
    return true;
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
    if (!EventIsKey(event_)) {
        return true;
    }

    // We implement a mechenism for custom escape seq detection on termbox2.
    KeyInfo key_info = EventKeyInfo(event_);
    if (!key_info.IsSpecialKey() || key_info.mod != 0 ||
        key_info.special_key != SpecialKey::kEsc) {
        return true;
    }

    left_events_.push_back(event_);

    auto esc_timeout = global_opts_->GetOpt<int64_t>(kOptEscTimeout);
    ;

    // Try the first following event
    auto start = std::chrono::steady_clock::now();
    res = PollInner(esc_timeout);
    if (!res) {
        return false;
    }

    left_events_.push_back(event_);

    // Interrupt by mouse event. Can't be escape seqs.
    if (EventIsMouse(event_)) {
        return false;
    }

    // We meet resize event, because termbox2 resize event return first is tty
    // and resize event all arrived, so we retry until we can meet a key event.
    bool not_timeout = false;
    if (EventIsResize(event_)) {
        auto end = std::chrono::steady_clock::now();
        while (
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count() < esc_timeout) {
            if (!PollInner(esc_timeout)) {
                return false;
            }
            left_events_.push_back(event_);

            // Interrupt by mouse event. Can't be escape seqs.
            if (EventIsMouse(event_)) {
                return false;
            }

            end = std::chrono::steady_clock::now();
            if (EventIsResize(event_)) {
                continue;
            }

            if (EventIsKey(event_)) {
                if (std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                          start)
                        .count() < esc_timeout) {
                    not_timeout = true;
                }
                break;
            }
        }
        if (!not_timeout) {
            return false;
        }
    }

    // We have esc >= 0 resize events and a key event in left_events
    // and event_ is the key event
    while (true) {
        Keyseq* handler;
        Result res =
            esc_keyseq_manager_->FeedKey(EventKeyInfo(event_), handler);
        if (res == kKeyseqError) {
            return false;
        } else if (res == kKeyseqDone) {
            handler->f();
            for (auto iter = left_events_.begin();
                 iter != left_events_.end();) {
                if (EventIsKey(*iter)) {
                    iter = left_events_.erase(iter);
                } else {
                    iter++;
                }
            }
            return true;
        }

        // escape seq should be ready in the user space termbox2 buffer
        bool poll_res = PollInner(0);
        if (!poll_res) {
            return false;
        }

        left_events_.push_back(event_);

        if (!EventIsKey(event_)) {
            return false;
        }
    }
    return true;
}

size_t Terminal::StringWidth(const std::string& str) {
    std::vector<uint32_t> character;
    size_t offset = 0;
    size_t width = 0;
    while (offset < str.size()) {
        int len, c_width;
        Result res = NextCharacterInUtf8(str, offset, character, len, &c_width);
        MGO_ASSERT(res == kOk);
        width += c_width;
        offset += len;
    }
    return width;
}

}  // namespace mango