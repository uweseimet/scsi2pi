//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2p_server.h"
#include <cassert>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

string S2pServer::Init(int port)
{
    assert(server_socket == -1);
    assert(port > 0 && port <= 65535);

    server_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        return "Can't create server socket: " + string(strerror(errno));
    }

    if (const int enable = 1; setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
        CleanUp();
        return "Can't reuse socket: " + string(strerror(errno));
    }

    sockaddr_in server = { };
    server.sin_family = PF_INET;
    server.sin_port = htons(static_cast<uint16_t>(port));
    server.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_socket, reinterpret_cast<const sockaddr*>(&server), // NOSONAR bit_cast is not supported by the bullseye compiler
        static_cast<socklen_t>(sizeof(sockaddr_in))) < 0) {
        CleanUp();
        return "Port " + to_string(port) + " is in use, s2p may already be running";
    }

    if (listen(server_socket, 2) == -1) {
        CleanUp();
        return "Can't listen on server socket: " + string(strerror(errno));
    }

    return "";
}

void S2pServer::CleanUp()
{
    if (server_socket != -1) {
        shutdown(server_socket, SHUT_RD);
        close(server_socket);

        server_socket = -1;
    }
}

int S2pServer::Accept() const
{
    return accept(server_socket, nullptr, nullptr);
}
