// 局域网对战 —— TCP 传输层（不依赖 EasyX）
// 负责：监听/接受、连接、后台接收线程、消息队列、发送。
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "Protocol.h"

#pragma comment(lib, "ws2_32.lib")

namespace obnet {

// 枚举本机 IPv4 地址（用于主机界面显示）
std::vector<std::string> GetLocalIPv4();

class NetChannel {
public:
    NetChannel();
    ~NetChannel();

    NetChannel(const NetChannel&) = delete;
    NetChannel& operator=(const NetChannel&) = delete;

    // 主机：监听端口（不阻塞）
    bool StartServer(uint16_t port, std::string* error);
    // 主机：等待一个客户端接入；waitMs 为单次等待时长，返回 true 表示已接入。
    // 典型用法：界面循环里每帧调用，超时返回 false 后继续重试。
    bool WaitForClient(int waitMs, std::string* error);

    // 客户端：连接主机（最多阻塞 timeoutMs 毫秒）
    bool ConnectTo(const std::string& ip, uint16_t port, int timeoutMs, std::string* error);

    // 连接建立后调用：启动后台接收线程
    void StartReceiveThread();

    // 关闭连接与线程（可重复调用）
    void Close();

    bool Send(const Msg& msg);

    // 主线程侧非阻塞取消息
    bool Poll(Msg& msg);
    void PollAll(std::vector<Msg>& out);

    bool Connected() const { return connected_.load(); }
    std::string LastError() const;

private:
    void ReceiveLoop();
    void PushMessage(const Msg& msg);
    void SetError(const std::string& error);
    void CloseSocketQuietly(SOCKET& s);

    SOCKET listenSock_;
    SOCKET sock_;
    std::thread recvThread_;
    std::atomic<bool> recvStop_;
    std::atomic<bool> connected_;
    std::mutex sendMutex_;
    std::mutex queueMutex_;
    std::queue<Msg> queue_;
    mutable std::mutex errorMutex_;
    std::string error_;
};

} // namespace obnet
