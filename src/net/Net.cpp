#include "Net.h"

#include <cstring>

namespace obnet {

namespace {

// 进程内只初始化一次 Winsock（静态对象析构时清理）
struct WinsockGuard {
    WinsockGuard() {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
    }
    ~WinsockGuard() {
        WSACleanup();
    }
};

WinsockGuard g_winsockGuard;

std::string SocketErrorMessage(int code) {
    switch (code) {
    case WSAECONNREFUSED: return "connection refused";
    case WSAETIMEDOUT:    return "connection timed out";
    case WSAENETUNREACH:  return "network unreachable";
    case WSAEHOSTUNREACH: return "host unreachable";
    case WSAEADDRINUSE:   return "port already in use";
    case WSAECONNRESET:   return "connection reset by peer";
    case WSAHOST_NOT_FOUND: return "host not found";
    default: return "socket error " + std::to_string(code);
    }
}

} // namespace

std::vector<std::string> GetLocalIPv4() {
    std::vector<std::string> result;
    char host[256] = { 0 };
    if (gethostname(host, sizeof(host) - 1) != 0) {
        return result;
    }
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* info = nullptr;
    if (getaddrinfo(host, nullptr, &hints, &info) != 0 || info == nullptr) {
        return result;
    }
    for (addrinfo* p = info; p != nullptr; p = p->ai_next) {
        char text[INET_ADDRSTRLEN] = { 0 };
        sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(p->ai_addr);
        if (inet_ntop(AF_INET, &addr->sin_addr, text, sizeof(text)) != nullptr) {
            std::string ip(text);
            bool exists = false;
            for (const std::string& item : result) {
                if (item == ip) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                result.push_back(ip);
            }
        }
    }
    freeaddrinfo(info);
    if (result.empty()) {
        result.push_back("127.0.0.1");
    }
    return result;
}

NetChannel::NetChannel()
    : listenSock_(INVALID_SOCKET), sock_(INVALID_SOCKET), recvStop_(false), connected_(false) {
}

NetChannel::~NetChannel() {
    Close();
}

void NetChannel::CloseSocketQuietly(SOCKET& s) {
    if (s != INVALID_SOCKET) {
        closesocket(s);
        s = INVALID_SOCKET;
    }
}

bool NetChannel::StartServer(uint16_t port, std::string* error) {
    Close();
    listenSock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock_ == INVALID_SOCKET) {
        SetError(SocketErrorMessage(WSAGetLastError()));
        if (error) {
            *error = LastError();
        }
        return false;
    }
    BOOL reuse = TRUE;
    setsockopt(listenSock_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(listenSock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        SetError(SocketErrorMessage(WSAGetLastError()));
        CloseSocketQuietly(listenSock_);
        if (error) {
            *error = LastError();
        }
        return false;
    }
    if (listen(listenSock_, 1) == SOCKET_ERROR) {
        SetError(SocketErrorMessage(WSAGetLastError()));
        CloseSocketQuietly(listenSock_);
        if (error) {
            *error = LastError();
        }
        return false;
    }
    return true;
}

bool NetChannel::WaitForClient(int waitMs, std::string* error) {
    if (listenSock_ == INVALID_SOCKET) {
        SetError("not listening");
        if (error) {
            *error = LastError();
        }
        return false;
    }
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(listenSock_, &readSet);
    timeval timeout;
    timeout.tv_sec = waitMs / 1000;
    timeout.tv_usec = (waitMs % 1000) * 1000;
    const int ready = select(0, &readSet, nullptr, nullptr, &timeout);
    if (ready == SOCKET_ERROR) {
        SetError(SocketErrorMessage(WSAGetLastError()));
        if (error) {
            *error = LastError();
        }
        return false;
    }
    if (ready == 0) {
        return false; // 超时，继续等
    }
    SOCKET client = accept(listenSock_, nullptr, nullptr);
    if (client == INVALID_SOCKET) {
        SetError(SocketErrorMessage(WSAGetLastError()));
        if (error) {
            *error = LastError();
        }
        return false;
    }
    sock_ = client;
    CloseSocketQuietly(listenSock_);
    connected_.store(true);
    SetError("");
    return true;
}

bool NetChannel::ConnectTo(const std::string& ip, uint16_t port, int timeoutMs, std::string* error) {
    Close();
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        SetError("invalid IPv4 address");
        if (error) {
            *error = LastError();
        }
        return false;
    }
    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET) {
        SetError(SocketErrorMessage(WSAGetLastError()));
        if (error) {
            *error = LastError();
        }
        return false;
    }
    u_long nonBlocking = 1;
    ioctlsocket(sock_, FIONBIO, &nonBlocking);

    const int rc = connect(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (rc == SOCKET_ERROR) {
        const int code = WSAGetLastError();
        if (code != WSAEWOULDBLOCK && code != WSAEINPROGRESS) {
            SetError(SocketErrorMessage(code));
            CloseSocketQuietly(sock_);
            if (error) {
                *error = LastError();
            }
            return false;
        }
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(sock_, &writeSet);
        timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;
        const int ready = select(0, nullptr, &writeSet, nullptr, &timeout);
        if (ready == SOCKET_ERROR) {
            SetError(SocketErrorMessage(WSAGetLastError()));
            CloseSocketQuietly(sock_);
            if (error) {
                *error = LastError();
            }
            return false;
        }
        if (ready == 0) {
            SetError("connect timed out");
            CloseSocketQuietly(sock_);
            if (error) {
                *error = LastError();
            }
            return false;
        }
        int soError = 0;
        int soErrorLen = sizeof(soError);
        getsockopt(sock_, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soError), &soErrorLen);
        if (soError != 0) {
            SetError(SocketErrorMessage(soError));
            CloseSocketQuietly(sock_);
            if (error) {
                *error = LastError();
            }
            return false;
        }
    }
    nonBlocking = 0;
    ioctlsocket(sock_, FIONBIO, &nonBlocking);
    connected_.store(true);
    SetError("");
    return true;
}

