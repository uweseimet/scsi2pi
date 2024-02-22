//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <map>
#include <fstream>
#include <cstring>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <spdlog/spdlog.h>
#include "rpi_bus.h"

using namespace spdlog;

bool RpiBus::Init(bool target)
{
    GpioBus::Init(target);

    // Determine the Raspberry Pi type from the base address
    const auto base_addr = GetPeripheralAddress();
    switch (base_addr) {
    case 0xfe000000:
        pi_type = PiType::pi_4;
        break;

    case 0x3f000000:
        pi_type = PiType::pi_2_3;
        break;

    default:
        pi_type = PiType::pi_1;
        break;
    }

    trace("Detected Raspberry Pi type {}", static_cast<int>(pi_type));

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        critical("Can't open /dev/mem: {}", strerror(errno));
        return false;
    }

    // Map peripheral region memory
    auto *map = static_cast<uint32_t*>(mmap(nullptr, 0x1000100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_addr));
    if (map == MAP_FAILED) {
        critical("Can't map memory: {}", strerror(errno));
        close(fd);
        return false;
    }

    // RPI Mailbox property interface
    // Get max clock rate
    //  Tag: 0x00030004
    //
    //  Request: Length: 4
    //   Value: u32: clock id
    //  Response: Length: 8
    //   Value: u32: clock id, u32: rate (in Hz)
    //
    // Clock id
    //  0x000000004: CORE

    // Get the core frequency
    const array<uint32_t, 32> maxclock = { 32, 0, 0x00030004, 8, 0, 4, 0, 0 };
    if (const int vcio_fd = open("/dev/vcio", O_RDONLY); vcio_fd != -1) {
        ioctl(vcio_fd, _IOWR(100, 0, char*), maxclock.data());
        corefreq = maxclock[6] / 1000000;
        close(vcio_fd);
    }
    else {
        critical("Can't open /dev/vcio: {}", strerror(errno));
        return false;
    }

    armtaddr = map + ARMT_OFFSET / sizeof(uint32_t);

    // Change the ARM timer to free run mode
    armtaddr[ARMT_CTRL] = 0x00000282;

    // GPIO
    gpio = map + GPIO_OFFSET / sizeof(uint32_t);
    level = &gpio[GPIO_LEV_0];

    // PADS
    pads = map + PADS_OFFSET / sizeof(uint32_t);

    // Interrupt controller
    irpctl = map + IRPT_OFFSET / sizeof(uint32_t);

    // Quad-A7 control
    qa7regs = map + QA7_OFFSET / sizeof(uint32_t);

    if (pi_type == PiType::pi_4) {
        map = static_cast<uint32_t*>(mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_SHARED, fd, ARM_GICD_BASE));
        if (map == MAP_FAILED) {
            critical("Can't map GIC memory: {}", strerror(errno));
            close(fd);
            return false;
        }
        gicc = map + (ARM_GICC_BASE - ARM_GICD_BASE) / sizeof(uint32_t);
    }

    close(fd);

    // Set Drive Strength to 16mA
    SetSignalDriveStrength(7);

    // Set pull up/pull down
#if SIGNAL_CONTROL_MODE == 0
    int pullmode = GPIO_PULLNONE;
#elif SIGNAL_CONTROL_MODE == 1
    int pullmode = GPIO_PULLUP;
#else
    int pullmode = GPIO_PULLDOWN;
#endif

    // Initialize all signals
    for (const int signal : SignalTable) {
        PinSetSignal(signal, false);
        PinConfig(signal, GPIO_INPUT);
        PullConfig(signal, pullmode);
    }

    // Set control signals
    PinSetSignal(PIN_ACT, false);
    PinSetSignal(PIN_TAD, false);
    PinSetSignal(PIN_IND, false);
    PinSetSignal(PIN_DTD, false);
    PinConfig(PIN_ACT, GPIO_OUTPUT);
    PinConfig(PIN_TAD, GPIO_OUTPUT);
    PinConfig(PIN_IND, GPIO_OUTPUT);
    PinConfig(PIN_DTD, GPIO_OUTPUT);

    // Set the ENABLE signal
    // This is used to show that the application is running
    PinSetSignal(PIN_ENB, ENB_OFF);
    PinConfig(PIN_ENB, GPIO_OUTPUT);

    // GPIO Function Select (GPFSEL) registers backup
    gpfsel[0] = gpio[GPIO_FSEL_0];
    gpfsel[1] = gpio[GPIO_FSEL_1];
    gpfsel[2] = gpio[GPIO_FSEL_2];
    gpfsel[3] = gpio[GPIO_FSEL_3];

    // Initialize SEL signal interrupt
    fd = open("/dev/gpiochip0", 0);
    if (fd == -1) {
        critical("Can't open /dev/gpiochip0. If s2p is running (e.g. as a service), shut it down first.");
        return false;
    }

