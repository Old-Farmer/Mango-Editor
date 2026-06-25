#pragma once

#include <vector>

#include "file.h"
#include "pos.h"
#include "utf8.h"
#include "utils.h"

namespace charxed {

// A Rope & B+ tree like data structure which hold text.
// It can find loc, modify text efficiently.
class TextTree {
   public:
    TextTree();
    CHX_DELETE_COPY(TextTree);
    TextTree(TextTree&&) noexcept;
    TextTree& operator=(TextTree&&) noexcept;
    ~TextTree();

    // Should BulkLoad before using.

    // throws IOException, CodingException
    void BulkLoad(File& file, EOLSeq& eol_seq);

    // BulkLoad an empty str just init the class.
    void BulkLoad(std::string_view str);

    // Clear the class, still need BulkLoad before using.
    void Clear();

   private:
    struct ChildInfo {
        size_t new_lines;
        size_t bytes;
    };

    static constexpr size_t kDataSize = 1024;
    static constexpr size_t kChildSize = 16;
    static constexpr size_t kDataSizeMergeThreshold =
        kDataSize / 2 -
        4 * kMaxBytesUtf8Codepoint;  // a little bit smaller than a half because
                                     // we must store codepoint contiously.

    struct InternalNode;

    struct Node {
        bool is_leaf;
        InternalNode* parent;
        size_t bytes;      // byte count belonging to this node
        size_t new_lines;  // new line count belonging to this node

        Node(bool _is_leaf) : is_leaf(_is_leaf) {}
    };

    struct InternalNode : Node {
        size_t size;
        ChildInfo infos[kChildSize + 1];  // leave on slot for convenient split
        Node* children[kChildSize + 1];

        InternalNode() : Node(false) {}
    };

    struct LeafNode : Node {
        LeafNode* next;
        LeafNode* prev;
        char data[kDataSize];

        LeafNode() : Node(true) {}
    };

   public:
    // A codepoint / byte iterator
    class Iterator {
#ifndef NDEBUG
        const TextTree* text_;
#endif
        LeafNode* node_;
        size_t index_;
        size_t offset_;  // A global offset

        friend TextTree;

       public:
        // Implement here in order to inline them.
        // Should Check Iterator validation before Next & Prev, otherwise
        // behavior is undefined.
        // Codepoint and byte ops shouldn't be mixed, unless you know what you
        // are doing. Require that leaf nodes shouldn't be empty unless only one
        // empty leaf in the tree.

        // return a iterator moving forward
        // and codepoint is assigned to the cur codepoint
        Iterator NextCodepoint(Codepoint& codepoint) {
            CHX_ASSERT(*this != text_->End());
            int byte_len;
            Utf8ToUnicode(&node_->data[index_], node_->bytes - index_, byte_len,
                          codepoint);
            Iterator iter = *this;
            iter.index_ += byte_len;
            if (iter.index_ == iter.node_->bytes &&
                iter.node_->next != nullptr) {
                iter.index_ = 0;
                iter.node_ = iter.node_->next;
            }
            iter.offset_ += byte_len;
            return iter;
        }

        // return a iterator moving backward
        // and codepoint is assigned to the prev codepoint
        Iterator PrevCodepoint(Codepoint& codepoint) {
            CHX_ASSERT(*this != text_->Begin());
            Iterator iter = *this;
            if (iter.index_ == 0) {
                iter.node_ = iter.node_->prev;
                iter.index_ = iter.node_->bytes - 1;
            } else {
                iter.index_--;
            }
            for (; !IsUtf8BeginByte(iter.node_->data[iter.index_]);
                 iter.index_--);
            int byte_len;
            Utf8ToUnicode(&iter.node_->data[iter.index_],
                          iter.node_->bytes - iter.index_, byte_len, codepoint);
            iter.offset_ -= byte_len;
            return iter;
        }

        char ThisByte() const {
            CHX_ASSERT(*this != text_->End());
            CHX_ASSERT(index_ < node_->bytes);
            return node_->data[index_];
        }

        // Moving forward one byte.
        void NextByte() {
            CHX_ASSERT(*this != text_->End());
            index_++;
            if (index_ == node_->bytes && node_->next != nullptr) {
                index_ = 0;
                node_ = node_->next;
            }
            offset_++;
        }

        // Moving backward one byte.
        void PrevByte() {
            CHX_ASSERT(*this != text_->Begin());
            if (index_ == 0) {
                node_ = node_->prev;
                CHX_ASSERT(node_->bytes != 0);
                index_ = node_->bytes - 1;
            } else {
                index_--;
            }
            offset_--;
        }

        size_t offset() const { return offset_; }

        // virtually continuous data from this iterator.
        std::string_view MaxContinuousData() {
            return {&node_->data[index_], node_->bytes - index_};
        }

        bool operator==(Iterator other) const {
            CHX_ASSERT(text_ == other.text_);
            return offset_ == other.offset_;
        }
        bool operator!=(Iterator other) const {
            CHX_ASSERT(text_ == other.text_);
            return offset_ != other.offset_;
        }
        bool operator<(Iterator other) const {
            CHX_ASSERT(text_ == other.text_);
            return offset_ < other.offset_;
        };
    };

    // A Block iterator
    class BlockIterator {
#ifndef NDEBUG
        const TextTree* text_;
#endif
        LeafNode* node_;

        friend TextTree;

       public:
        void Next() {
            CHX_ASSERT(*this != text_->BlockEnd());
            node_ = node_->next;
        }
        std::string_view Data() { return {node_->data, node_->bytes}; }

        bool operator==(BlockIterator other) {
            CHX_ASSERT(text_ == other.text_);
            return node_ == other.node_;
        }
        bool operator!=(BlockIterator other) {
            CHX_ASSERT(text_ == other.text_);
            return node_ != other.node_;
        }
    };

