#include "rpc.h"

#include <unistd.h>

#include <charconv>

#include "exception.h"
#include "result.h"
#include "utils.h"

namespace mango {

using namespace std::string_view_literals;

static constexpr auto kLspEOL = "\r\n"sv;
static constexpr auto kLspHeaderKVSep = ": "sv;
static constexpr auto kLspDefautlContentType =
    "application/vscode-jsonrpc; charset=utf-8"sv;
static constexpr auto kLspKeyContentLength = "Content-Length"sv;
static constexpr auto kLspKeyContentType = "Content-Type"sv;

struct LspHeader {
    size_t content_length = 0;
    std::string content_type{kLspDefautlContentType};

    bool IsValid() noexcept {
        return content_length != 0 && content_type == kLspDefautlContentType;
    }
};

// line shouldn't include \r\n.
// Header will be modified according to the parsing result.
// Throw ParseMsgException.
static void ParseLspHeaderLine(std::string_view line, LspHeader& header) {
    auto sep_loc = line.find(kLspHeaderKVSep);
    if (sep_loc == std::string_view::npos) {
        throw ParseMsgException("{}", "Bad header line: Can't find \": \"");
    }
    if (line.substr(0, sep_loc) == kLspKeyContentType) {
        const char* last = line.data() + line.size();
        auto [ptr, errc] =
            std::from_chars(line.data() + sep_loc + kLspHeaderKVSep.size(),
                            last, header.content_length);
        if (errc != std::errc() || ptr != last) {
            throw ParseMsgException("{}", "Bad header line: Wrong value type");
        }
    } else if (line.substr(0, sep_loc) == kLspKeyContentType) {
        header.content_type = line.substr(sep_loc + kLspHeaderKVSep.size());
    } else {
        throw ParseMsgException("{}", "Unknown header line");
    }
}

// Header will be modified according to the parsing result.
// Throw ParseMsgException.
// return kOk or kMsgNeedMore
static Result ParseLspHeader(std::string_view msg, LspHeader& header,
                             size_t& content_offset) {
    size_t start = 0;
    while (true) {
        auto eol_loc = msg.find("\r\n", start);
        if (eol_loc == std::string_view::npos) {
            return kMsgNeedMore;
        }
        if (eol_loc == start) {
            // content begin
            if (!header.IsValid()) {
                return kError;
            }
            content_offset = eol_loc + kLspEOL.size();
            return kOk;
        }
        ParseLspHeaderLine(msg.substr(start, eol_loc - start), header);
        start = eol_loc + kLspEOL.size();
    }
    return kOk;  // Make compiler happy
}

// If success, content & msg_end will be set. msg_end denotes the last
// index(exclude) for the whole msg in str.
// Throw ParseMsgException, Json::exception return kOk or kMsgNeedMore
static Result ParseLspMessage(std::string_view msg, Json& content,
                              size_t& msg_end) {
    LspHeader header;
    size_t content_offset;
    Result res = ParseLspHeader(msg, header, content_offset);
    if (res != kOk) {
        return res;
    }
    if (msg.size() - content_offset < header.content_length) {
        return kMsgNeedMore;
    }
    content = Json::parse(msg.substr(content_offset, header.content_length));
    msg_end = content_offset + header.content_length;
    return kOk;
}

std::vector<Json> LspRpcReader::ReadMessage() {
    MGO_ASSERT(acc_buf_.empty());
    constexpr size_t size = 1024;
    char buf[size];
    std::vector<Json> contents(1);
    size_t msg_end;
    while (true) {
        ssize_t bytes = read(fd_, buf, size);
        if (bytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw IOException("read from fd {} error: {}", fd_,
                              strerror(errno));
        }
        if (acc_buf_.empty()) {
            Result res = ParseLspMessage({buf, static_cast<size_t>(bytes)},
                                         contents.back(), msg_end);
            if (res == kMsgNeedMore) {
                acc_buf_.append(buf, bytes);
                continue;
            }
            // kOk
            if (msg_end == static_cast<size_t>(bytes)) {
                return contents;
            } else {
                contents.emplace_back();
                acc_buf_.append(buf + msg_end, bytes - msg_end);
                continue;
            }
        } else {
            acc_buf_.append(buf, bytes);
            Result res = ParseLspMessage(acc_buf_, contents.back(), msg_end);
            if (res == kMsgNeedMore) {
                continue;
            }
            // kOk
            if (msg_end == acc_buf_.size()) {
                acc_buf_.clear();
                return contents;
            } else {
                contents.emplace_back();
                acc_buf_.erase(0, msg_end);
                continue;
            }
        }
    }
}

void AddLspHeaderLine(std::string_view key, const std::string& value,
                      std::string& msg) {
    msg.append(key);
    msg.append(kLspHeaderKVSep);
    msg.append(value);
    msg.append(kLspEOL);
}

void AddLspHeader(std::string& msg, const LspHeader& header) {
    AddLspHeaderLine(kLspKeyContentLength,
                     std::to_string(header.content_length), msg);
    msg.append(kLspEOL);
}

void LspRpcWriter::WriteMessage(const Json& content) {
    LspHeader header;
    std::string content_str = nlohmann::to_string(content);
    header.content_length = content_str.size();
    AddLspHeader(msg_buf_, header);
    msg_buf_.append(content_str);

    size_t cur_index = 0;
    while (cur_index != msg_buf_.size()) {
        ssize_t bytes = write(fd_, msg_buf_.data() + cur_index,
                              msg_buf_.size() - cur_index);
        if (bytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw IOException("write to fd {} error: {}", fd_, strerror(errno));
        }
        cur_index += bytes;
    }
    msg_buf_.clear();
}

}  // namespace mango
