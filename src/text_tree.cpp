#include "text_tree.h"

#include <algorithm>
#include <queue>

#include "exception.h"
#include "logging.h"  // IWYU pragma: keep

namespace charxed {

TextTree::TextTree() : root_(nullptr) {}

TextTree::TextTree(TextTree&& other) noexcept {
    root_ = other.root_;
    begin_leaf_ = other.begin_leaf_;
    end_leaf_ = other.end_leaf_;
    other.root_ = other.begin_leaf_ = other.end_leaf_ = nullptr;
}

TextTree& TextTree::operator=(TextTree&& other) noexcept {
    root_ = other.root_;
    begin_leaf_ = other.begin_leaf_;
    end_leaf_ = other.end_leaf_;
    other.root_ = other.begin_leaf_ = other.end_leaf_ = nullptr;
    return *this;
}

TextTree::~TextTree() { Clear(); }

void TextTree::Init() {
    root_ = new LeafNode;
    root_->bytes = 0;
    root_->new_lines = 0;
    root_->parent = nullptr;
    auto leaf = static_cast<LeafNode*>(root_);
    leaf->next = nullptr;
    leaf->prev = nullptr;
    begin_leaf_ = static_cast<LeafNode*>(root_);
    end_leaf_ = static_cast<LeafNode*>(root_);
}

void TextTree::BulkLoad(File& file, EOLSeq& eol_seq) {
    Clear();
    size_t cnt = LoadLeafNodes(file, eol_seq);
    if (cnt == 0) {
        Init();
        return;
    }
    bool coding_valid = true;
    CHX_ASSERT(begin_leaf_);
    for (LeafNode* n = begin_leaf_; n != nullptr; n = n->next) {
        if (!CheckUtf8Valid({n->data, n->bytes})) {
            coding_valid = false;
            break;
        }
    }
    if (!coding_valid) {
        for (LeafNode* n = begin_leaf_; n != nullptr;) {
            auto next = n->next;
            delete n;
            n = next;
        }
        root_ = nullptr;
        throw CodingException("{}", "utf8 encoding error");
    }
    BuildIndex(cnt);
    // CHX_LOG_DEBUG("loading file: leaf_cnt: {}, bytes: {}, lines: {}", cnt,
    //               root_->bytes, LineCnt());
#ifndef NDEBUG
    auto check_str = Check();
    auto check_c_str = check_str.c_str();
    (void)check_c_str;
    CHX_ASSERT(check_str == "");
#endif
}

void TextTree::BulkLoad(std::string_view str) {
    Clear();
    size_t cnt = LoadLeafNodes(str);
    if (cnt == 0) {
        Init();
        return;
    }
    BuildIndex(cnt);
    // CHX_LOG_DEBUG("loading str: leaf_cnt: {}, bytes: {}, lines: {}", cnt,
    //               root_->bytes, root_->lines);
}

size_t TextTree::LoadLeafNodes(File& file, EOLSeq& eol_seq) {
    eol_seq = EOLSeq::kLF;  // Default

    char c;
    Result res;
    size_t leaf_cnt = 0;
    LeafNode* node = nullptr;
    begin_leaf_ = nullptr;
    bool next_c_valid = false;
    char next_c = -1;
    while (true) {
        if (next_c_valid) {
            c = next_c;
            next_c_valid = false;
        } else {
            try {
                res = file.ReadByte(c);
                if (res == kEof) {
                    break;
                }
                if (c == '\r') {
                    res = file.ReadByte(next_c);
                    if (res == kEof) {
                        break;
                    }
                    if (next_c == '\n') {
                        c = '\n';
                        eol_seq = EOLSeq::kCRLF;
                    } else {
                        next_c_valid = true;
                    }
                }
            } catch (IOException& e) {
                for (LeafNode* n = begin_leaf_; n != nullptr;) {
                    auto next = n->next;
                    delete n;
                    n = next;
                }
                throw;
            }
        }
        if (node == nullptr) {
            node = new LeafNode;
            begin_leaf_ = node;
            node->prev = nullptr;
            node->new_lines = 0;
            node->bytes = 0;
            leaf_cnt++;
        }
        if (node->bytes != kDataSize) {
            node->data[node->bytes] = c;
            node->bytes++;
            node->new_lines += c == '\n' ? 1 : 0;
            continue;
        }
        node->next = new LeafNode;
        node->next->prev = node;
        node = node->next;
        leaf_cnt++;
        if (IsUtf8BeginByte(c)) {
            node->data[0] = c;
            node->bytes = 1;
            node->new_lines = c == '\n' ? 1 : 0;
            continue;
        }

        auto prev = node->prev;
        // Ensure codepoint is continuous in memory.
        int i = prev->bytes - 1;
        for (; i >= 0; i--) {
            if (IsUtf8BeginByte(prev->data[i])) break;
        }
        memcpy(node->data, prev->data + i, prev->bytes - i);
        node->bytes = prev->bytes - i;
        prev->bytes = i;
        node->data[node->bytes] = c;
        node->bytes++;
        node->new_lines = 0;  // '\n' can't be multi-byte
    }
    if (node == nullptr) {
        return 0;
    }
    end_leaf_ = node;
    node->next = nullptr;
    // Because all leaf nodes should be at least half full, we should ensure
    // this manually by redistribute data between the last two leaf nodes.
    if (end_leaf_->prev == nullptr) {
        return leaf_cnt;
    }
    if (end_leaf_->bytes >= kDataSizeMergeThreshold) {
        return leaf_cnt;
    }
    RedistributeNodes(end_leaf_->prev, end_leaf_);
    return leaf_cnt;
}

size_t TextTree::LoadLeafNodes(std::string_view str) {
    if (str.empty()) {
        return 0;
    }

    size_t i = 0;
    size_t leaf_cnt = 0;
    LeafNode* node;
    while (i != str.size()) {
        if (leaf_cnt == 0) {
            node = new LeafNode;
            node->prev = nullptr;
            begin_leaf_ = node;
        } else {
            node->next = new LeafNode;
            node->next->prev = node;
            node = node->next;
        }
        int64_t e = i + kDataSize > str.size() ? str.size() : i + kDataSize;
        for (; e < static_cast<int64_t>(str.size()) && e >= 0; e--) {
            if (IsUtf8BeginByte(str[e])) break;
        }
        memcpy(node->data, str.data() + i, e - i);
        node->bytes = e - i;
        node->new_lines = std::count(str.data() + i, str.data() + e, '\n');
        leaf_cnt++;
        i = e;
    }
    end_leaf_ = node;
    node->next = nullptr;
    // Because all leaf nodes should be at least half full, we should ensure
    // this manually by redistribute data between the last two leaf nodes.
    if (end_leaf_->prev == nullptr) {
        return leaf_cnt;
    }
    if (end_leaf_->bytes >= kDataSizeMergeThreshold) {
        return leaf_cnt;
    }
    RedistributeNodes(end_leaf_->prev, end_leaf_);
    return leaf_cnt;
}

// Ref TryRedistributeLeafNode
void TextTree::RedistributeNodes(LeafNode* sibling, LeafNode* node) {
    size_t bytes = sibling->bytes + node->bytes;
    size_t redist_i = bytes / 2;
    for (; redist_i < sibling->bytes; redist_i++) {
        if (IsUtf8BeginByte(sibling->data[redist_i])) break;
    }
    memmove(node->data + sibling->bytes - redist_i, node->data, node->bytes);
    memcpy(node->data, sibling->data + redist_i, sibling->bytes - redist_i);
    size_t moved_lines = std::count(sibling->data + redist_i,
                                    sibling->data + sibling->bytes, '\n');
    node->bytes += sibling->bytes - redist_i;
    node->new_lines += moved_lines;
    sibling->bytes = redist_i;
    sibling->new_lines -= moved_lines;
}

void TextTree::FillInternalNode(InternalNode* node,
                                const std::vector<Node*>& nodes,
                                size_t nodes_begin_index, size_t size) {
    node->bytes = 0;
    node->new_lines = 0;
    for (size_t i = 0; i < size; i++) {
        node->infos[i] = {nodes[nodes_begin_index + i]->new_lines,
                          nodes[nodes_begin_index + i]->bytes};
        node->new_lines += node->infos[i].new_lines;
        node->bytes += node->infos[i].bytes;
        node->children[i] = nodes[nodes_begin_index + i];
        nodes[nodes_begin_index + i]->parent = node;
    }
    node->size = size;
}

void TextTree::BuildIndex(size_t leaf_cnt) {
    if (leaf_cnt == 1) {
        root_ = begin_leaf_;
        root_->parent = nullptr;
        return;
    }

    size_t single_level_intenal_cnt = leaf_cnt / kChildSize;
    size_t remain = leaf_cnt % kChildSize;
    std::vector<Node*> nodes;
    std::vector<Node*> upper_level_nodes;
    nodes.reserve(leaf_cnt);
    upper_level_nodes.reserve(single_level_intenal_cnt);
    for (auto n = begin_leaf_; n != nullptr; n = n->next) {
        nodes.push_back(n);
    }
    while (single_level_intenal_cnt != 0) {
        if (remain == 0) {
            for (size_t i = 0; i < single_level_intenal_cnt; i++) {
                auto intenal = new InternalNode;
                FillInternalNode(intenal, nodes, i * kChildSize, kChildSize);
                upper_level_nodes.push_back(intenal);
            }
        } else {
            size_t stop_i = single_level_intenal_cnt + 1 - 2;
            for (size_t i = 0; i < stop_i; i++) {
                auto intenal = new InternalNode;
                FillInternalNode(intenal, nodes, i * kChildSize, kChildSize);
                upper_level_nodes.push_back(intenal);
            }

            // Last two internal nodes, we redistribute them.
            size_t size = (kChildSize + remain) / 2;
            auto intenal = new InternalNode;
            FillInternalNode(intenal, nodes, stop_i * kChildSize, size);
            upper_level_nodes.push_back(intenal);

            intenal = new InternalNode;
            size_t begin_index = stop_i * kChildSize + size;
            size = kChildSize + remain - size;
            FillInternalNode(intenal, nodes, begin_index, size);
            upper_level_nodes.push_back(intenal);
        }

        std::swap(nodes, upper_level_nodes);
        single_level_intenal_cnt = nodes.size() / kChildSize;
        remain = nodes.size() % kChildSize;
        upper_level_nodes.clear();
    }

    if (remain == 1) {
        root_ = nodes[0];
        root_->parent = nullptr;
        return;
    }
    root_ = new InternalNode;
    FillInternalNode(static_cast<InternalNode*>(root_), nodes, 0, remain);
    root_->parent = nullptr;
}

void TextTree::DestoryNode(Node* node) {
    if (node->is_leaf) {
        delete node;
        return;
    }
    auto internal = static_cast<InternalNode*>(node);
    for (size_t i = 0; i < internal->size; i++) {
        DestoryNode(internal->children[i]);
    }
    delete node;
}

void TextTree::Clear() {
    if (root_ == nullptr) {
        return;
    }

    if (root_->is_leaf) {
        delete root_;
        root_ = nullptr;
        return;
    }

    // We use B+-tree like tree, so the tree height is always quite small.
    // It is a good situation to use recursion and the code is clean.
    DestoryNode(root_);
    root_ = nullptr;
}

TextTree::Iterator TextTree::Find(Pos pos) const {
    Node* node = root_;
    size_t acc_lines = 0;
    size_t acc_bytes = 0;
    while (!node->is_leaf) {
        auto internal = static_cast<InternalNode*>(node);
        size_t i = 0;
        for (; i < internal->size &&
               acc_lines + internal->infos[i].new_lines < pos.line;
             i++) {
            acc_lines += internal->infos[i].new_lines;
            acc_bytes += internal->infos[i].bytes;
        }
        CHX_ASSERT(i < internal->size);
        node = internal->children[i];
    }

    auto leaf = static_cast<LeafNode*>(node);

    CHX_ASSERT(acc_lines <= pos.line);
    // Find line
    size_t i = 0;
    while (acc_lines != pos.line) {
        for (; i < leaf->bytes && acc_lines != pos.line; i++) {
            acc_lines += leaf->data[i] == '\n' ? 1 : 0;
            acc_bytes++;
        }
        if (i == leaf->bytes) {
            leaf = leaf->next;
            i = 0;
        }
    }

    // Find byte_offset
    size_t acc_byte_offset = 0;
    while (true) {
        if (leaf == nullptr) {
            // return end only when after the last byte of the file
            // CHX_ASSERT(acc_byte_offset == pos.byte_offset);
            return End();
        }
        if (leaf->bytes - i > pos.byte_offset - acc_byte_offset) {
            i += pos.byte_offset - acc_byte_offset;
            break;
        }
        acc_byte_offset += leaf->bytes - i;
        leaf = leaf->next;
        i = 0;
    }
    acc_bytes += pos.byte_offset;

    Iterator iter;
#ifndef NDEBUG
    iter.text_ = this;
#endif
    iter.node_ = leaf;
    iter.index_ = i;
    iter.offset_ = acc_bytes;
    return iter;
}

TextTree::Iterator TextTree::Find(size_t offset) const {
    Node* node = root_;
    size_t acc_bytes = 0;
    while (!node->is_leaf) {
        auto internal = static_cast<InternalNode*>(node);
        size_t i = 0;
        for (; i < internal->size &&
               acc_bytes + internal->infos[i].bytes <= offset;
             i++) {
            acc_bytes += internal->infos[i].bytes;
        }
        if (i == internal->size) {
            return End();
        }
        node = internal->children[i];
    }

    auto leaf = static_cast<LeafNode*>(node);

    Iterator iter;
#ifndef NDEBUG
    iter.text_ = this;
#endif
    iter.node_ = leaf;
    iter.index_ = offset - acc_bytes;
    iter.offset_ = offset;
    return iter;
}

Pos TextTree::OffsetToPos(size_t offset) const {
    Node* node = root_;
    size_t acc_bytes = 0;
    size_t acc_lines = 0;
    while (!node->is_leaf) {
        auto internal = static_cast<InternalNode*>(node);
        size_t i = 0;
        for (; i < internal->size &&
               acc_bytes + internal->infos[i].bytes <= offset;
             i++) {
            acc_bytes += internal->infos[i].bytes;
            acc_lines += internal->infos[i].new_lines;
        }
        if (i == internal->size) {
            auto iter = Find({LineCnt() - 1, 0});
            return {LineCnt() - 1, root_->bytes - iter.offset()};
        }
        node = internal->children[i];
    }

    auto leaf = static_cast<LeafNode*>(node);

    size_t cur_node_lines = 0;
    size_t byte_offset = 0;
    const size_t left_bytes = offset - acc_bytes;
    CHX_ASSERT((leaf == end_leaf_ && left_bytes <= leaf->bytes) ||
               left_bytes < leaf->bytes);
    for (size_t i = 0; i < left_bytes; i++) {
        if (leaf->data[i] == '\n') {
            cur_node_lines++;
            byte_offset = 0;
        } else {
            byte_offset++;
        }
    }
    // We find '\n' in this leaf, meaning byte_offset is correct
    if (cur_node_lines > 0) {
        return {cur_node_lines + acc_lines, byte_offset};
    }

    // If we can't find any '\n' in this node, we should search leaves backward
    // to get the actul byte_offset.
    leaf = leaf->prev;
    while (leaf) {
        CHX_ASSERT(leaf->bytes != 0);
        for (int64_t i = leaf->bytes - 1; i >= 0; i--) {
            if (leaf->data[i] == '\n') {
                break;
            }
            byte_offset++;
        }
        leaf = leaf->prev;
    }
    return {acc_lines, byte_offset};
}

// If str is too large, we split it too some substrs and insert it to the trees
// one by one.
void TextTree::Add(Iterator pos, std::string_view str) {
    if (str.size() <= kDataSize - kMaxBytesUtf8Codepoint) {
        AddSplitUptoOneNode(pos, str);
        return;
    }

    size_t begin = 0;
    size_t end = begin + kDataSize - kMaxBytesUtf8Codepoint;
    for (int64_t i = end; i >= static_cast<int64_t>(begin); i--) {
        if (IsUtf8BeginByte(str[i])) {
            end = i;
            break;
        }
    }
    AddSplitUptoOneNode(pos, str.substr(begin, end - begin));

    // maintain an global offset insert point to insert substrs one by one.
    size_t offset = pos.offset() + end - begin;
    begin = end;
    while (true) {
        auto iter = Find(offset);
        if (str.size() - begin <= kDataSize - kMaxBytesUtf8Codepoint) {
            AddSplitUptoOneNode(iter, str.substr(begin));
            break;
        }

        end = begin + kDataSize - kMaxBytesUtf8Codepoint;
        for (int64_t i = end; i >= static_cast<int64_t>(begin); i--) {
            if (IsUtf8BeginByte(str[i])) {
                end = i;
                break;
            }
        }
        AddSplitUptoOneNode(iter, str.substr(begin, end - begin));
        offset += end - begin;
        begin = end;
    }
}

// If range is crossed multiple leaf node, we split the range and delete them
// backwards.
void TextTree::Delete(Iterator begin, Iterator end) {
    // Iterator should be on a specific real byte if it is gen by Find.
    // but end has exclude semantic so we can tweak it to the prev node if
    // index_ == 0 whenever possible for quicker deletions.
    if (end.index_ == 0 && end.node_->prev != nullptr) {
        end.node_ = end.node_->prev;
        end.index_ = end.node_->bytes;
    }
    if (begin.node_ == end.node_) {
        DeleteInOneNode(begin.node_, begin.index_, end.index_);
        return;
    }

    size_t offset_begin = begin.offset();
    begin = end;
    begin.index_ = 0;
    begin.offset_ = end.offset() - end.index_;
    DeleteInOneNode(begin.node_, begin.index_, end.index_);
    CHX_ASSERT(Check() == "");

    size_t offset_end = begin.offset();
    while (true) {
        begin = Find(offset_begin);
        end = Find(offset_end);
        if (end.index_ == 0 && end.node_->prev != nullptr) {
            end.node_ = end.node_->prev;
            end.index_ = end.node_->bytes;
        }
        if (begin.node_ == end.node_) {
            DeleteInOneNode(begin.node_, begin.index_, end.index_);
            CHX_ASSERT(Check() == "");
            return;
        }

        begin = end;
        begin.index_ = 0;
        begin.offset_ = end.offset() - end.index_;
        DeleteInOneNode(begin.node_, begin.index_, end.index_);
        offset_end = begin.offset();
#ifndef NDEBUG
        auto check_str = Check();
        auto check_c_str = check_str.c_str();
        (void)check_c_str;
        CHX_ASSERT(check_str == "");
#endif
    }
}

void TextTree::UpdateInfoToRoot(Node* node) {
    InternalNode* internal;
    while (node != root_) {
        internal = static_cast<InternalNode*>(node->parent);
        size_t i = std::find(internal->children,
                             internal->children + internal->size, node) -
                   internal->children;
        CHX_ASSERT(i < kChildSize);
        internal->new_lines +=
            static_cast<int64_t>(node->new_lines) -
            static_cast<int64_t>(internal->infos[i].new_lines);
        internal->bytes += static_cast<int64_t>(node->bytes) -
                           static_cast<int64_t>(internal->infos[i].bytes);
        internal->infos[i] = {node->new_lines, node->bytes};
        node = internal;
    }
}

void TextTree::AddSplitUptoOneNode(Iterator pos, std::string_view str) {
    size_t bytes = pos.node_->bytes + str.size();
    if (bytes <= kDataSize) {
        memmove(pos.node_->data + pos.index_ + str.size(),
                pos.node_->data + pos.index_, pos.node_->bytes - pos.index_);
        memcpy(pos.node_->data + pos.index_, str.data(), str.size());
        pos.node_->bytes = bytes;
        pos.node_->new_lines += std::count(str.begin(), str.end(), '\n');
        UpdateInfoToRoot(pos.node_);
        return;
    }

    Node* node = pos.node_;
    Node* new_node = SplitLeafNode(pos.node_, pos.index_, str);
    InternalNode* internal = pos.node_->parent;
    while (internal) {
        size_t i = std::find(internal->children,
                             internal->children + internal->size, node) -
                   internal->children;
        CHX_ASSERT(i < kChildSize);
        internal->new_lines +=
            static_cast<int64_t>(node->new_lines + new_node->new_lines) -
            static_cast<int64_t>(internal->infos[i].new_lines);
        internal->bytes += static_cast<int64_t>(node->bytes + new_node->bytes) -
                           static_cast<int64_t>(internal->infos[i].bytes);
        internal->infos[i] = {node->new_lines, node->bytes};
        std::move(internal->infos + i + 1, internal->infos + internal->size,
                  internal->infos + i + 2);
        std::move(internal->children + i + 1,
                  internal->children + internal->size,
                  internal->children + i + 2);
        internal->infos[i + 1] = {new_node->new_lines, new_node->bytes};
        internal->children[i + 1] = new_node;
        internal->size++;
        if (internal->size <= kChildSize) {
            UpdateInfoToRoot(internal);
            return;
        }
        new_node = SplitInternalNode(internal);
        node = internal;
        internal = node->parent;
    }

    // root is splited
    root_ = new InternalNode;
    root_->parent = nullptr;
    root_->new_lines = node->new_lines + new_node->new_lines;
    root_->bytes = node->bytes + new_node->bytes;

    internal = static_cast<InternalNode*>(root_);
    internal->size = 2;
    internal->infos[0] = {node->new_lines, node->bytes};
    internal->infos[1] = {new_node->new_lines, new_node->bytes};
    internal->children[0] = node;
    internal->children[1] = new_node;
    node->parent = internal;
    new_node->parent = internal;
}

void TextTree::DeleteInOneNode(LeafNode* node, size_t begin_index,
                               size_t end_index) {
    size_t bytes = node->bytes - (end_index - begin_index);
    node->new_lines -=
        std::count(node->data + begin_index, node->data + end_index, '\n');
    memmove(node->data + begin_index, node->data + end_index,
            node->bytes - end_index);
    node->bytes = bytes;
    if (bytes >= kDataSizeMergeThreshold) {
        UpdateInfoToRoot(node);
        return;
    }

    auto internal = node->parent;
    if (!internal) {  // when leaf is the root
        return;
    }

    size_t i = std::find(internal->children,
                         internal->children + internal->size, node) -
               internal->children;
    CHX_ASSERT(i != internal->size);
    internal->new_lines -= internal->infos[i].new_lines - node->new_lines;
    internal->bytes -= internal->infos[i].bytes - node->bytes;
    // We don't update internal->infos[i] here becuse redistribute or merge will
    // update it.
    if (TryRedistributeLeafNode(node, i)) {
        UpdateInfoToRoot(internal);
        return;
    }
    MergeLeafNode(node, i);
    if (internal->size >= kChildSize / 2) {
        UpdateInfoToRoot(internal);
        return;
    }

    auto p = internal->parent;
    while (p) {
        i = std::find(p->children, p->children + p->size, internal) -
            p->children;
        CHX_ASSERT(i != internal->size);
        p->new_lines -= p->infos[i].new_lines - internal->new_lines;
        p->bytes -= p->infos[i].bytes - internal->bytes;
        if (TryRedistributeIntenalNode(internal, i)) {
            UpdateInfoToRoot(p);
            return;
        }
        MergeInternalNode(internal, i);
        if (p->size >= kChildSize / 2) {
            UpdateInfoToRoot(p);
            return;
        }
        internal = p;
        p = internal->parent;
    }

    // internal == root_
    if (internal->size > 1) {
        return;
    }
    auto tmp = root_;
    root_ = internal->children[0];
    root_->parent = nullptr;
    delete tmp;
}

TextTree::Node* TextTree::SplitLeafNode(LeafNode* node, size_t insert_index,
                                        std::string_view str) {
    size_t bytes = node->bytes + str.size();
    auto new_leaf = new LeafNode;
    new_leaf->parent = node->parent;

    new_leaf->next = node->next;
    new_leaf->prev = node;
    if (node->next) {
        node->next->prev = new_leaf;
    } else {
        end_leaf_ = new_leaf;
    }
    node->next = new_leaf;

#ifndef NDEBUG
    size_t _total_lines =
        node->new_lines + std::count(str.begin(), str.end(), '\n');
    size_t _total_bytes = node->bytes + str.size();
#endif

    if (insert_index >= bytes / 2) {  // split before the str
        // CHX_LOG_DEBUG("before");
        size_t split_i = bytes / 2;
        for (; split_i < insert_index; split_i++) {
            if (IsUtf8BeginByte(node->data[split_i])) break;
        }
        memcpy(new_leaf->data, node->data + split_i, insert_index - split_i);
        memcpy(new_leaf->data + insert_index - split_i, str.data(), str.size());
        memcpy(new_leaf->data + insert_index - split_i + str.size(),
               node->data + insert_index, node->bytes - insert_index);
        new_leaf->bytes = node->bytes - split_i + str.size();
        size_t old_leaf_lines =
            std::count(node->data, node->data + split_i, '\n');
        new_leaf->new_lines = node->new_lines - old_leaf_lines +
                              std::count(str.begin(), str.end(), '\n');
        node->bytes = split_i;
        node->new_lines = old_leaf_lines;
    } else if (insert_index + str.size() < bytes / 2) {  // split after the str
        // CHX_LOG_DEBUG("after");
        size_t split_pos =
            insert_index + (bytes / 2 - str.size() - insert_index);
        for (; split_pos < node->bytes; split_pos++) {
            if (IsUtf8BeginByte(node->data[split_pos])) break;
        }
        memcpy(new_leaf->data, node->data + split_pos, node->bytes - split_pos);
        new_leaf->bytes = node->bytes - split_pos;
        new_leaf->new_lines =
            std::count(new_leaf->data, new_leaf->data + new_leaf->bytes, '\n');
        memmove(node->data + insert_index + str.size(),
                node->data + insert_index, split_pos - insert_index);
        memcpy(node->data + insert_index, str.data(), str.size());
        node->bytes = node->bytes + str.size() - new_leaf->bytes;
        node->new_lines = node->new_lines +
                          std::count(str.begin(), str.end(), '\n') -
                          new_leaf->new_lines;
    } else {  // split the str
        // CHX_LOG_DEBUG("in");
        size_t split_pos = bytes / 2 - insert_index;
        for (; split_pos < str.size(); split_pos++) {
            if (IsUtf8BeginByte(str[split_pos])) break;
        }
        memcpy(new_leaf->data, str.data() + split_pos, str.size() - split_pos);
        memcpy(new_leaf->data + str.size() - split_pos,
               node->data + insert_index, node->bytes - insert_index);
        new_leaf->bytes = str.size() - split_pos + node->bytes - insert_index;
        size_t line_cnt = std::count(new_leaf->data + str.size() - split_pos,
                                     new_leaf->data + new_leaf->bytes, '\n');
        new_leaf->new_lines =
            std::count(new_leaf->data, new_leaf->data + str.size() - split_pos,
                       '\n') +
            line_cnt;
        memcpy(node->data + insert_index, str.data(), split_pos);
        node->bytes = insert_index + split_pos;
        node->new_lines =
            std::count(str.begin(), str.begin() + split_pos, '\n') +
            node->new_lines - line_cnt;
    }

    CHX_ASSERT(_total_lines == node->new_lines + new_leaf->new_lines);
    CHX_ASSERT(_total_bytes == node->bytes + new_leaf->bytes);
    CHX_ASSERT(node->bytes <= kDataSize &&
               node->bytes >= kDataSizeMergeThreshold);
    CHX_ASSERT(new_leaf->bytes <= kDataSize &&
               new_leaf->bytes >= kDataSizeMergeThreshold);

    return new_leaf;
}

TextTree::Node* TextTree::SplitInternalNode(InternalNode* node) {
    auto new_node = new InternalNode;
    new_node->parent = node->parent;
    std::move(node->infos + kChildSize / 2 + 1, node->infos + kChildSize + 1,
              new_node->infos);
    std::move(node->children + kChildSize / 2 + 1,
              node->children + kChildSize + 1, new_node->children);
    new_node->size = (kChildSize + 1) - (kChildSize / 2 + 1);
    new_node->new_lines = 0;
    new_node->bytes = 0;
    for (size_t i = 0; i < new_node->size; i++) {
        new_node->new_lines += new_node->infos[i].new_lines;
        new_node->bytes += new_node->infos[i].bytes;
    }
    node->size -= new_node->size;
    node->new_lines -= new_node->new_lines;
    node->bytes -= new_node->bytes;
    for (size_t i = 0; i < new_node->size; i++) {
        new_node->children[i]->parent = new_node;
    }
    CHX_ASSERT(new_node->size + node->size == kChildSize + 1);
    CHX_ASSERT(new_node->size >= kChildSize / 2 &&
               new_node->size <= kChildSize);
    CHX_ASSERT(node->size >= kChildSize / 2 && node->size <= kChildSize);
    return new_node;
}

bool TextTree::TryRedistributeLeafNode(LeafNode* node, size_t index) {
    // CHX_LOG_DEBUG("redistribute");
    // First try left sibling
    auto p = node->parent;
    if (index != 0) {
        auto sibling = static_cast<LeafNode*>(p->children[index - 1]);
        size_t bytes = sibling->bytes + node->bytes;
        // CHX_LOG_DEBUG("left redis: {}", bytes);
        if (bytes > kDataSize) {
#ifndef NDEBUG
            size_t _total_bytes = node->bytes + sibling->bytes;
            size_t _total_lines = node->new_lines + sibling->new_lines;
#endif
            size_t redst_i = bytes / 2;
            for (; redst_i < sibling->bytes; redst_i++) {
                if (IsUtf8BeginByte(sibling->data[redst_i])) break;
            }
            memmove(node->data + sibling->bytes - redst_i, node->data,
                    node->bytes);
            memcpy(node->data, sibling->data + redst_i,
                   sibling->bytes - redst_i);
            size_t moved_lines = std::count(
                sibling->data + redst_i, sibling->data + sibling->bytes, '\n');
            node->bytes += sibling->bytes - redst_i;
            node->new_lines += moved_lines;
            sibling->bytes = redst_i;
            sibling->new_lines -= moved_lines;
            p->infos[index] = {node->new_lines, node->bytes};
            p->infos[index - 1] = {sibling->new_lines, sibling->bytes};

            CHX_ASSERT(node->bytes >= kDataSizeMergeThreshold &&
                       node->bytes <= kDataSize);
            CHX_ASSERT(sibling->bytes >= kDataSizeMergeThreshold &&
                       sibling->bytes <= kDataSize);
            CHX_ASSERT(_total_bytes == node->bytes + sibling->bytes);
            CHX_ASSERT(_total_lines == node->new_lines + sibling->new_lines);
            return true;
        }
    }
    // Then try right sibling
    if (index != p->size - 1) {
        auto sibling = static_cast<LeafNode*>(p->children[index + 1]);
        size_t bytes = sibling->bytes + node->bytes;
        // CHX_LOG_DEBUG("right redis: {}", bytes);
        if (bytes > kDataSize) {
#ifndef NDEBUG
            size_t _total_bytes = node->bytes + sibling->bytes;
            size_t _total_lines = node->new_lines + sibling->new_lines;
#endif
            size_t redst_i = bytes / 2 - node->bytes;
            for (; redst_i < sibling->bytes; redst_i++) {
                if (IsUtf8BeginByte(sibling->data[redst_i])) break;
            }
            size_t moved_lines =
                std::count(sibling->data, sibling->data + redst_i, '\n');
            memcpy(node->data + node->bytes, sibling->data, redst_i);
            memmove(sibling->data, sibling->data + redst_i,
                    sibling->bytes - redst_i);
            node->bytes += redst_i;
            node->new_lines += moved_lines;
            sibling->bytes -= redst_i;
            sibling->new_lines -= moved_lines;
            p->infos[index] = {node->new_lines, node->bytes};
            p->infos[index + 1] = {sibling->new_lines, sibling->bytes};

            CHX_ASSERT(node->bytes >= kDataSizeMergeThreshold &&
                       node->bytes <= kDataSize);
            CHX_ASSERT(sibling->bytes >= kDataSizeMergeThreshold &&
                       sibling->bytes <= kDataSize);
            CHX_ASSERT(_total_bytes == node->bytes + sibling->bytes);
            CHX_ASSERT(_total_lines == node->new_lines + sibling->new_lines);
            return true;
        }
    }
    return false;
}

void TextTree::MergeLeafNode(LeafNode* node, size_t index) {
    // CHX_LOG_DEBUG("merge");
    int64_t merged_i = -1;
    auto p = node->parent;
    CHX_ASSERT(p->children[index] == node);
    // First try left sibling
    if (index != 0 && p->infos[index - 1].bytes + node->bytes <= kDataSize) {
        // CHX_LOG_DEBUG("left merge: {} {}", p->infos[index - 1].bytes,
        //               node->bytes);
        CHX_ASSERT(p->infos[index - 1].bytes == p->children[index - 1]->bytes);
        merged_i = index - 1;
    }
    // Then try right sibling
    if (merged_i == -1 && index != node->parent->size - 1 &&
        p->infos[index + 1].bytes + node->bytes <= kDataSize) {
        // CHX_LOG_DEBUG("right merge: {} {}", p->infos[index + 1].bytes,
        //               node->bytes);
        CHX_ASSERT(p->infos[index + 1].bytes == p->children[index + 1]->bytes);
        merged_i = index;
    }
    CHX_ASSERT(merged_i != -1);

    // Merge them
    auto merged_leaf = static_cast<LeafNode*>(p->children[merged_i]);
    auto another_leaf = static_cast<LeafNode*>(p->children[merged_i + 1]);
#ifndef NDEBUG
    size_t _total_bytes = merged_leaf->bytes + another_leaf->bytes;
    size_t _total_lines = merged_leaf->new_lines + another_leaf->new_lines;
#endif
    memcpy(merged_leaf->data + merged_leaf->bytes, another_leaf->data,
           another_leaf->bytes);
    merged_leaf->new_lines += another_leaf->new_lines;
    merged_leaf->bytes += another_leaf->bytes;

    // tweak list
    merged_leaf->next = another_leaf->next;
    if (another_leaf->next) {
        another_leaf->next->prev = merged_leaf;
    } else {
        end_leaf_ = merged_leaf;
    }
    delete another_leaf;

    CHX_ASSERT(merged_leaf->bytes >= kDataSizeMergeThreshold &&
               merged_leaf->bytes <= kDataSize);
    CHX_ASSERT(merged_leaf->bytes == _total_bytes);
    CHX_ASSERT(merged_leaf->new_lines == _total_lines);

    // tweak the parent
    p->infos[merged_i] = {merged_leaf->new_lines, merged_leaf->bytes};
    std::move(p->infos + merged_i + 2, p->infos + p->size,
              p->infos + merged_i + 1);
    std::move(p->children + merged_i + 2, p->children + p->size,
              p->children + merged_i + 1);
    p->size--;
}

bool TextTree::TryRedistributeIntenalNode(InternalNode* node, size_t index) {
    // First try left sibling
    auto p = node->parent;
    if (index != 0) {
        auto sibling = static_cast<InternalNode*>(p->children[index - 1]);
        size_t size = sibling->size + node->size;
        if (size > kChildSize) {
            size_t redst_i = size / 2;
            std::move(node->infos, node->infos + node->size,
                      node->infos + sibling->size - redst_i);
            std::move(node->children, node->children + node->size,
                      node->children + sibling->size - redst_i);
            std::move(sibling->infos + redst_i, sibling->infos + sibling->size,
                      node->infos);
            std::move(sibling->children + redst_i,
                      sibling->children + sibling->size, node->children);
            size_t moved_lines = 0;
            size_t moved_bytes = 0;
            for (size_t i = redst_i; i < sibling->size; i++) {
                moved_lines += sibling->infos[i].new_lines;
                moved_bytes += sibling->infos[i].bytes;
                sibling->children[i]->parent = node;
            }
            node->bytes += moved_bytes;
            node->new_lines += moved_lines;
            sibling->bytes -= moved_bytes;
            sibling->new_lines -= moved_lines;
            sibling->size = redst_i;
            node->size = size - sibling->size;
            p->infos[index] = {node->new_lines, node->bytes};
            p->infos[index - 1] = {sibling->new_lines, sibling->bytes};
            return true;
        }
    }
    // Then try right sibling
    if (index != p->size - 1) {
        auto sibling = static_cast<InternalNode*>(p->children[index + 1]);
        size_t size = sibling->size + node->size;
        if (size > kChildSize) {
            size_t redst_i = size / 2 - node->size;
            size_t moved_lines = 0;
            size_t moved_bytes = 0;
            for (size_t i = 0; i < redst_i; i++) {
                moved_lines += sibling->infos[i].new_lines;
                moved_bytes += sibling->infos[i].bytes;
                sibling->children[i]->parent = node;
            }
            std::move(sibling->infos, sibling->infos + redst_i,
                      node->infos + node->size);
            std::move(sibling->children, sibling->children + redst_i,
                      node->children + node->size);
            std::move(sibling->infos + redst_i, sibling->infos + sibling->size,
                      sibling->infos);
            std::move(sibling->children + redst_i,
                      sibling->children + sibling->size, sibling->children);
            node->bytes += moved_bytes;
            node->new_lines += moved_lines;
            sibling->bytes -= moved_bytes;
            sibling->new_lines -= moved_lines;
            node->size += redst_i;
            sibling->size -= redst_i;
            p->infos[index] = {node->new_lines, node->bytes};
            p->infos[index + 1] = {sibling->new_lines, sibling->bytes};
            return true;
        }
    }
    return false;
}
void TextTree::MergeInternalNode(InternalNode* node, size_t index) {
    int64_t merged_i = -1;
    auto p = node->parent;
    // First try left sibling
    if (index != 0 &&
        static_cast<InternalNode*>(p->children[index - 1])->size + node->size <=
            kChildSize) {
        merged_i = index - 1;
    }
    // Then try right sibling
    if (merged_i == -1 && index != node->parent->size - 1 &&
        static_cast<InternalNode*>(p->children[index + 1])->size + node->size <=
            kChildSize) {
        merged_i = index;
    }
    CHX_ASSERT(merged_i != -1);
    // Merge them
    auto merged_internal = static_cast<InternalNode*>(p->children[merged_i]);
    auto another_internal =
        static_cast<InternalNode*>(p->children[merged_i + 1]);
    for (size_t i = 0; i < another_internal->size; i++) {
        another_internal->children[i]->parent = merged_internal;
    }
    std::move(another_internal->infos,
              another_internal->infos + another_internal->size,
              merged_internal->infos + merged_internal->size);
    std::move(another_internal->children,
              another_internal->children + another_internal->size,
              merged_internal->children + merged_internal->size);
    merged_internal->new_lines += another_internal->new_lines;
    merged_internal->bytes += another_internal->bytes;
    merged_internal->size += another_internal->size;
    delete another_internal;

    // tweak the parent
    p->infos[merged_i] = {merged_internal->new_lines, merged_internal->bytes};
    std::move(p->infos + merged_i + 2, p->infos + p->size,
              p->infos + merged_i + 1);
    std::move(p->children + merged_i + 2, p->children + p->size,
              p->children + merged_i + 1);
    p->size--;
}

std::string_view TextTree::TextView::ToStringView(std::string& buf) const {
    CHX_ASSERT(end.offset() >= begin.offset());
    if (begin.node_ == end.node_) {
        return {&begin.node_->data[begin.index_], end.index_ - begin.index_};
    }
    buf.clear();
    buf.reserve(end.offset() - begin.offset());
    size_t i = begin.index_;
    LeafNode* node = begin.node_;
    while (true) {
        if (node == end.node_) {
            buf.append(node->data + i, end.index_ - i);
            break;
        }
        buf.append(node->data + i, node->bytes - i);
        i = 0;
        node = node->next;
    }
    return {buf.c_str(), buf.size()};
}

std::string TextTree::TextView::ToString() const {
    std::string str;
    auto sv = ToStringView(str);
    if (str.empty()) {
        str = std::string(sv);
    }
    return str;
}

// TODO: Implement it.
void TextTree::Print() {}

std::string TextTree::Check() {
    // Check leaf nodes
    if (begin_leaf_ == nullptr) {
        return fmt::format("begin_leaf_ is null");
    }
    if (begin_leaf_->prev != nullptr) {
        return fmt::format("begin_leaf_->prev is not null");
    }
    LeafNode* last_n = nullptr;
    for (auto n = begin_leaf_; n != nullptr; n = n->next) {
        size_t lines = std::count(n->data, n->data + n->bytes, '\n');
        if (lines != n->new_lines) {
            return fmt::format(
                "Line cnt mismatch in leaf node: {}, record: {}, actual: {}",
                (void*)n, n->new_lines, lines);
        }
        if (n->bytes > kDataSize) {
            return fmt::format("Leaf node {} data bytes overflow", (void*)n);
        }
        if (n->bytes < kDataSizeMergeThreshold && n != root_) {
            return fmt::format("Leaf node {} data bytes under the threshold",
                               (void*)n);
        }
        if (last_n != nullptr) {
            if (last_n != n->prev) {
                return fmt::format("Leaf node {} 's prev is not correct",
                                   (void*)n);
            }
        }
        last_n = n;
    }
    if (last_n != end_leaf_) {
        return fmt::format("end_leaf_ is not the actually end leaf");
    }

    // Check Index
    if (root_ == begin_leaf_) {
        if (begin_leaf_->parent != nullptr) {
            return fmt::format("root parent is not null");
        } else {
            return {};
        }
    }

    std::queue<InternalNode*> q;
    for (auto n = begin_leaf_; n != nullptr; n = n->next) {
        if (n->parent == nullptr) {
            return fmt::format("Leaf node {} parent is null", (void*)n);
        }
        auto iter = std::find(n->parent->children,
                              n->parent->children + n->parent->size, n);
        if (iter == n->parent->children + n->parent->size) {
            return fmt::format("Leaf node {} can't found in its parent",
                               (void*)n);
        }
        if (q.empty()) {
            q.push(n->parent);
        } else if (q.back() != n->parent) {
            q.push(n->parent);
        }
    }
    size_t this_level_size = q.size();
    while (this_level_size != 1) {
        for (size_t i = 0; i < this_level_size; i++) {
            auto n = q.front();
            q.pop();
            if (n->parent == nullptr) {
                return fmt::format("Internal node {} parent is null", (void*)n);
            }
            if (q.back() != n->parent) {
                q.push(n->parent);
            }
            if (n->size < kChildSize / 2 && n != root_) {
                return fmt::format(
                    "Internal node {} children size under the threshold",
                    (void*)n);
            }
            if (n->size > kChildSize) {
                return fmt::format("Internal node {} has {} children", (void*)n,
                                   n->size);
            }
            size_t lines = 0;
            size_t bytes = 0;
            for (size_t j = 0; j < n->size; j++) {
                lines += n->infos[j].new_lines;
                bytes += n->infos[j].bytes;
                if (n->children[j]->bytes != n->infos[j].bytes) {
                    return fmt::format(
                        "Internal node {} child {} bytes mismatch", (void*)n,
                        j);
                }
                if (n->children[j]->new_lines != n->infos[j].new_lines) {
                    return fmt::format(
                        "Internal node {} child {} lines mismatch", (void*)n,
                        j);
                }
            }
            if (lines != n->new_lines) {
                return fmt::format("Internal node {} lines mismatch", (void*)n);
            }
            if (bytes != n->bytes) {
                return fmt::format("Internal node {} bytes mismatch", (void*)n);
            }
        }
        this_level_size = q.size();
    }
    auto n = q.front();
    if (root_ != n) {
        return fmt::format("root mismatch");
    }
    if (n->size > kChildSize) {
        return fmt::format("root node {} has {} children", (void*)n, n->size);
    }
    size_t lines = 0;
    size_t bytes = 0;
    for (size_t j = 0; j < n->size; j++) {
        lines += n->infos[j].new_lines;
        bytes += n->infos[j].bytes;
        if (n->children[j]->bytes != n->infos[j].bytes) {
            return fmt::format("root node {} child {} bytes mismatch", (void*)n,
                               j);
        }
        if (n->children[j]->new_lines != n->infos[j].new_lines) {
            return fmt::format("root node {} child {} lines mismatch", (void*)n,
                               j);
        }
    }
    if (lines != n->new_lines) {
        return fmt::format("root node {} lines mismatch", (void*)n);
    }
    if (bytes != n->bytes) {
        return fmt::format("root node {} bytes mismatch", (void*)n);
    }
    return {};
}

}  // namespace charxed
