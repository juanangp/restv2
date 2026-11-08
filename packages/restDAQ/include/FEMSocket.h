#pragma once

#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <iostream>
#include <string>

/// \file FEMSocket.h
/// \brief UDP socket to a FEM.

/// UDP port of the FEMs (and the TCM)
constexpr unsigned short remote_dst_port = 1122;

/// \class FEMSocket
/// \brief UDP socket to a FEM: 8 MB receive buffer (if the kernel allows it) and a receive timeout of 1 s.
class FEMSocket {
   public:
    FEMSocket() = default;
    ~FEMSocket() { Close(); }

    // Neither copied nor moved: the socket belongs to a single object
    FEMSocket(const FEMSocket&) = delete;
    FEMSocket& operator=(const FEMSocket&) = delete;

    int client = -1;              ///< Socket descriptor, -1 if not open
    struct sockaddr_in target{};  ///< Address of the FEM

    struct sockaddr_in remote{};                     ///< Address of the sender of the last datagram
    unsigned int remote_size = sizeof(sockaddr_in);  ///< Size of remote
    unsigned short rem_port = remote_dst_port;       ///< Port of the FEM

    /// \brief Closes the socket, if open
    void Close();
    /// \brief Opens the socket (bound to any local port) to the FEM at this IP address; throws on an error
    void Open(const std::string& ip);

    /// \brief Sends a command (a datagram) to the target; throws if it cannot be sent
    void Send(const char* cmd) const;

    /// \brief Waits for a datagram up to timeoutSeconds (each recvfrom waits at most the 1 s of
    /// SO_RCVTIMEO). Returns its length in bytes, or -1 if `abort` becomes true while waiting. Throws on a
    /// socket error or the timeout.
    int Receive(void* buffer, size_t size, int timeoutSeconds, const std::atomic<bool>* abort = nullptr);
};
