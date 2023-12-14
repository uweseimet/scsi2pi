//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <csignal>
#include <cassert>
#include "shared/s2p_util.h"
#include "shared/shared_exceptions.h"
#include "shared_protobuf/command_context.h"
#include "s2p_thread.h"

using namespace s2p_util;

string S2pThread::Init(const callback &cb, int port)
{
    assert(service_socket == -1);

    if (port <= 0 || port > 65535) {
        return "Invalid port number " + to_string(port);
    }

    service_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (service_socket == -1) {
        return "Unable to create service socket: " + string(strerror(errno));
    }

    if (const int yes = 1; setsockopt(service_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        Stop();
        return "Can't reuse address";
    }

    sockaddr_in server = { };
    server.sin_family = PF_INET;
    server.sin_port = htons((uint16_t)port);
    server.sin_addr.s_addr = INADDR_ANY;
    if (bind(service_socket, reinterpret_cast<const sockaddr*>(&server), // NOSONAR bit_cast is not supported by the bullseye compiler
        static_cast<socklen_t>(sizeof(sockaddr_in))) < 0) {
        Stop();
        return "Port " + to_string(port) + " is in use, s2p may already be running";
    }

    if (listen(service_socket, 2) == -1) {
        Stop();
        return "Can't listen to service socket: " + string(strerror(errno));
    }

    execute = cb;

    return "";
}

void S2pThread::Start()
{
    assert(service_socket != -1);

#if !defined __FreeBSD__ && !defined __APPLE__
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
    CommandContext context(fd);
    try {
        if (context.ReadCommand()) {
            execute(context);
        }
    }
    catch (const io_exception &e) {
        spdlog::warn(e.what());

        // Try to return an error message (this may fail if the exception was caused when returning the actual result)
        PbResult result;
        result.set_msg(e.what());
        try {
            context.WriteResult(result);
        }
        catch (const io_exception&) { // NOSONAR Ignored because not relevant when writing the result
            // Ignore
        }
    }
}
