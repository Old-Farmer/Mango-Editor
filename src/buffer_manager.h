#pragma once

#include <unordered_map>
#include "buffer.h"
#include "utils.h"

namespace mango {

class Buffer;
class Path;

class BufferManager {
   public:
    MANGO_DEFAULT_CONSTRUCT_DESTRUCT(BufferManager);
    MANGO_DELETE_COPY(BufferManager);
    MANGO_DELETE_MOVE(BufferManager);

    Buffer* AddBuffer(Buffer buffer);
    void RemoveBuffer(Buffer* buffer);
    Buffer* FirstBuffer();
    Buffer* LastBuffer();
    Buffer* FindBuffer(Path& path);
    Buffer* FindBuffer(int64_t id);

   private:
    std::unordered_map<int64_t, Buffer> buffers_;
    Buffer buffer_list_head_ = Buffer();
    Buffer* buffer_list_tail_ = &buffer_list_head_;
};

}  // namespace mango