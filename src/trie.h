#pragma once

#include <climits>
#include <string_view>
#include <vector>

namespace mango {

// A printable ascii character trie.
// If insert in 
class Trie {
   public:
    Trie() {}
    ~Trie();

    void Clear();

    void Add(std::string_view seq);
    void Delete(std::string_view seq);

    std::vector<std::string> PrefixWith(std::string_view seq);

   private:
    static constexpr char kStart = 32;
    struct Node {
        Node* nexts[CHAR_MAX - kStart + 1] = {nullptr};
        size_t count = 0;
    };

    void DfsToEveryEnd(std::string& str, Node* node, std::vector<std::string>& res);

    Node root_;
};
}  // namespace mango