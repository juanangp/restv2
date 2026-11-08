#include "FEMSocket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <stdexcept>

void FEMSocket::Open(const std::string& ip) {
    // 1. Initialize UDP socket
    if ((client = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        throw std::runtime_error("Socket open failed: " + std::string(strerror(errno)));
    }

    // 2. Set Receive Buffer Size to 8MB
    int rcvsz_req = 8 * 1024 * 1024;
    socklen_t optlen = sizeof(int);
    setsockopt(client, SOL_SOCKET, SO_RCVBUF, &rcvsz_req, optlen);
    int rcvsz = 0;
    getsockopt(client, SOL_SOCKET, SO_RCVBUF, &rcvsz, &optlen);
    if (rcvsz < 2 * rcvsz_req) {
        // The kernel limits the size to net.core.rmem_max (the value returned is
        // twice the granted one). Try to skip the limit, it needs CAP_NET_ADMIN
        setsockopt(client, SOL_SOCKET, SO_RCVBUFFORCE, &rcvsz_req, optlen);
        getsockopt(client, SOL_SOCKET, SO_RCVBUF, &rcvsz, &optlen);
    }
    if (rcvsz < 2 * rcvsz_req)
        std::cerr << "[WARNING] Socket receive buffer of " << ip << " is only " << rcvsz / 1024
                  << " KB, packets can be lost. Raise the limit "
                  << "with: sysctl -w net.core.rmem_max=" << rcvsz_req << std::endl;

    // 3. Set Receive Timeout (1 second)
    // This allows the thread to unblock from recvfrom and check 'stopReceiver'
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        throw std::runtime_error("Error setting socket timeout: " + std::string(strerror(errno)));
    }

    // 4. Bind the socket (Port 0 lets the OS choose)
    struct sockaddr_in src;
    memset(&src, 0, sizeof(src));
    src.sin_family = AF_INET;
    src.sin_addr.s_addr = htonl(INADDR_ANY);
    src.sin_port = htons(0);

    if (bind(client, (struct sockaddr*)&src, sizeof(src)) != 0) {
        throw std::runtime_error("Socket bind failed: " + std::string(strerror(errno)));
    }

    // 5. Init target address
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(rem_port);

    if (inet_pton(AF_INET, ip.c_str(), &target.sin_addr) != 1) {
        throw std::runtime_error("Invalid Target IP: " + ip);
    }

    remote_size = sizeof(remote);
}

void FEMSocket::Close() {
    if (client >= 0) {
        close(client);
        client = -1;
    }
}

void FEMSocket::Send(const char* cmd) const {
    if (sendto(client, cmd, strlen(cmd), 0, (const struct sockaddr*)&target, sizeof(target)) == -1)
        throw std::runtime_error("sendto failed: " + std::string(strerror(errno)));
}

int FEMSocket::Receive(void* buffer, size_t size, int timeoutSeconds, const std::atomic<bool>* abort) {
    const auto start = std::chrono::steady_clock::now();
    while (true) {
        const int length = recvfrom(client, buffer, size, 0, (struct sockaddr*)&remote, &remote_size);
        if (length >= 0) return length;
        if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) {
            if (abort && *abort) return -1;
            throw std::runtime_error("recvfrom failed: " + std::string(strerror(errno)));
        }
        if (abort && *abort) return -1;
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start)
                .count();
        if (elapsed >= timeoutSeconds) {
            throw std::runtime_error("TIMEOUT: No reply after " + std::to_string(elapsed) + " seconds");
        }
    }
}
