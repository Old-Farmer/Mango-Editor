#pragma once

#include <functional>
#include <string>

#include "result.h"
#include "state.h"
#include "term.h"
#include "utils.h"

namespace mango {

struct Keyseq {
    std::string name;
    std::string description;
    int type;
    std::function<void()> f;

    Keyseq() = default;
    Keyseq(std::function<void()> _f, int _type = 0) : type(_type), f(_f) {}
};

class KeyseqManager {
   public:
    KeyseqManager(Mode& mode);
    ~KeyseqManager() = default;
    MGO_DELETE_COPY(KeyseqManager);
    MGO_DELETE_MOVE(KeyseqManager);

    // Add/Remove a keyseq, a keyseq is a key sequence for triggering a defined
    // handler. a keyseq prefixed with another keyseq will be hidden.
    // only ascii charset seq is supported.

    // throws KeyNotPredefinedException if key str is not pre-defined
    // this is always considered a bug and should fixed emidiately
    // return kError if keymap is not well formed
    Result AddKeyseq(const std::string& seq, const Keyseq& handler,
                     const std::vector<Mode>& modes = kDefaultsModes);
    Result RemoveKeyseq(const std::string& seq,
                        const std::vector<Mode>& modes = kDefaultsModes);

    // return kKeyseqError, kKeyseqDone, kKeyseqMatched
    // if kKeyseqDone return, handler will be set to the related handler
    // NOTE: Be careful of the handler lifetime
    Result FeedKey(const Terminal::KeyInfo& key, Keyseq*& handler);

   private:
    struct Node;
    using Nexts = std::unordered_map<size_t, Node*>;
    // use Trie tree to organize keymaps
    struct Node {
        Keyseq handler;
        Nexts nexts;
        bool end = false;

        Node() = default;
        explicit Node(bool _end) : end(_end) {}
    };

    // For simplicity, only ascii charset seq is supported.
    // throws KeyNotPredefinedException if key str is not pre-defined
    // return kError if keymap is not well formed by users
    Result ParseKeyseq(const std::string& seq,
                       std::vector<Terminal::KeyInfo>& keys);

   private:
    std::vector<Node> roots_{static_cast<size_t>(Mode::_kCount),
                             Node{true}};  // one tree per mode

    // keyseq state
    Node* cur_ = nullptr;
    Mode last_mode_;

    Mode& mode_;
};

}  // namespace mango
