//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2p_server.h"
#include <cassert>
#include <cstring>
#include <system_error>
#include <spdlog/spdlog.h>
#include <unistd.h>
#if __has_include(<netinet/in.h>)
#include <netinet/in.h>
#endif
#if __has_include(<sys/socket.h>)
#include <sys/socket.h>
#endif

string S2pServer::Init(int port)
{
    assert(!running);
    assert(server_socket == -1);
    assert(port > 0 && port <= 65535);

#if __has_include(<sys/socket.h>)
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == -1) {
        return fmt::format("Can't create server socket: {}", system_error(errno, generic_category()).what());
    }

    if (const int enable = 1; setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
        CleanUp();
        return fmt::format("Can't reuse socket: {}", system_error(errno, generic_category()).what());
    }

    sockaddr_in server = { };
    server.sin_family = AF_INET;
    server.sin_port = htons(static_cast<uint16_t>(port));
    server.sin_addr.s_addr = INADDR_ANY;
    if (::bind(server_socket, (const sockaddr*)&server, static_cast<socklen_t>(sizeof(sockaddr_in))) < 0) {
        CleanUp();
        return fmt::format("Port {} is in use, s2p may already be running", port);
    }

    if (listen(server_socket, 2) == -1) {
        CleanUp();
        return fmt::format("Can't listen on server socket: {}", system_error(errno, generic_category()).what());
    }

    running = true;
#endif

    return "";
}

void S2pServer::CleanUp()
{
#ifdef SHUT_RD
    if (const int fd = server_socket.exchange(-1); fd != -1) {
        shutdown(fd, SHUT_RD);
        close(fd);
    }
#endif

    running = false;
}

int S2pServer::Accept() const
{
#if __has_include(<sys/socket.h>)
    return accept(server_socket, nullptr, nullptr);
#else
    return -1;
#endif
}
