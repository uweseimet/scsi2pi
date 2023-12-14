//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <functional>
#include <thread>
#include <string>

class CommandContext;

using namespace std;

class S2pThread
{
    using callback = function<bool(CommandContext&)>;

public:

    S2pThread() = default;
    ~S2pThread() = default;

    string Init(const callback&, int);
    void Start();
    void Stop();
    bool IsRunning() const
    {
        return service_socket != -1 && service_thread.joinable();
    }

private:

    void Execute() const;
    void ExecuteCommand(int) const;

    callback execute;

#if !defined __FreeBSD__ && !defined __APPLE__
    jthread service_thread;
    #else
	thread service_thread;
#endif

    int service_socket = -1;
};
