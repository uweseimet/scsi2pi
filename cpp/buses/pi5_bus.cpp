//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
//---------------------------------------------------------------------------

#include "pi5_bus.h"
#include <algorithm>
#include <bit>
#include <thread>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <spdlog/spdlog.h>

using namespace spdlog;

Pi5Bus::Pi5Bus(bool standard_board)
{
    if (standard_board) {
        pin_ind = -1;
        pin_tad = -1;
        pin_dtd = -1;
    }
}

string Pi5Bus::SetUp(bool target)
{
    if (pin_ind < 0 && !target) {
        return "Initiator mode requires a FULLSPEC board";
    }

    if (const unsigned int cores = thread::hardware_concurrency(); cores > 3) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(3, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    }

    // The interrupts cannot be disabled like on the other Pis, real-time scheduling is used instead.
    // This requires kernel.sched_rt_runtime_us=-1, otherwise the thread is paused for 50 ms every second.
    sched_param param { };
    param.sched_priority = 99;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    // Unlike /dev/mem, /dev/gpiomem0 does not require root permissions but membership in the gpio group
    const int fd = open("/dev/gpiomem0", O_RDWR | O_SYNC);
    if (fd == -1) {
        return "Can't open /dev/gpiomem0: "s + system_error(errno, generic_category()).what();
    }
    void *map = mmap(nullptr, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (map == MAP_FAILED) {
        return "Can't map RP1 GPIO: "s + system_error(errno, generic_category()).what();
    }
    gpio = span(static_cast<volatile uint32_t*>(map), MAP_SIZE / sizeof(uint32_t));

    // Hand all pins over to software control, some of them default to other functions, e.g. SPI0 and UART0.
    // 12 mA is the maximum drive strength.
    for (int pin = PIN_ACT; pin <= PIN_SEL; ++pin) {
        gpio[GetControlRegister(pin)] = (gpio[GetControlRegister(pin)] & ~FUNCSEL_MASK) | FUNCSEL_SYS_RIO;
    }
    SetPads(PAD_DRIVE_12MA);

    InitializeSignals();

    // Set control signals
    PinSetSignal(PIN_ACT, false);
    PinSetSignal(pin_tad, false);
    PinSetSignal(pin_ind, false);
    PinSetSignal(pin_dtd, false);
    SetSignal(PIN_ACT, true);
    SetSignal(pin_tad, true);
    SetSignal(pin_ind, true);
    SetSignal(pin_dtd, true);

    PinSetSignal(PIN_ENB, false);
    SetSignal(PIN_ENB, true);

    // Initialize SEL signal interrupt, the RP1 is the first GPIO chip
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
        return "Can't create epoll instance";
    }

    epoll_event ev = { };
    ev.events = EPOLLIN | EPOLLPRI;
    ev.data.fd = selevreq.fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, selevreq.fd, &ev) == -1) {
        close(epoll_fd);
        close(selevreq.fd);
        return "Can't add file descriptor to epoll";
    }

    // A data pin is asserted by enabling its output
    for (uint32_t i = 0; i < data_oe.size(); ++i) {
        const uint32_t parity = popcount(i) % 2 == 0 ? 1 : 0;
        data_oe[i] = (i | parity << 8) << PIN_DT0;
    }

    pio = make_unique<Rp1Pio>();
    if (const string &error = pio->Init(gpio); !error.empty()) {
        warn("{}, using slower software handshakes", error);
        pio.reset();
    }

    // Set the initiator signal direction
    PinSetSignal(pin_ind, !target);

    // Set data bus signal directions
    PinSetSignal(pin_dtd, target);

    // Set ENABLE in order to show the user that s2p is running
    PinSetSignal(PIN_ENB, true);

    return "";
}

void Pi5Bus::CleanUp()
{
    pio.reset();

    if (epoll_fd >= 0) {
        close(epoll_fd);
    }

    if (selevreq.fd >= 0) {
        close(selevreq.fd);
    }

    // Set control signals
    PinSetSignal(PIN_ENB, false);
    PinSetSignal(PIN_ACT, false);
    PinSetSignal(pin_tad, false);
    PinSetSignal(pin_ind, false);
    PinSetSignal(pin_dtd, false);
    SetSignal(PIN_ACT, false);
    SetSignal(pin_tad, false);
    SetSignal(pin_ind, false);
    SetSignal(pin_dtd, false);

    InitializeSignals();

    SetPads(PAD_DRIVE_8MA);
}