#ifdef __linux__
    // Event request setting
    strcpy(selevreq.consumer_label, "SCSI2Pi"); // NOSONAR Using strcpy is safe
    selevreq.lineoffset = PIN_SEL;
    selevreq.handleflags = GPIOHANDLE_REQUEST_INPUT;
#if SIGNAL_CONTROL_MODE < 2
    selevreq.eventflags = GPIOEVENT_REQUEST_FALLING_EDGE;
#else
    selevreq.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
#endif

    if (ioctl(fd, GPIO_GET_LINEEVENT_IOCTL, &selevreq) == -1) {
        critical("Can't register event request. If s2p is running (e.g. as a service), shut it down first.");
        close(fd);
        return false;
    }
    close(fd);

    epoll_fd = epoll_create(1);
    epoll_event ev = { };
    ev.events = EPOLLIN | EPOLLPRI;
    ev.data.fd = selevreq.fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, selevreq.fd, &ev);
#endif

    CreateWorkTable();

    // Enable ENABLE in ordert to show the user that s2p is running
    SetControl(PIN_ENB, ENB_ON);

    return true;
}

void RpiBus::CleanUp()
{
    // Release SEL signal interrupt
#ifdef __linux__
    close(selevreq.fd);
#endif

    // Set control signals
    PinSetSignal(PIN_ENB, false);
    PinSetSignal(PIN_ACT, false);
    PinSetSignal(PIN_TAD, false);
    PinSetSignal(PIN_IND, false);
    PinSetSignal(PIN_DTD, false);
    PinConfig(PIN_ACT, GPIO_INPUT);
    PinConfig(PIN_TAD, GPIO_INPUT);
    PinConfig(PIN_IND, GPIO_INPUT);
    PinConfig(PIN_DTD, GPIO_INPUT);

    // Initialize all signals
    for (const int signal : SignalTable) {
        PinSetSignal(signal, false);
        PinConfig(signal, GPIO_INPUT);
        PullConfig(signal, GPIO_PULLNONE);
    }

    // Set drive strength back to 8mA
    SetSignalDriveStrength(3);
}

void RpiBus::Reset()
{
    // Turn off active signal
    SetControl(PIN_ACT, false);

    // Set all signals to off
    for (const int signal : SignalTable) {
        SetSignal(signal, false);
    }

    // Set target signal to input for all modes
    SetControl(PIN_TAD, TAD_IN);
    SetMode(PIN_BSY, IN);
    SetMode(PIN_MSG, IN);
    SetMode(PIN_CD, IN);
    SetMode(PIN_REQ, IN);
    SetMode(PIN_IO, IN);

    if (IsTarget()) {
        // Set the initiator signal to input
        SetControl(PIN_IND, IND_IN);
        SetMode(PIN_SEL, IN);
        SetMode(PIN_ATN, IN);
        SetMode(PIN_ACK, IN);
        SetMode(PIN_RST, IN);

        // Set data bus signals to input
        SetControl(PIN_DTD, DTD_IN);
        SetMode(PIN_DT0, IN);
        SetMode(PIN_DT1, IN);
        SetMode(PIN_DT2, IN);
        SetMode(PIN_DT3, IN);
        SetMode(PIN_DT4, IN);
        SetMode(PIN_DT5, IN);
        SetMode(PIN_DT6, IN);
        SetMode(PIN_DT7, IN);
    } else {
        // Set the initiator signal to output
        SetControl(PIN_IND, IND_OUT);
        SetMode(PIN_SEL, OUT);
        SetMode(PIN_ATN, OUT);
        SetMode(PIN_ACK, OUT);
        SetMode(PIN_RST, OUT);

        // Set the data bus signals to output
        SetControl(PIN_DTD, DTD_OUT);
        SetMode(PIN_DT0, OUT);
        SetMode(PIN_DT1, OUT);
        SetMode(PIN_DT2, OUT);
        SetMode(PIN_DT3, OUT);
        SetMode(PIN_DT4, OUT);
        SetMode(PIN_DT5, OUT);
        SetMode(PIN_DT6, OUT);
        SetMode(PIN_DT7, OUT);
    }

    // Initialize all signals
    signals = 0;
}

