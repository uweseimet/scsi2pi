//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "gpio_bus.h"
#include <system_error>
#include <thread>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <spdlog/spdlog.h>

using namespace spdlog;

GpioBus::GpioBus(bool standard_board)
{
    if (standard_board) {
        pin_ind = -1;
        pin_tad = -1;
        pin_dtd = -1;
    }
}

string GpioBus::SetUp(bool target)
{
    if (pin_ind < 0 && !target) {
        return "Initiator mode requires a FULLSPEC board";
    }

    // On the Pi5, pinning the CPU is not required, but it can reduce jitter
    if (const unsigned int cores = thread::hardware_concurrency(); cores > 3) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(3, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    }

    return "";
}

string GpioBus::SetUpSelectionEvent()
{
    const int chip_fd = open("/dev/gpiochip0", 0);
    if (chip_fd == -1) {
        return "Can't open /dev/gpiochip0. If s2p is running (e.g. as a service), shut it down first.";
    }

    // Event request setting
    "SCSI2Pi"sv.copy(selevreq.consumer_label, sizeof(selevreq.consumer_label) - 1);
    selevreq.lineoffset = PIN_SEL;
    selevreq.handleflags = GPIOHANDLE_REQUEST_INPUT;
    selevreq.eventflags = GPIOEVENT_REQUEST_FALLING_EDGE;
    selevreq.fd = -1;

    if (ioctl(chip_fd, GPIO_GET_LINEEVENT_IOCTL, &selevreq) == -1) {
        close(chip_fd);
        return "Can't register event request. If s2p is running (e.g. as a service), shut it down first.";
    }
    close(chip_fd);

    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        close(selevreq.fd);
        selevreq.fd = -1;
        return "Can't create epoll instance";
    }

    epoll_event ev = { };
    ev.events = EPOLLIN | EPOLLPRI;
    ev.data.fd = selevreq.fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, selevreq.fd, &ev) == -1) {
        close(epoll_fd);
        epoll_fd = -1;
        close(selevreq.fd);
        selevreq.fd = -1;
        return "Can't add file descriptor to epoll";
    }

    return "";
}

void GpioBus::CleanUp()
{
    if (epoll_fd >= 0) {
        close(epoll_fd);
        epoll_fd = -1;
    }

    if (selevreq.fd >= 0) {
        close(selevreq.fd);
        selevreq.fd = -1;
    }
}

void GpioBus::Reset() const
{
    Bus::Reset();

    // Turn off active signal
    PinSetSignal(PIN_ACT, false);

    // Set all signals to off
    for (const int pin : SIGNAL_TABLE) {
        SetSignal(pin, false);
    }

    // Set target signal to input for all modes
    PinSetSignal(pin_tad, false);
}

uint8_t GpioBus::WaitForSelection()
{
    if (epoll_event epev = { }; epoll_wait(epoll_fd, &epev, 1, -1) == -1) {
        if (errno != EINTR) {
            warn("epoll_wait failed: {}", system_error(errno, generic_category()).what());
        }

        return 0;
    }

    if (gpioevent_data gpev = { }; read(selevreq.fd, &gpev, sizeof(gpev)) == -1) {
        if (errno != EINTR) {
            warn("Reading event failed: {}", system_error(errno, generic_category()).what());
        }

        return 0;
    }

    return GetSelection();
}

void GpioBus::SetBSY(bool state) const
{
    Bus::SetBSY(state);

    PinSetSignal(PIN_ACT, state);
    PinSetSignal(pin_tad, state);
}

void GpioBus::SetSEL(bool state) const
{
    Bus::SetSEL(state);

    PinSetSignal(PIN_ACT, state);
}

void GpioBus::SetDataDirIn(bool in) const
{
    // Change the data input/output direction according to the IO signal. The derived classes
    // additionally have to switch the data pins.
    PinSetSignal(pin_dtd, !in);
}
