#pragma once

#include <unordered_map>

#include "buffer.h"
#include "utils.h"

namespace mango {

class Buffer;
class Path;

using OnBufferRemove = std::function<void(const Buffer*)>;

class BufferManager {
   public:
    BufferManager();
    ~BufferManager() = default;
    MGO_DELETE_COPY(BufferManager);
    MGO_DELETE_MOVE(BufferManager);

    Buffer* AddBuffer(Buffer&& buffer);
    void RemoveBuffer(Buffer* buffer);

    struct HandlerID {
        size_t id;
        size_t generation;
    };

    HandlerID AddOnBufferRemoveHandler(OnBufferRemove handler);
    void RemoveOnBufferRemoveHandler(HandlerID id);

    Buffer* Begin();
    Buffer* End();  // exclusive
    Buffer* FindBuffer(const std::string& name);
    Buffer* FindBuffer(int64_t id);


   private:
    std::unordered_map<int64_t, Buffer> buffers_;
    Buffer list_head_;
    Buffer list_tail_;

    struct OnBufferRemoveStore {
        OnBufferRemove handler;
        size_t generation = 0;
    };

    std::vector<OnBufferRemoveStore> on_buffer_removes_;
};

}  // namespace mango