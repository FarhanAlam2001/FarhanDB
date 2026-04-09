#include "server/server.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <iomanip>

namespace FarhanDB {

TCPServer::TCPServer(Executor* executor, int port)
    : executor_(executor), port_(port),
      server_socket_(INVALID_SOCKET), running_(false) {}

TCPServer::~TCPServer() {
    Stop();
}

bool TCPServer::Start() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "[Server] WSAStartup failed.\n";
        return false;
    }
#endif

    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ == INVALID_SOCKET) {
        std::cerr << "[Server] Failed to create socket.\n";
        return false;
    }

    // Allow port reuse
    int opt = 1;
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port_);

    if (bind(server_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[Server] Bind failed on port " << port_ << "\n";
        return false;
    }

    if (listen(server_socket_, 10) == SOCKET_ERROR) {
        std::cerr << "[Server] Listen failed.\n";
        return false;
    }

    running_ = true;
    std::cout << "\n  [FarhanDB Server] Started on port " << port_ << std::endl;
    std::cout << "  [FarhanDB Server] Waiting for connections..." << std::endl;
    std::cout << "  [FarhanDB Server] Connect using: telnet localhost "
              << port_ << std::endl;
    std::cout << "  [FarhanDB Server] Press Ctrl+C to stop.\n" << std::endl;

    AcceptLoop();
    return true;
}

void TCPServer::Stop() {
    running_ = false;
#ifdef _WIN32
    if (server_socket_ != INVALID_SOCKET) {
        closesocket(server_socket_);
        server_socket_ = INVALID_SOCKET;
    }
    WSACleanup();
#else
    if (server_socket_ != INVALID_SOCKET) {
        close(server_socket_);
        server_socket_ = INVALID_SOCKET;
    }
#endif
}

void TCPServer::AcceptLoop() {
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t   client_len = sizeof(client_addr);

        SocketType client = accept(server_socket_,
                                   reinterpret_cast<sockaddr*>(&client_addr),
                                   &client_len);
        if (client == INVALID_SOCKET) {
            if (running_) std::cerr << "[Server] Accept failed.\n";
            continue;
        }

        // Get client IP
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        std::string client_str = std::string(ip) + ":" +
                                 std::to_string(ntohs(client_addr.sin_port));

        std::cout << "  [+] Client connected: " << client_str << std::endl;

        // Handle each client in a separate thread
        std::thread([this, client, client_str]() {
            HandleClient(client, client_str);
        }).detach();
    }
}

void TCPServer::HandleClient(SocketType client_socket, std::string client_addr) {
    // Send welcome message
    std::string welcome =
        "====================================\n"
        "  Welcome to FarhanDB v1.0.0\n"
        "  Type SQL queries ending with ;\n"
        "  Type 'exit' to disconnect.\n"
        "====================================\n"
        "farhandb> ";
    SendResponse(client_socket, welcome);

    std::string buffer, query;
    while (true) {
        std::string chunk = ReceiveQuery(client_socket);
        if (chunk.empty()) break; // client disconnected

        if (chunk == "exit\n" || chunk == "exit\r\n" || chunk == "quit\n") {
            SendResponse(client_socket, "Goodbye!\n");
            break;
        }

        query += chunk;

        if (query.find(';') != std::string::npos) {
            try {
                Lexer  lexer(query);
                auto   tokens = lexer.Tokenize();
                Parser parser(tokens);
                auto   stmt   = parser.Parse();
                auto   result = executor_->Execute(stmt);
                SendResponse(client_socket, FormatResult(result) + "\nfarhandb> ");
            } catch (const std::exception& e) {
                SendResponse(client_socket,
                    std::string("[ERROR] ") + e.what() + "\nfarhandb> ");
            }
            query.clear();
        }
    }

    std::cout << "  [-] Client disconnected: " << client_addr << std::endl;
#ifdef _WIN32
    closesocket(client_socket);
#else
    close(client_socket);
#endif
}

std::string TCPServer::FormatResult(const ExecutionResult& result) {
    std::ostringstream ss;

    if (!result.success) {
        ss << "[ERROR] " << result.message << "\n";
        return ss.str();
    }

    if (!result.rows.empty()) {
        // Calculate column widths
        std::vector<size_t> widths;
        for (const auto& name : result.column_names)
            widths.push_back(name.size());
        for (const auto& row : result.rows)
            for (size_t i = 0; i < row.size() && i < widths.size(); i++)
                widths[i] = std::max(widths[i], row[i].size());

        // Top border
        ss << "+";
        for (auto w : widths) ss << std::string(w + 2, '=') << "+";
        ss << "\n| ";

        // Headers
        for (size_t i = 0; i < result.column_names.size(); i++)
            ss << std::left << std::setw(widths[i])
               << result.column_names[i] << " | ";
        ss << "\n+";
        for (auto w : widths) ss << std::string(w + 2, '=') << "+";
        ss << "\n";

        // Rows
        for (size_t r = 0; r < result.rows.size(); r++) {
            ss << "| ";
            for (size_t i = 0; i < result.rows[r].size() && i < widths.size(); i++)
                ss << std::left << std::setw(widths[i])
                   << result.rows[r][i] << " | ";
            ss << "\n";
            if (r < result.rows.size() - 1) {
                ss << "+";
                for (auto w : widths) ss << std::string(w + 2, '-') << "+";
                ss << "\n";
            }
        }

        // Bottom border
        ss << "+";
        for (auto w : widths) ss << std::string(w + 2, '=') << "+";
        ss << "\n";
        ss << result.rows.size() << " row(s) in set.\n";
    } else {
        ss << "[OK] " << result.message << "\n";
    }

    return ss.str();
}

void TCPServer::SendResponse(SocketType socket, const std::string& response) {
    send(socket, response.c_str(), (int)response.size(), 0);
}

std::string TCPServer::ReceiveQuery(SocketType socket) {
    char buf[4096];
    int  received = recv(socket, buf, sizeof(buf) - 1, 0);
    if (received <= 0) return "";
    buf[received] = '\0';
    return std::string(buf);
}

} // namespace FarhanDB