bool RpiBus::WaitForSelection()
{
#ifndef __linux__
    return false;
#else
    errno = 0;

    if (epoll_event epev; epoll_wait(epoll_fd, &epev, 1, -1) == -1) {
        if (errno != EINTR) {
            warn("epoll_wait failed: {}", strerror(errno));
        }
        return false;
    }

    if (gpioevent_data gpev; read(selevreq.fd, &gpev, sizeof(gpev)) == -1) {
        if (errno != EINTR) {
            warn("Event read failed: {}", strerror(errno));
        }
        return false;
    }
#endif

    Acquire();

    return true;
}

bool RpiBus::GetBSY() const
{
    return GetSignal(PIN_BSY);
}

void RpiBus::SetBSY(bool state)
{
    SetSignal(PIN_BSY, state);

    if (state) {
        SetControl(PIN_ACT, true);

        SetControl(PIN_TAD, TAD_OUT);

        SetMode(PIN_BSY, OUT);
        SetMode(PIN_MSG, OUT);
        SetMode(PIN_CD, OUT);
        SetMode(PIN_REQ, OUT);
        SetMode(PIN_IO, OUT);
    } else {
        SetControl(PIN_ACT, false);

        SetControl(PIN_TAD, TAD_IN);

        SetMode(PIN_BSY, IN);
        SetMode(PIN_MSG, IN);
        SetMode(PIN_CD, IN);
        SetMode(PIN_REQ, IN);
        SetMode(PIN_IO, IN);
    }
}

bool RpiBus::GetSEL() const
{
    return GetSignal(PIN_SEL);
}

void RpiBus::SetSEL(bool state)
{
    if (!IsTarget() && state) {
        SetControl(PIN_ACT, true);
    }

    SetSignal(PIN_SEL, state);
}

bool RpiBus::GetATN() const
{
    return GetSignal(PIN_ATN);
}

void RpiBus::SetATN(bool state)
{
    SetSignal(PIN_ATN, state);
}

bool RpiBus::GetACK() const
{
    return GetSignal(PIN_ACK);
}

void RpiBus::SetACK(bool state)
{
    SetSignal(PIN_ACK, state);
}

bool RpiBus::GetRST() const
{
    return GetSignal(PIN_RST);
}

void RpiBus::SetRST(bool state)
{
    SetSignal(PIN_RST, state);
}

bool RpiBus::GetMSG() const
{
    return GetSignal(PIN_MSG);
}

void RpiBus::SetMSG(bool state)
{
    SetSignal(PIN_MSG, state);
}

bool RpiBus::GetCD() const
{
    return GetSignal(PIN_CD);
}

void RpiBus::SetCD(bool state)
{
    SetSignal(PIN_CD, state);
}

bool RpiBus::GetIO()
{
    bool state = GetSignal(PIN_IO);

    if (!IsTarget()) {
        // Change the data input/output direction by IO signal
        if (state) {
            SetControl(PIN_DTD, DTD_IN);
            SetMode(PIN_DT0, IN);
            SetMode(PIN_DT1, IN);
            SetMode(PIN_DT2, IN);
            SetMode(PIN_DT3, IN);
            SetMode(PIN_DT4, IN);
            SetMode(PIN_DT5, IN);
            SetMode(PIN_DT6, IN);
            SetMode(PIN_DT7, IN);
        } else {
            SetControl(PIN_DTD, DTD_OUT);
            SetMode(PIN_DT0, OUT);
            SetMode(PIN_DT1, OUT);
            SetMode(PIN_DT2, OUT);
            SetMode(PIN_DT3, OUT);
            SetMode(PIN_DT4, OUT);
            SetMode(PIN_DT5, OUT);
            SetMode(PIN_DT6, OUT);
            SetMode(PIN_DT7, OUT);
        }
    }

    return state;
}

