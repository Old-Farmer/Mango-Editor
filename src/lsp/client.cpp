#include "client.h"

namespace mango {

void LspClient::StartClient() {
    if (started) {
        return;
    }
    started = true;

}
void LspClient::StopClient() {
    if (!started) {
        return;
    }
    started = false;
}

StdioLspClient::StdioLspClient(const char* const argv[])
    : server_process_(argv) {
    Init(LspRpcReader(server_process_.stdout().fd),
         LspRpcWriter(server_process_.stdin().fd));
}

StdioLspClient::~StdioLspClient() {
    StopClient();
}

}  // namespace mango
