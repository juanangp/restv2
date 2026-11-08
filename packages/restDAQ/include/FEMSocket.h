#pragma once

#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <iostream>
#include <string>

constexpr unsigned short remote_dst_port = 1122;

class FEMSocket {
   public:
    FEMSocket() = default;
    ~FEMSocket() { Close(); }

    // Neither copied nor moved: the socket belongs to a single object
    FEMSocket(const FEMSocket&) = delete;
    FEMSocket& operator=(const FEMSocket&) = delete;

    int client = -1;
    struct sockaddr_in target{};

    struct sockaddr_in remote{};
    unsigned int remote_size = sizeof(sockaddr_in);
    unsigned short rem_port = remote_dst_port;

    void Close();
    void Open(const std::string& ip);

    /// \brief Sends a command (a datagram) to the target; throws if it cannot be sent
    void Send(const char* cmd) const;

    /// \brief Waits for a datagram up to timeoutSeconds (each recvfrom waits at most the 1 s of
    /// SO_RCVTIMEO). Returns its length in bytes, or -1 if `abort` becomes true while waiting. Throws on a
    /// socket error or the timeout.
    int Receive(void* buffer, size_t size, int timeoutSeconds, const std::atomic<bool>* abort = nullptr);
};