void RpiBus::SetIO(bool state)
{
    assert(IsTarget());

    SetSignal(PIN_IO, state);

    // Change the data input/output direction by IO signal
    if (state) {
        SetControl(PIN_DTD, DTD_OUT);
        SetDAT(0);
        SetMode(PIN_DT0, OUT);
        SetMode(PIN_DT1, OUT);
        SetMode(PIN_DT2, OUT);
        SetMode(PIN_DT3, OUT);
        SetMode(PIN_DT4, OUT);
        SetMode(PIN_DT5, OUT);
        SetMode(PIN_DT6, OUT);
        SetMode(PIN_DT7, OUT);
    } else {
        SetControl(PIN_DTD, DTD_IN);
        SetMode(PIN_DT0, IN);
        SetMode(PIN_DT1, IN);
        SetMode(PIN_DT2, IN);
        SetMode(PIN_DT3, IN);
        SetMode(PIN_DT4, IN);
        SetMode(PIN_DT5, IN);
        SetMode(PIN_DT6, IN);
        SetMode(PIN_DT7, IN);
    }
}

bool RpiBus::GetREQ() const
{
    return GetSignal(PIN_REQ);
}

void RpiBus::SetREQ(bool state)
{
    SetSignal(PIN_REQ, state);
}

inline uint8_t RpiBus::GetDAT()
{
    return static_cast<uint8_t>(Acquire() >> PIN_DT0);
}

void RpiBus::SetDAT(uint8_t dat)
{
    // Write to port
#if SIGNAL_CONTROL_MODE == 0
    uint32_t fsel = gpfsel[0];
    fsel &= tblDatMsk[0][dat];
    fsel |= tblDatSet[0][dat];
    gpfsel[0] = fsel;
    gpio[GPIO_FSEL_0] = fsel;

    fsel = gpfsel[1];
    fsel &= tblDatMsk[1][dat];
    fsel |= tblDatSet[1][dat];
    gpfsel[1] = fsel;
    gpio[GPIO_FSEL_1] = fsel;

    fsel = gpfsel[2];
    fsel &= tblDatMsk[2][dat];
    fsel |= tblDatSet[2][dat];
    gpfsel[2] = fsel;
    gpio[GPIO_FSEL_2] = fsel;
#else
    gpio[GPIO_CLR_0] = tblDatMsk[dat];
    gpio[GPIO_SET_0] = tblDatSet[dat];
#endif
}

const array<int, 19> RpiBus::SignalTable = { PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4, PIN_DT5, PIN_DT6,
    PIN_DT7, PIN_DP, PIN_SEL, PIN_ATN, PIN_RST, PIN_ACK, PIN_BSY,
    PIN_MSG, PIN_CD, PIN_IO, PIN_REQ };

void RpiBus::CreateWorkTable(void)
{
    const array<int, 9> pintbl = { PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4, PIN_DT5, PIN_DT6, PIN_DT7, PIN_DP };

    array<bool, 256> tblParity;

    // Create parity table
    for (uint32_t i = 0; i < 0x100; i++) {
        uint32_t bits = i;
        uint32_t parity = 0;
        for (int j = 0; j < 8; j++) {
            parity ^= bits & 1;
            bits >>= 1;
        }
        parity = ~parity;
        tblParity[i] = parity & 1;
    }

#if SIGNAL_CONTROL_MODE == 0
    // Mask and setting data generation
    for (auto &tbl : tblDatMsk) {
        tbl.fill(-1);
    }
    for (auto &tbl : tblDatSet) {
        tbl.fill(0);
    }

    for (uint32_t i = 0; i < 0x100; i++) {
        // Bit string for inspection
        uint32_t bits = i;

        // Get parity
        if (tblParity[i]) {
            bits |= (1 << 8);
        }

        // Bit check
        for (int j = 0; j < 9; j++) {
            // Index and shift amount calculation
            int index = pintbl[j] / 10;
            int shift = (pintbl[j] % 10) * 3;

            // Mask data
            tblDatMsk[index][i] &= ~(0x7 << shift);

            // Setting data
            if (bits & 1) {
                tblDatSet[index][i] |= (1 << shift);
            }

            bits >>= 1;
        }
    }
#else
    for (uint32_t i = 0; i < 0x100; i++) {
        // Bit string for inspection
        uint32_t bits = i;

        // Get parity
        if (tblParity[i]) {
            bits |= (1 << 8);
        }

#if SIGNAL_CONTROL_MODE == 1
        // Negative logic is inverted
        bits = ~bits;
#endif

        // Create GPIO register information
        uint32_t gpclr = 0;
        uint32_t gpset = 0;
        for (int j = 0; j < 9; j++) {
            if (bits & 1) {
                gpset |= (1 << pintbl[j]);
            } else {
                gpclr |= (1 << pintbl[j]);
            }
            bits >>= 1;
        }

        tblDatMsk[i] = gpclr;
        tblDatSet[i] = gpset;
    }
#endif
}

