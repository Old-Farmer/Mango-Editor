#include "keyseq_manager.h"

#include <stack>

#include "character.h"

namespace mango {

using tm = Terminal;
using ki = Terminal::KeyInfo;
using sk = Terminal::SpecialKey;

// only support ascii keystr
static const std::unordered_map<std::string_view, Terminal::KeyInfo>
    kKeyStrToKeyInfo = {
        // esc
        {"<esc>", {ki::CreateSpecialKey(sk::kEsc)}},
        {"<c-[>", {ki::CreateSpecialKey(sk::kCtrlLsqBracket)}},  // same <esc>
        {"<c-3>", {ki::CreateSpecialKey(sk::kCtrl3)}},           // same <esc>

        // traditioncal ascii control code
        {"<c-~>", {ki::CreateSpecialKey(sk::kCtrlTilde, tm::kCtrl)}},
        {"<c-space>",
         {ki::CreateSpecialKey(
             sk::kCtrlTilde,
             tm::kCtrl)}},  // NOTE:
                            // 1. In Windows Terminal, same as
                            // <c-~>; 2. in alacritty, no effect.
                            // TODO: maybe some detection?

        {"<c-a>", {ki::CreateSpecialKey(sk::kCtrlA, tm::kCtrl)}},
        {"<c-b>", {ki::CreateSpecialKey(sk::kCtrlB, tm::kCtrl)}},
        {"<c-c>", {ki::CreateSpecialKey(sk::kCtrlC, tm::kCtrl)}},
        {"<c-d>", {ki::CreateSpecialKey(sk::kCtrlD, tm::kCtrl)}},
        {"<c-f>", {ki::CreateSpecialKey(sk::kCtrlF, tm::kCtrl)}},
        {"<c-i>", {ki::CreateSpecialKey(sk::kCtrlI, tm::kCtrl)}},
        {"<tab>", {ki::CreateSpecialKey(sk::kTab, tm::kCtrl)}},  // same <c-i>
        {"<c-j>", {ki::CreateSpecialKey(sk::kCtrlJ, tm::kCtrl)}},
        {"<c-k>", {ki::CreateSpecialKey(sk::kCtrlK, tm::kCtrl)}},
        {"<c-l>", {ki::CreateSpecialKey(sk::kCtrlL, tm::kCtrl)}},
        {"<enter>", {ki::CreateSpecialKey(sk::kEnter, tm::kCtrl)}},
        {"<c-m>",
         {ki::CreateSpecialKey(sk::kCtrlM, tm::kCtrl)}},  // same <enter>
        {"<c-n>", {ki::CreateSpecialKey(sk::kCtrlN, tm::kCtrl)}},
        {"<c-p>", {ki::CreateSpecialKey(sk::kCtrlP, tm::kCtrl)}},
        {"<c-q>", {ki::CreateSpecialKey(sk::kCtrlQ, tm::kCtrl)}},
        {"<c-s>", {ki::CreateSpecialKey(sk::kCtrlS, tm::kCtrl)}},
        {"<c-w>", {ki::CreateSpecialKey(sk::kCtrlW, tm::kCtrl)}},
        {"<c-y>", {ki::CreateSpecialKey(sk::kCtrlY, tm::kCtrl)}},
        {"<c-z>", {ki::CreateSpecialKey(sk::kCtrlZ, tm::kCtrl)}},
        {"<bs>", {ki::CreateSpecialKey(sk::kBackspace2, tm::kCtrl)}},
        {"<c-8>", {ki::CreateSpecialKey(sk::kCtrl8, tm::kCtrl)}},  // same <bs>
        {"<s-tab>", {ki::CreateSpecialKey(sk::kBackTab, tm::kCtrl)}},
        {"<space>", {ki::CreateNormalKey(kSpaceChar)}},

        // functional key
        {"<up>", {ki::CreateSpecialKey(sk::kArrowUp)}},
        {"<down>", {ki::CreateSpecialKey(sk::kArrowDown)}},
        {"<left>", {ki::CreateSpecialKey(sk::kArrowLeft)}},
        {"<right>", {ki::CreateSpecialKey(sk::kArrowRight)}},
        {"<c-left>", {ki::CreateSpecialKey(sk::kArrowLeft, tm::kCtrl)}},
        {"<c-right>", {ki::CreateSpecialKey(sk::kArrowRight, tm::kCtrl)}},
        {"<home>", {ki::CreateSpecialKey(sk::kHome)}},
        {"<end>", {ki::CreateSpecialKey(sk::kEnd)}},
        {"<pgup>", {ki::CreateSpecialKey(sk::kPgup)}},
        {"<pgdn>", {ki::CreateSpecialKey(sk::kPgdn)}},
        {"<c-pgup>", {ki::CreateSpecialKey(sk::kPgup, tm::kCtrl)}},
        {"<c-pgdn>", {ki::CreateSpecialKey(sk::kPgdn, tm::kCtrl)}},
};

KeyseqManager::KeyseqManager(Mode& mode) : mode_(mode) {
    MGO_ASSERT(keymap_roots_.size() == static_cast<size_t>(Mode::_COUNT));
    MGO_ASSERT(escape_seq_roots_.size() == static_cast<size_t>(Mode::_COUNT));
}

Result KeyseqManager::ParseKeyseq(const std::string& seq,
                                  std::vector<Terminal::KeyInfo>& keys) {
    int start = -1;
    for (size_t i = 0; i < seq.size(); i++) {
        if (seq[i] == '<') {
            if (start != -1) {
                return kError;
            }
            start = i;
        } else if (seq[i] == '>') {
            if (start == -1) {
                return kError;
            }

            std::string_view key = {seq.c_str() + start, i - start + 1};
            auto iter = kKeyStrToKeyInfo.find(key);
            if (iter == kKeyStrToKeyInfo.end()) {
                std::string key_copy(key);
                throw KeyNotPredefinedException("Didn't predefine %s",
                                                key_copy.c_str());
            }
            keys.push_back(iter->second);
            start = -1;
        } else if (start == -1) {
            keys.push_back(Terminal::KeyInfo::CreateNormalKey(seq[i]));
        }
    }
    return kOk;
}

void KeyseqManager::AddKeyseq(const std::vector<Terminal::KeyInfo>& keys,
                              Keyseq handler, std::vector<Node>& roots,
                              const std::vector<Mode>& modes) {
    for (size_t i = 0; i < modes.size(); i++) {
        Node* node = &roots[static_cast<size_t>(modes[i])];
        for (const Terminal::KeyInfo& key_info : keys) {
            auto iter = node->nexts.find(key_info.ToNumber());
            if (iter == node->nexts.end()) {
                node->nexts[key_info.ToNumber()] = new Node;
            }
            node = node->nexts[key_info.ToNumber()];
        }
        node->end = true;
        if (i == modes.size() - 1) {
            node->handler = std::move(handler);
        } else {
            node->handler = handler;
        }
    }
}

void KeyseqManager::RemoveKeyseq(const std::vector<Terminal::KeyInfo>& keys,
                                 std::vector<Node>& roots,
                                 const std::vector<Mode>& modes) {
    for (Mode mode : modes) {
        Node* node = &roots[static_cast<int>(mode)];
        std::stack<std::pair<Node*, Nexts::iterator>> sta;
        bool to_end = true;
        for (const Terminal::KeyInfo& key_info : keys) {
            auto iter = node->nexts.find(key_info.ToNumber());
            if (iter == node->nexts.end()) {
                to_end = false;
                break;
            }
            sta.push({node, iter});
            node = iter->second;
        }
        if (!to_end || !node->end) {
            continue;
        }
        delete node;
        while (!sta.empty()) {
            auto [node, iter] = sta.top();
            if (node->end) {
                node->nexts.erase(iter);
                break;
            }

            delete node;
            sta.pop();
        }
    }
}

Result KeyseqManager::AddKeymap(const std::string& seq, Keyseq handler,
                                const std::vector<Mode>& modes) {
    std::vector<Terminal::KeyInfo> keys;
    Result res = ParseKeyseq(seq, keys);
    if (res != kOk) {
        return res;
    }
    AddKeyseq(keys, std::move(handler), keymap_roots_, modes);
    return kOk;
}

Result KeyseqManager::RemoveKeymap(const std::string& seq,
                                   const std::vector<Mode>& modes) {
    std::vector<Terminal::KeyInfo> keys;
    Result res = ParseKeyseq(seq, keys);
    if (res != kOk) {
        return res;
    }
    RemoveKeyseq(keys, keymap_roots_, modes);
    return kOk;
}

Result KeyseqManager::AddEscapeSeq(const std::string& seq, Keyseq handler,
                                   const std::vector<Mode>& modes) {
    std::vector<Terminal::KeyInfo> keys;
    Result res = ParseKeyseq(seq, keys);
    if (res != kOk) {
        return res;
    }
    AddKeyseq(keys, std::move(handler), escape_seq_roots_, modes);
    return kOk;
}

Result KeyseqManager::RemoveEscapeSeq(const std::string& seq,
                                      const std::vector<Mode>& modes) {
    std::vector<Terminal::KeyInfo> keys;
    Result res = ParseKeyseq(seq, keys);
    if (res != kOk) {
        return res;
    }
    RemoveKeyseq(keys, escape_seq_roots_, modes);
    return kOk;
}

Result KeyseqManager::FeedKey(const Terminal::KeyInfo& key, Keyseq*& handler,
                              std::vector<Node>& roots, Node*& cur,
                              Mode& last_mode) {
    if (cur == nullptr) {
        cur = &roots[static_cast<int>(mode_)];
        last_mode = mode_;
    } else if (last_mode != mode_) {
        cur = nullptr;
        return kKeyseqError;
    }

    auto iter = cur->nexts.find(key.ToNumber());
    if (iter == cur->nexts.end()) {
        cur = nullptr;
        return kKeyseqError;
    }

    cur = iter->second;
    if (cur->end) {
        handler = &cur->handler;
        cur = nullptr;
        return kKeyseqDone;
    } else {
        return kKeyseqMatched;
    }
}

Result KeyseqManager::FeedKeyForKeymap(const Terminal::KeyInfo& key,
                                       Keyseq*& handler) {
    return FeedKey(key, handler, keymap_roots_, keymap_cur_, keymap_last_mode_);
}

Result KeyseqManager::FeedKeyForEscapeSeq(const Terminal::KeyInfo& key,
                                          Keyseq*& handler) {
    return FeedKey(key, handler, escape_seq_roots_, escape_seq_cur_,
                   escape_seq_last_mode_);
}

}  // namespace mango