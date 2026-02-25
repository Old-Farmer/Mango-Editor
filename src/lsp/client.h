#pragma once

#include "lsp/rpc.h"
#include "subprocess.h"

namespace mango {

class LspClient {
   public:
    LspClient() {}
    virtual ~LspClient() {}
    // This function should be called by derived class ctor.
    void Init(const LspRpcReader& reader, const LspRpcWriter& writer) {
        reader_ = reader;
        writer_ = writer;
    }

    // Lifetime management.
    void StartClient();
    // This function should be called by derived class dtor.
    void StopClient();

    void OnResponse();
    void OnNotification();
    void Request();
    void Notification();

   private:
    LspRpcReader reader_;
    LspRpcWriter writer_;
    bool started = false;
};

class StdioLspClient : public LspClient {
   public:
    // Thorw OSException.
    explicit StdioLspClient(const char* const argv[]);
    ~StdioLspClient();

   private:
    Subprocess server_process_;
};

}  // namespace mango