void RpiBus::SetControl(int pin, bool state)
{
    PinSetSignal(pin, state);
}

//---------------------------------------------------------------------------
//
//	Input/output mode setting
//
// Set direction fo pin (IN / OUT)
//   Used with: TAD, BSY, MSG, CD, REQ, O, SEL, IND, ATN, ACK, RST, DT*
//
//---------------------------------------------------------------------------
void RpiBus::SetMode(int pin, int mode)
{
#if SIGNAL_CONTROL_MODE == 0
    if (mode == OUT) {
        return;
    }
#endif

    const int index = pin / 10;
    const int shift = (pin % 10) * 3;
    uint32_t data = gpfsel[index];
    data &= ~(0x7 << shift);
    if (mode == OUT) {
        data |= (1 << shift);
    }
    gpio[index] = data;
    gpfsel[index] = data;
}

// Get input signal value
bool RpiBus::GetSignal(int pin) const
{
    return (signals >> pin) & 1;
}

//---------------------------------------------------------------------------
//
//	Set output signal value
//
//  Sets the output value. Used with:
//     PIN_ENB, ACT, TAD, IND, DTD, BSY, SignalTable
//
//---------------------------------------------------------------------------
void RpiBus::SetSignal(int pin, bool state)
{
#if SIGNAL_CONTROL_MODE == 0
    const int index = pin / 10;
    const int shift = (pin % 10) * 3;
    uint32_t data = gpfsel[index];
    if (state) {
        data |= (1 << shift);
    } else {
        data &= ~(0x7 << shift);
    }
    gpio[index] = data;
    gpfsel[index] = data;
#elif SIGNAL_CONTROL_MODE == 1
    if (state) {
        gpio[GPIO_CLR_0] = 0x1 << pin;
    } else {
        gpio[GPIO_SET_0] = 0x1 << pin;
    }
#elif SIGNAL_CONTROL_MODE == 2
    if (state) {
        gpio[GPIO_SET_0] = 0x1 << pin;
    } else {
        gpio[GPIO_CLR_0] = 0x1 << pin;
    }
#endif
}

void RpiBus::DisableIRQ()
{
#ifdef __linux__
    switch (pi_type) {
    case PiType::pi_4:
        // RPI4 disables interrupts via the GIC
        giccpmr = gicc[GICC_PMR];
        gicc[GICC_PMR] = 0;
        break;

    case PiType::pi_2_3:
        // RPI2,3 disable core timer IRQ
        tintcore = sched_getcpu() + QA7_CORE0_TINTC;
        tintctl = qa7regs[tintcore];
        qa7regs[tintcore] = 0;
        break;

    default:
        // Stop system timer interrupt with interrupt controller
        irptenb = irpctl[IRPT_ENB_IRQ_1];
        irpctl[IRPT_DIS_IRQ_1] = irptenb & 0xf;
        break;
    }
#endif
}

void RpiBus::EnableIRQ()
{
#ifdef __linux__
    switch (pi_type) {
    case PiType::pi_4:
        // RPI4 enables interrupts via the GIC
        gicc[GICC_PMR] = giccpmr;
        break;

    case PiType::pi_2_3:
        // RPI2,3 re-enable core timer IRQ
        qa7regs[tintcore] = tintctl;
        break;

    default:
        // Restart the system timer interrupt with the interrupt controller
        irpctl[IRPT_ENB_IRQ_1] = irptenb & 0xf;
        break;
    }
#endif
}

