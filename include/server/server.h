#pragma once
#include "query/executor.h"
#include "query/lexer.h"
#include "query/parser.h"
#include <string>
#include <thread>
#include <atomic>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET SocketType;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int SocketType;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

namespace FarhanDB {

class TCPServer {
public:
    TCPServer(Executor* executor, int port = 5555);
    ~TCPServer();

    bool Start();
    void Stop();
    bool IsRunning() const { return running_; }

private:
    Executor*         executor_;
    int               port_;
    SocketType        server_socket_;
    std::atomic<bool> running_;

    void        AcceptLoop();
    void        HandleClient(SocketType client_socket, std::string client_addr);
    std::string FormatResult(const ExecutionResult& result);
    void        SendResponse(SocketType socket, const std::string& response);
    std::string ReceiveQuery(SocketType socket);
};

} // namespace FarhanDB
