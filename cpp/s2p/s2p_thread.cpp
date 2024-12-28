//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2p_thread.h"
#include <cassert>
#include <csignal>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "command/command_context.h"
#include "shared/s2p_exceptions.h"

using namespace spdlog;
using namespace s2p_util;

string S2pThread::Init(const callback &cb, int port, shared_ptr<logger> logger)
{
    assert(service_socket == -1);

    if (port <= 0 || port > 65535) {
        return fmt::format("Invalid port number: {}", port);
    }

    service_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (service_socket == -1) {
        return fmt::format("Can't create s2p service socket: {}", strerror(errno));
    }

    if (const int enable = 1; setsockopt(service_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
        Stop();
        return fmt::format("Can't reuse socket: {}", strerror(errno));
    }

    sockaddr_in server = { };
    server.sin_family = PF_INET;
    server.sin_port = htons((uint16_t)port);
    server.sin_addr.s_addr = INADDR_ANY;
    if (bind(service_socket, reinterpret_cast<const sockaddr*>(&server), // NOSONAR bit_cast is not supported by the bullseye compiler
        static_cast<socklen_t>(sizeof(sockaddr_in))) < 0) {
        Stop();
        return fmt::format("Port {} is in use, s2p may already be running", port);
    }

    if (listen(service_socket, 2) == -1) {
        Stop();
        return "Can't listen to service socket: " + string(strerror(errno));
    }

    s2p_logger = logger;

    execute = cb;

    return "";
}

void S2pThread::Start()
{
    assert(service_socket != -1);

#ifndef __APPLE__
    service_thread = jthread([this]() {Execute();});
#else
    service_thread = thread([this] () { Execute(); } );
#endif
}

void S2pThread::Stop()
{
    // This method might be called twice when pressing Ctrl-C, because of the installed handlers
    if (service_socket != -1) {
        shutdown(service_socket, SHUT_RD);
        close(service_socket);

        service_socket = -1;
    }
}

bool S2pThread::IsRunning() const
{
    return service_socket != -1 && service_thread.joinable();
}

void S2pThread::Execute() const
{
    while (service_socket != -1) {
        const int fd = accept(service_socket, nullptr, nullptr);
        if (fd != -1) {
            ExecuteCommand(fd);
            close(fd);
        }
    }
}

void S2pThread::ExecuteCommand(int fd) const
{
    CommandContext context(fd, *s2p_logger);
    try {
        if (context.ReadCommand()) {
            execute(context);
        }
    }
    catch (const IoException &e) {
        warn(e.what());

        // Try to return an error message (this may fail if the exception was caused when returning the actual result)
        PbResult result;
        result.set_msg(e.what());
        try {
            context.WriteResult(result);
        }
        catch (const IoException&) { // NOSONAR Ignored because not relevant when writing the result
            // Ignore
        }
    }
}