//---------------------------------------------------------------------------
//
//	Pin direction setting (input/output)
//
// Used in Init() for ACT, TAD, IND, DTD, ENB to set direction (GPIO_OUTPUT vs GPIO_INPUT)
// Also used on SignalTable
// Only used in Init and Cleanup. Reset uses SetMode
//---------------------------------------------------------------------------
void RpiBus::PinConfig(int pin, int mode)
{
    // Check for invalid pin
    if (pin < 0) {
        return;
    }

    const int index = pin / 10;
    uint32_t mask = ~(0x7 << ((pin % 10) * 3));
    gpio[index] = (gpio[index] & mask) | ((mode & 0x7) << ((pin % 10) * 3));
}

// Pin pull-up/pull-down setting
void RpiBus::PullConfig(int pin, int mode)
{
    // Check for invalid pin
    if (pin < 0) {
        return;
    }

    if (pi_type == PiType::pi_4) {
        uint32_t pull;
        switch (mode) {
        case GPIO_PULLNONE:
            pull = 0;
            break;

        case GPIO_PULLUP:
            pull = 1;
            break;

        case GPIO_PULLDOWN:
            pull = 2;
            break;

        default:
            return;
        }

        pin &= 0x1f;
        const int shift = (pin & 0xf) << 1;
        uint32_t bits = gpio[GPIO_PUPPDN0 + (pin >> 4)];
        bits &= ~(3 << shift);
        bits |= (pull << shift);
        gpio[GPIO_PUPPDN0 + (pin >> 4)] = bits;
    } else {
        // 2 us
        const timespec ts = { .tv_sec = 0, .tv_nsec = 2'000 };

        pin &= 0x1f;
        gpio[GPIO_PUD] = mode & 0x3;
        nanosleep(&ts, nullptr);
        gpio[GPIO_CLK_0] = 0x1 << pin;
        nanosleep(&ts, nullptr);
        gpio[GPIO_PUD] = 0;
        gpio[GPIO_CLK_0] = 0;
    }
}

// Set output pin
void RpiBus::PinSetSignal(int pin, bool state)
{
    // Check for invalid pin
    if (pin >= 0) {
        gpio[state ? GPIO_SET_0 : GPIO_CLR_0] = 1 << pin;
    }
}

void RpiBus::SetSignalDriveStrength(uint32_t drive)
{
    const uint32_t data = pads[PAD_0_27];
    pads[PAD_0_27] = (0xfffffff8 & data) | drive | 0x5a000000;
}

// Read date byte from bus
inline uint32_t RpiBus::Acquire()
{
    signals = *level;

#if SIGNAL_CONTROL_MODE < 2
    // Invert if negative logic (internal processing is unified to positive logic)
    signals = ~signals;
#endif

    return signals;
}

// Wait until the signal line stabilizes (400 ns bus settle delay).
// nanosleep() does not provide the required resolution, which causes issues when reading data from the bus.
void RpiBus::WaitBusSettle() const
{
    if (const uint32_t diff = corefreq * 400 / 1000; diff) {
        const uint32_t start = armtaddr[ARMT_FREERUN];
        while (armtaddr[ARMT_FREERUN] - start < diff) {
            // Intentionally empty
        }
    }
}

uint32_t RpiBus::GetDtRanges(const string &filename, uint32_t offset)
{
    if (ifstream in(filename, ios::binary); in.good()) {
        in.seekg(offset, ios::beg);
        array<char, 4> buf;
        in.read(buf.data(), buf.size());
        if (in.good()) {
            return (int)buf[0] << 24 | (int)buf[1] << 16 | (int)buf[2] << 8 | (int)buf[3] << 0;
        }
    }

    return ~0;
}

uint32_t RpiBus::GetPeripheralAddress()
{
    uint32_t address = GetDtRanges("/proc/device-tree/soc/ranges", 4);
    if (!address) {
        address = GetDtRanges("/proc/device-tree/soc/ranges", 8);
    }
    return address == (uint32_t)~0 ? 0x20000000 : address;
}