    // if pos can't be found, return End()
    // if pos.line >= line cnt, behavior is undefined.
    Iterator Find(Pos pos) const;

    // if offset can't be found, return End()
    Iterator Find(size_t offset) const;

    // Transfer global offset to pos
    // if offset can't be found, return the pos just after the last byte
    Pos OffsetToPos(size_t offset) const;

    Iterator Begin() const {
        Iterator iter;
#ifndef NDEBUG
        iter.text_ = this;
#endif
        iter.node_ = begin_leaf_;
        iter.index_ = 0;
        iter.offset_ = 0;
        return iter;
    }
    Iterator End() const {
        Iterator iter;
#ifndef NDEBUG
        iter.text_ = this;
#endif
        iter.node_ = end_leaf_;
        iter.index_ = end_leaf_->bytes;
        iter.offset_ = root_->bytes;
        return iter;
    }

    BlockIterator BlockBegin() const {
        BlockIterator iter;
#ifndef NDEBUG
        iter.text_ = this;
#endif
        iter.node_ = begin_leaf_;
        return iter;
    }
    BlockIterator BlockEnd() const {
        BlockIterator iter;
#ifndef NDEBUG
        iter.text_ = this;
#endif
        iter.node_ = nullptr;
        return iter;
    }

    // A view of part of text, [begin, end)
    struct TextView {
        Iterator begin;
        Iterator end;

        size_t Size() const {
            CHX_ASSERT(end.offset() >= begin.offset());
            return end.offset() - begin.offset();
        }
        // To std::string_view.
        // If data of TextView is stored contiously, no copy happend.
        // Else buf will be used and data will copied to this buf.
        std::string_view ToStringView(std::string& buf) const;

        std::string ToString() const;
    };

    struct TextViewHash {
        size_t operator()(const TextTree::TextView& v) const noexcept {
            // FNV-1a 64-bit
            uint64_t h = 1469598103934665603ull;
            auto it = v.begin;
            while (it != v.end) {
                h ^= static_cast<unsigned char>(it.ThisByte());
                h *= 1099511628211ull;
                it.NextByte();
            }
            return static_cast<size_t>(h);
        }
    };
    struct TextViewEqual {
        bool operator()(const TextTree::TextView& a,
                        const TextTree::TextView& b) const noexcept {
            if (a.Size() != b.Size()) return false;
            auto ia = a.begin;
            auto ib = b.begin;
            while (ia != a.end) {
                if (ia.ThisByte() != ib.ThisByte()) return false;
                ia.NextByte();
                ib.NextByte();
            }
            return true;
        }
    };

    void Add(Iterator pos, std::string_view str);
    void Delete(Iterator begin, Iterator end);

    // GetLine will not include \n char
    TextView GetLine(size_t line) const {
        auto l_begin = Find({line, 0});
        Iterator l_end;
        if (root_->bytes != 0 && line != LineCnt() - 1) {
            l_end = Find({line + 1, 0});
            l_end.PrevByte();  // skip '\n'
        } else {
            l_end = End();
        }
        return {l_begin, l_end};
    }
    size_t LineCnt() const {
        CHX_ASSERT(root_);
        return root_->new_lines + 1;
    }

    size_t Size() const {
        CHX_ASSERT(root_);
        return root_->bytes;
    }

    // For test
    // TODO: maybe not compile it in release?

    // Print the internal data structure.
    void Print();
    // Check all internal data structure invariants.
    // return empty string if ok, else return a error string
    std::string Check();

   private:
    void Init();

    // Load leaf nodes, all leaf nodes bytes should be >=
    // kDataSizeMergeThreshold.
    // Begin_leaf_ and end_leaf_ will be set. All leaf nodes are unset.
    // Return the the number of all leaf nodes.

    // throws IOException
    size_t LoadLeafNodes(File& file, EOLSeq& eol_seq);

    size_t LoadLeafNodes(std::string_view str);

    // Just used in LoadLeafNodes.
    // sibling bytes >= node bytes
    // and sibling is left to node
    void RedistributeNodes(LeafNode* sibling, LeafNode* node);
    // Fill a internal node during BulidIndex.
    void FillInternalNode(InternalNode* node, const std::vector<Node*>& nodes,
                          size_t nodes_begin_index, size_t size);
    // Build index after leaf nodes loaded.
    void BuildIndex(size_t leaf_cnt);

    void DestoryNode(Node* node);

    void UpdateInfoToRoot(Node* node);

    void AddSplitUptoOneNode(Iterator pos, std::string_view str);
    void DeleteInOneNode(LeafNode* node, size_t begin_index, size_t end_index);

    // Split a leaf
    // Return a new leaf, the orginal leaf will also be changed.
    Node* SplitLeafNode(LeafNode* node, size_t insert_index,
                        std::string_view str);
    // Split an internal node
    // return a new internal node, and the orginal internal node will also be
    // changed.
    Node* SplitInternalNode(InternalNode* node);

    // Try redistribute leaf node data with its siblings
    // return true if redistribution has done and the parent will also be
    // changed to a corresponding state.
    bool TryRedistributeLeafNode(LeafNode* node, size_t index);

    // Merge leaf with sibling node, and the parent will also be
    // changed to a corresponding state.
    void MergeLeafNode(LeafNode* node, size_t index);

    bool TryRedistributeIntenalNode(InternalNode* node, size_t index);
    void MergeInternalNode(InternalNode* node, size_t index);

    Node* root_;  // nullptr means the initial state.
    LeafNode *begin_leaf_, *end_leaf_;
};

}  // namespace charxed
