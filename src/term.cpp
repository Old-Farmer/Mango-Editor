#include "term.h"

#include <vector>

#include "keyseq_manager.h"
#include "options.h"

namespace mango {

Terminal::~Terminal() { Shutdown(); }

void Terminal::InitEscKeyseq() {
    esc_keyseq_manager_->AddKeyseq(
        "[200~", {[this] {
            event_.type = static_cast<uint8_t>(EventType::kBracketedPasteOpen);
        }},
        {Mode::kNone});
    esc_keyseq_manager_->AddKeyseq(
        "[201~", {[this] {
            event_.type = static_cast<uint8_t>(EventType::kBracketedPasteClose);
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
    InitEscKeyseq();

    int ret = tb_init();
    if (ret != TB_OK) {
        MGO_LOG_ERROR("{}", tb_strerror(ret));
        throw TermException("{}", tb_strerror(ret));
    }
    init_ = true;

    // NOTE: We use esc mode, so pure codepoint will not have any mod.
    // And enable mouse.
    ret = tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);
    if (ret != TB_OK) {
        MGO_LOG_ERROR("{}", tb_strerror(ret));
        throw TermException("{}", tb_strerror(ret));
    }

    tb_set_wcswidth_fn(my_tb_wcswidth_fn);

    if (global_opts->GetOpt<bool>(kOptTrueColor)) {
        ret = tb_set_output_mode(TB_OUTPUT_TRUECOLOR);
    } else {
        ret = tb_set_output_mode(TB_OUTPUT_NORMAL);
    }
    if (ret != TB_OK) {
        MGO_LOG_ERROR("{}", tb_strerror(ret));
        throw TermException("{}", tb_strerror(ret));
    }
    SetClearAttr(global_opts->GetOpt<ColorScheme>(kOptColorScheme)[kNormal]);

    // Enable Bracketed Paste
    tb_sendf("\e[?2004h");
    tb_present();
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

void Terminal::SetClearAttr(const AttrPair& attr) {
    int ret;
    if ((ret = tb_set_clear_attrs(attr.fg, attr.bg) != TB_OK)) {
        throw TermException("Terminal::SetDefaultAttr error: {}",
                            tb_strerror(ret));
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
            MGO_LOG_ERROR("{}", tb_strerror(ret));
            throw TermException("{}", tb_strerror(ret));
        }
        return true;
    }
}

bool Terminal::Poll(int timeout_ms) {
    // Try pendding events.
    while (!pendding_events_.empty()) {
        event_ = pendding_events_[0];
        pendding_events_.erase(pendding_events_.begin());
        return true;
    }

    bool res = PollInner(timeout_ms);
    if (!res) {
        return false;
    }
    if (event_.type != TB_EVENT_KEY) {
        return true;
    }

    if (event_.key != TB_KEY_ESC) {
        return true;
    }

    HandleEsc();
    return true;
}

// We implement a mechenism for custom escape seq detection on termbox2.
void Terminal::HandleEsc() {
    // We have poll a esc event into event_
    // Now we start detetect if any escape sequence
    pendding_events_.push_back(event_);

    // Try the first following event
    bool res = PollInner(0);
    if (!res) {
        // pop up esc
        event_ = pendding_events_[0];
        pendding_events_.erase(pendding_events_.begin());
        return;
    }

    // Interrupt by mouse event and resize event.
    // Although a resize event always return first when tty
    // and resize event all arrived, we don't care this because we believe we
    // will have chars in buffer if we encounter escape sequence.
    if (event_.type != TB_EVENT_KEY) {
        // pop up esc
        event_ = pendding_events_[0];
        pendding_events_.erase(pendding_events_.begin());
        return;
    }
    pendding_events_.push_back(event_);

    // We have esc event and a key event in left_events
    // and event_ is the key event.
    while (true) {
        Keyseq* handler;
        if (event_.ch > CHAR_MAX) {
            // pop up esc
            event_ = pendding_events_[0];
            pendding_events_.erase(pendding_events_.begin());
            return;
        }
        Result res = esc_keyseq_manager_->FeedKey(EventKeyInfo(), handler);
        if (res == kKeyseqError) {
            // pop up esc
            event_ = pendding_events_[0];
            pendding_events_.erase(pendding_events_.begin());
            return;
        } else if (res == kKeyseqDone) {
            handler->f();
            for (auto iter = pendding_events_.begin();
                 iter != pendding_events_.end();) {
                iter = pendding_events_.erase(iter);
            }
            return;
        }

        // escape seq should be ready in the user space termbox2 buffer
        bool poll_res = PollInner(0);
        if (!poll_res) {
            // pop up esc
            event_ = pendding_events_[0];
            pendding_events_.erase(pendding_events_.begin());
            return;
        }

        pendding_events_.push_back(event_);

        if (event_.type != TB_EVENT_KEY) {
            // pop up esc
            event_ = pendding_events_[0];
            pendding_events_.erase(pendding_events_.begin());
            return;
        }
    }
}

Terminal& Terminal::GetInstance() {
    static Terminal term;
    return term;
}

}  // namespace mango