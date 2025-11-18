#include "clipboard.h"

#include "character.h"
#include "constants.h"
#include "exception.h"
#include "utils.h"

namespace mango {

std::unique_ptr<ClipBoard> ClipBoard::CreateClipBoard(bool prefer_system) {
    if (prefer_system) {
        if (XClipBoard::DetectUsable()) {
            return std::make_unique<XClipBoard>();
        }
        return std::make_unique<DefaultClipBoard>();
    } else {
        return std::make_unique<DefaultClipBoard>();
    }
}

std::string DefaultClipBoard::GetContent(bool& lines) const {
    lines = lines_;
    return content_;
}
void DefaultClipBoard::SetContent(std::string content, bool lines) {
    MGO_ASSERT(!lines || (lines && content[0] == '\n'));
    lines_ = lines;
    content_ = std::move(content);
}

bool XClipBoard::DetectUsable() {
    const char* const argv[] = {"xsel", "--help", NULL};
    int exit_code;
    try {
        Result res = Exec(argv[0], const_cast<char* const*>(argv), nullptr,
                          nullptr, nullptr, exit_code);
        return exit_code == 0 && res == kOk;
    } catch (OSException& e) {
        return false;
    }
}

XClipBoard::XClipBoard() {
    const char* v = getenv(kWSLEnv);
    in_wsl = v != nullptr;
}

std::string XClipBoard::GetContent(bool& lines) const {
    lines = lines_;
    const char* const argv[] = {"xsel", "--clipboard", NULL};
    std::string content;
    int exit_code;
    try {
        Result res = Exec(argv[0], const_cast<char* const*>(argv), nullptr,
                          &content, nullptr, exit_code);
        if (exit_code != 0 || res != kOk) {
            return "";
        }
    } catch (OSException& e) {
        return "";
    }

    if (content.empty()) {
        return "";
    }

    if (in_wsl) WslFilterCharacter(content);
    if (!CheckUtf8Valid(content)) {
        return "";
    }
    if (lines_) {
        // Assume content is utf-8?
        // TODO: Really?
        if (content[0] != '\n') {
            lines = false;
        }
    }
    return content;
}
void XClipBoard::SetContent(std::string content, bool lines) {
    MGO_ASSERT(!lines || (lines && content[0] == '\n'));
    lines_ = lines;
    const char* const argv[] = {"xsel", "--clipboard", NULL};
    int exit_code;
    try {
        Result res = Exec(argv[0], const_cast<char* const*>(argv), &content,
                          nullptr, nullptr, exit_code);
        if (exit_code != 0 || res != kOk) {
            return;
        }
    } catch (OSException& e) {
        return;
    }
}

void XClipBoard::WslFilterCharacter(std::string& content) {
    auto iter_end = content.end() - 1;
    for (auto iter = content.begin(); iter != iter_end;) {
        if (*iter == '\r' && *(iter + 1) == '\n') {
            iter = content.erase(iter);
        } else {
            iter++;
        }
    }
}

}  // namespace mango