void Pi5Bus::Reset() const
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

uint8_t Pi5Bus::WaitForSelection()
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

void Pi5Bus::SetBSY(bool state) const
{
    Bus::SetBSY(state);

    PinSetSignal(PIN_ACT, state);
    PinSetSignal(pin_tad, state);
}

void Pi5Bus::SetSEL(bool state) const
{
    Bus::SetSEL(state);

    PinSetSignal(PIN_ACT, state);
}

void Pi5Bus::SetDataDirIn(bool in) const
{
    // Change the data input/output direction according to the IO signal
    PinSetSignal(pin_dtd, !in);

    gpio[RIO_OFFSET + (in ? CLR : SET) + RIO_OUT] = DATA_MASK;
}

int Pi5Bus::TargetSendHandShake(data_out_t buf, int daynaport_delay_after_bytes)
{
    if (!pio || buf.size() < MIN_PIO_BYTES || daynaport_delay_after_bytes != SEND_NO_DELAY) {
        return Bus::TargetSendHandShake(buf, daynaport_delay_after_bytes);
    }

    if (!WaitHandShake(PIN_ACK_MASK, false)) {
        return 0;
    }

    pio_words.resize(buf.size());
    ranges::transform(buf, pio_words.begin(), [this](uint8_t b) { return data_oe[b] >> PIN_DT0; });

    // From here on the PIO drives the data pins
    SetOutputEnable(DATA_MASK, false);

    return pio->Send(pio_words);
}

int Pi5Bus::TargetReceiveHandShake(data_in_t buf)
{
    if (!pio || buf.size() < MIN_PIO_BYTES || buf.size() % 4) {
        return Bus::TargetReceiveHandShake(buf);
    }

    if (!WaitHandShake(PIN_ACK_MASK, false)) {
        return 0;
    }

    // The data pins are inputs during DATA OUT
    SetOutputEnable(DATA_MASK, false);

    return pio->Receive(buf);
}

void Pi5Bus::SetDAT(uint8_t dat) const
{
    // Keep the previous byte on the bus until the initiator has released ACK.
    // Without this the Mac Plus crashes while reading from the disk.
    WaitHandShake(PIN_ACK_MASK, false);

    // Change all data pins with a single atomic XOR of their output enable bits
    gpio[RIO_OFFSET + XOR + RIO_OE] = current_data_oe ^ data_oe[dat];
    current_data_oe = data_oe[dat];
}

void Pi5Bus::InitializeSignals() const
{
    for (const int pin : SIGNAL_TABLE) {
        PinSetSignal(pin, false);
        SetSignal(pin, false);
        gpio[PADS_OFFSET + 1 + pin] = gpio[PADS_OFFSET + 1 + pin] & ~PAD_PULL_MASK;
    }
}

void Pi5Bus::SetSignal(int pin, bool state) const
{
    if (pin >= 0) {
        SetOutputEnable(1U << pin, state);
    }
}

void Pi5Bus::PinSetSignal(int pin, bool state) const
{
    if (pin >= 0) {
        gpio[RIO_OFFSET + (state ? SET : CLR) + RIO_OUT] = 1U << pin;
    }
}

void Pi5Bus::SetOutputEnable(uint32_t mask, bool enable) const
{
    gpio[RIO_OFFSET + (enable ? SET : CLR) + RIO_OE] = mask;

    if (enable) {
        current_data_oe |= mask & DATA_MASK;
    }
    else {
        current_data_oe &= ~mask;
    }
}

void Pi5Bus::SetPads(uint32_t drive) const
{
    for (int pin = PIN_ACT; pin <= PIN_SEL; ++pin) {
        gpio[PADS_OFFSET + 1 + pin] = (gpio[PADS_OFFSET + 1 + pin] & ~(PAD_OUTPUT_DISABLE | PAD_DRIVE_MASK))
            | PAD_INPUT_ENABLE | drive;
    }
}

// The RP1 has no ARM timer, but the Pi 5 has a fast steady clock
void Pi5Bus::WaitNanoSeconds(bool daynaport) const
{
    const auto deadline = chrono::steady_clock::now() + (daynaport ? DAYNAPORT_DELAY : BUS_SETTLE_DELAY);
    while (chrono::steady_clock::now() < deadline) {
        // Intentionally empty
    }
}
