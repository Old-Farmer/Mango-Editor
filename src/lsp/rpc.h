#pragma once
#include "json.h"

namespace mango {

// A Blocking reader
class LspRpcReader {
   public:
    LspRpcReader() {}
    explicit LspRpcReader(int fd) : fd_(fd) {}
    // throw ParseMsgException, Json::exception, IOException
    std::vector<Json> ReadMessage();

   private:
    int fd_;
    std::string acc_buf_;
};

// A Blocking writer
class LspRpcWriter {
   public:
    LspRpcWriter() {}
    explicit LspRpcWriter(int fd) : fd_(fd) {}
    // throw IOException
    void WriteMessage(const Json& content);

   private:
    int fd_;
    std::string msg_buf_;
};

}  // namespace mango