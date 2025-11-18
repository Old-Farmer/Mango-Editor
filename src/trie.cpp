#include "trie.h"

#include <queue>
#include <stack>
#include <string>

#include "utils.h"

namespace mango {

Trie::~Trie() { Clear(); }

void Trie::Clear() {
    std::queue<Node*> q;
    for (int i = 0; i <= CHAR_MAX - kStart; i++) {
        if (root_.nexts[i]) {
            q.push(root_.nexts[i]);
            root_.nexts[i] = nullptr;
        }
    }
    Node* node;
    while (!q.empty()) {
        node = q.front();
        for (int i = 0; i <= CHAR_MAX - kStart; i++) {
            if (root_.nexts[i]) {
                q.push(root_.nexts[i]);
            }
        }
        q.pop();
        delete node;
    }
}

void Trie::Add(std::string_view seq) {
    Node* node = &root_;
    for (char c : seq) {
        Node*& next = node->nexts[c - kStart];
        if (next == nullptr) {
            next = new Node;
        }
        node = next;
    }
    node->count++;
}

void Trie::Delete(std::string_view seq) {
    Node* node = &root_;
    std::stack<std::pair<Node*, int>> sta;
    for (char c : seq) {
        Node* next = node->nexts[c - kStart];
        if (next == nullptr) {
            return;
        }
        sta.push({node, c - kStart});
        node = next;
    }
    if (node->count == 0) {
        return;
    }
    if (--node->count != 0) {
        return;
    }
    for (int i = 0; i <= CHAR_MAX - kStart; i++) {
        if (node->nexts[i]) {
            return;
        }
    }

    delete node;
    while (!sta.empty()) {
        auto [node, nexts_index] = sta.top();
        node->nexts[nexts_index] = nullptr;
        if (node->count != 0) {
            return;
        }
        for (int i = 0; i <= CHAR_MAX - kStart; i++) {
            if (node->nexts[i]) {
                return;
            }
        }

        if (node != &root_) {
            delete node;
        }
        sta.pop();
    }
}

std::vector<std::string> Trie::PrefixWith(std::string_view seq) {
    Node* node = &root_;
    for (char c : seq) {
        Node* next = node->nexts[c - kStart];
        if (next == nullptr) {
            return {};
        }
        node = next;
    }
    MGO_ASSERT(node);

    std::string str(seq);
    std::vector<std::string> ret;
    DfsToEveryEnd(str, node, ret);
    return ret;
}

void Trie::DfsToEveryEnd(std::string& str, Node* node,
                         std::vector<std::string>& res) {
    MGO_ASSERT(node);

    if (node->count > 0) {
        res.push_back(str);
    }

    for (int i = 0; i <= CHAR_MAX - kStart; i++) {
        if (node->nexts[i] == nullptr) {
            continue;
        }
        str.push_back(i + kStart);
        DfsToEveryEnd(str, node->nexts[i], res);
        str.pop_back();
    }
}

}  // namespace mango