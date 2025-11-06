#pragma once

#include <functional>

#include "state.h"
#include "term.h"

namespace mango {


struct Keymap {
    std::string name;
    std::string description;
    std::function<void()> f;

    Keymap() = default;
    Keymap(std::function<void()> _f) : f(_f) {}
};

class KeymapManager {
   public:
    KeymapManager(Mode& mode);
    ~KeymapManager() = default;
    MGO_DELETE_COPY(KeymapManager);
    MGO_DELETE_MOVE(KeymapManager);

    // Add/Remove a keymap, a keymap is a key sequence for triggering a defined
    // handler. a keymap prefixed with another keymap will be hidden.
    // only ascii charset seq is supported.

    // throws KeyNotPredefinedException if key str is not pre-defined
    // this is always considered a bug and should fixed emidiately
    // return kError if keymap is not well formed
    Result AddKeymap(const std::string& seq, Keymap handler,
                     const std::vector<Mode>& modes = kDefaultsModes);
    Result RemoveKeymap(const std::string& seq,
                        const std::vector<Mode>& modes = kDefaultsModes);

    // return kKeymapError, kKeymapDone, kKeymapMatched
    // if kKeymapDone return, handler will be set to the related handler
    // NOTE: Be careful of the handler lifetime
    Result FeedKey(const Terminal::KeyInfo& key, Keymap*& handler);

   private:
    // For simplicity, only ascii charset seq is supported.
    // throws KeyNotPredefinedException if key str is not pre-defined
    // return kError if keymap is not well formed by users
    Result ParseKeymap(const std::string& seq,
                       std::vector<Terminal::KeyInfo>& keys);

   private:
    struct Node;
    using Nexts = std::unordered_map<size_t, Node*>;
    // use Trie tree to organize keymaps
    struct Node {
        Keymap handler;
        Nexts nexts;
        bool end = false;

        Node() = default;
        explicit Node(bool _end) : end(_end) {}
    };

    std::vector<Node> roots_{static_cast<size_t>(Mode::_COUNT),
                             Node{true}};  // one tree per mode

    // keymap state
    Node* cur_ = nullptr;
    Mode last_mode_;

    Mode& mode_;
};

}  // namespace mango