void NetChannel::StartReceiveThread() {
    if (recvThread_.joinable() || sock_ == INVALID_SOCKET) {
        return;
    }
    recvStop_.store(false);
    recvThread_ = std::thread(&NetChannel::ReceiveLoop, this);
}

void NetChannel::ReceiveLoop() {
    uint8_t buffer[1024];
    size_t filled = 0;
    while (!recvStop_.load()) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(sock_, &readSet);
        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100 * 1000;
        const int ready = select(0, &readSet, nullptr, nullptr, &timeout);
        if (recvStop_.load()) {
            break;
        }
        if (ready == SOCKET_ERROR) {
            SetError(SocketErrorMessage(WSAGetLastError()));
            break;
        }
        if (ready == 0) {
            continue;
        }
        const int received = recv(sock_, reinterpret_cast<char*>(buffer) + filled,
                                  static_cast<int>(sizeof(buffer) - filled), 0);
        if (received <= 0) {
            SetError(received == 0 ? "peer disconnected" : SocketErrorMessage(WSAGetLastError()));
            break;
        }
        filled += static_cast<size_t>(received);

        size_t offset = 0;
        while (filled - offset >= static_cast<size_t>(kMsgSize)) {
            Msg msg;
            if (!DecodeMsg(buffer + offset, msg)) {
                SetError("protocol error: unknown message type");
                filled = 0;
                offset = 0;
                break;
            }
            PushMessage(msg);
            offset += static_cast<size_t>(kMsgSize);
        }
        if (filled == 0) {
            break; // 协议错误，退出接收循环
        }
        if (offset > 0) {
            std::memmove(buffer, buffer + offset, filled - offset);
            filled -= offset;
        }
    }
    connected_.store(false);
}

bool NetChannel::Send(const Msg& msg) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    if (sock_ == INVALID_SOCKET || !connected_.load()) {
        return false;
    }
    uint8_t buffer[kMsgSize];
    EncodeMsg(msg, buffer);
    size_t sent = 0;
    while (sent < static_cast<size_t>(kMsgSize)) {
        const int rc = send(sock_, reinterpret_cast<const char*>(buffer) + sent,
                            static_cast<int>(kMsgSize - sent), 0);
        if (rc <= 0) {
            SetError(SocketErrorMessage(WSAGetLastError()));
            connected_.store(false);
            return false;
        }
        sent += static_cast<size_t>(rc);
    }
    return true;
}

bool NetChannel::Poll(Msg& msg) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (queue_.empty()) {
        return false;
    }
    msg = queue_.front();
    queue_.pop();
    return true;
}

void NetChannel::PollAll(std::vector<Msg>& out) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    while (!queue_.empty()) {
        out.push_back(queue_.front());
        queue_.pop();
    }
}

void NetChannel::PushMessage(const Msg& msg) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    queue_.push(msg);
}

void NetChannel::Close() {
    recvStop_.store(true);
    connected_.store(false);
    if (sock_ != INVALID_SOCKET) {
        shutdown(sock_, SD_BOTH);
    }
    if (recvThread_.joinable()) {
        recvThread_.join();
    }
    CloseSocketQuietly(sock_);
    CloseSocketQuietly(listenSock_);
    std::lock_guard<std::mutex> lock(queueMutex_);
    while (!queue_.empty()) {
        queue_.pop();
    }
}

void NetChannel::SetError(const std::string& error) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    error_ = error;
}

std::string NetChannel::LastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return error_;
}

} // namespace obnet
