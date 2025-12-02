//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "rpi_bus.h"
#include <fstream>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <spdlog/spdlog.h>

using namespace spdlog;

bool RpiBus::Init(bool target)
{
    Bus::Init(target);

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        critical("Root permissions are required");
        return false;
    }

    off_t base_addr = 0;
    uint32_t gpio_offset = GPIO_OFFSET;
    uint32_t pads_offset = PADS_OFFSET;
    switch (pi_type) {
    case RpiBus::PiType::PI_1:
        base_addr = 0x20000000;
        break;

    case RpiBus::PiType::PI_2:
    case RpiBus::PiType::PI_3:
        base_addr = 0x3f000000;
        break;

    case RpiBus::PiType::PI_4:
        base_addr = 0xfe000000;
        break;

    case RpiBus::PiType::PI_5:
        base_addr = 0x1f00000000;
        gpio_offset = GPIO_OFFSET_RP1;
        pads_offset = PADS_OFFSET_RP1;
        break;

    default:
        assert(false);
        break;
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
        const uint32_t timer_core_freq = maxclock[6] / 1'000'000;
        close(vcio_fd);
        bus_settle_count = timer_core_freq * 400 / 1000;
        daynaport_count = timer_core_freq * DAYNAPORT_SEND_DELAY_NS / 1000;
    }
    else {
        critical("Can't open /dev/vcio: {}", strerror(errno));
        return false;
    }

    armt_addr = map + ARMT_OFFSET / sizeof(uint32_t);

    // Change the ARM timer to free run mode
    armt_addr[ARMT_CTRL] = 0x00000282;

    // GPIO
    gpio = map + gpio_offset / sizeof(uint32_t);
    level = &gpio[GPIO_LEV_0];

    // PADS
    pads = map + pads_offset / sizeof(uint32_t);

    // Interrupt controller (Pi 1)
    irp_ctl = map + IRPT_OFFSET / sizeof(uint32_t);

    // Quad-A7 control (Pi 2/3)
    qa7_regs = map + QA7_OFFSET / sizeof(uint32_t);

    // Map GIC interrupt priority mask register
    if (pi_type == PiType::PI_4) {
        gicc_mpr = static_cast<uint32_t*>(mmap(nullptr, 8, PROT_READ | PROT_WRITE, MAP_SHARED, fd, PI4_ARM_GICC_CTLR));
        if (gicc_mpr == MAP_FAILED) {
            critical("Can't map GIC: {}", strerror(errno));
            close(fd);
            return false;
        }
        // MPR has offset 1
        ++gicc_mpr;
    }

    close(fd);

    // Set Drive Strength to 16mA
    SetSignalDriveStrength(7);

    InitializeSignals();

    // Set control signals
    PinSetSignal(PIN_ACT, false);
    PinSetSignal(PIN_TAD, false);
    PinSetSignal(PIN_IND, false);
    PinSetSignal(PIN_DTD, false);
    PinConfig(PIN_ACT, GPIO_OUTPUT);
    PinConfig(PIN_TAD, GPIO_OUTPUT);
    PinConfig(PIN_IND, GPIO_OUTPUT);
    PinConfig(PIN_DTD, GPIO_OUTPUT);

    PinSetSignal(PIN_ENB, OFF);
    PinConfig(PIN_ENB, GPIO_OUTPUT);

    // GPIO Function Select (GPFSEL) registers copy
    gpfsel[GPIO_FSEL_0] = gpio[GPIO_FSEL_0];
    gpfsel[GPIO_FSEL_1] = gpio[GPIO_FSEL_1];
    gpfsel[GPIO_FSEL_2] = gpio[GPIO_FSEL_2];

    // Initialize SEL signal interrupt
    fd = open("/dev/gpiochip0", 0);
    if (fd == -1) {
        critical("Can't open /dev/gpiochip0. If s2p is running (e.g. as a service), shut it down first.");
        return false;
    }

    // Event request setting
    strcpy(selevreq.consumer_label, "SCSI2Pi"); // NOSONAR Using strcpy is safe
    selevreq.lineoffset = PIN_SEL;
    selevreq.handleflags = GPIOHANDLE_REQUEST_INPUT;
    selevreq.eventflags = GPIOEVENT_REQUEST_FALLING_EDGE;

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

    CreateWorkTable();

    // Enable ENABLE in order to show the user that s2p is running
    SetControl(PIN_ENB, ON);

    return true;
}

void RpiBus::CleanUp()
{
    if (irq_disabled) {
        EnableIRQ();
    }

    // Release SEL signal interrupt
    close(selevreq.fd);

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

    InitializeSignals();

    // Set drive strength back to 8mA
    SetSignalDriveStrength(3);
}

void RpiBus::Reset()
{
    Bus::Reset();

    // Turn off active signal
    SetControl(PIN_ACT, false);

    // Set all signals to off
    for (const int s : SIGNAL_TABLE) {
        SetSignal(s, false);
    }

    // Set target signal to input for all modes
    SetControl(PIN_TAD, TAD_IN);
    SetSignal(PIN_BSY, false);
    SetSignal(PIN_MSG, false);
    SetSignal(PIN_CD, false);
    SetSignal(PIN_REQ, false);
    SetSignal(PIN_IO, false);

    // Set the initiator signal direction
    SetControl(PIN_IND, IsTarget() ? IND_IN : IND_OUT);

    // Set data bus signal directions
    SetControl(PIN_DTD, IsTarget() ? DTD_IN : DTD_OUT);

    // Set the initiator signal to input
    if (IsTarget()) {
        SetSignal(PIN_SEL, false);
        SetSignal(PIN_ATN, false);
        SetSignal(PIN_ACK, false);
        SetSignal(PIN_RST, false);
    }
}

uint8_t RpiBus::WaitForSelection()
{
    if (epoll_event epev; epoll_wait(epoll_fd, &epev, 1, -1) == -1) {
        if (errno != EINTR) {
            warn("epoll_wait failed: {}", strerror(errno));
        }
        return 0;
    }

    if (gpioevent_data gpev; read(selevreq.fd, &gpev, sizeof(gpev)) == -1) {
        if (errno != EINTR) {
            warn("Event read failed: {}", strerror(errno));
        }
        return 0;
    }

    if (WaitForNotBusy()) {
        DisableIRQ();

        // Initiator and target ID
        const uint8_t ids = GetDAT();

        EnableIRQ();

        return ids;
    }

    return 0;
}

void RpiBus::SetBSY(bool state)
{
    SetSignal(PIN_BSY, state);

    SetControl(PIN_ACT, state);
    SetControl(PIN_TAD, state ? TAD_OUT : TAD_IN);

    if (!state) {
        SetSignal(PIN_BSY, false);
        SetSignal(PIN_MSG, false);
        SetSignal(PIN_CD, false);
        SetSignal(PIN_REQ, false);
        SetSignal(PIN_IO, false);
    }
}

void RpiBus::SetSEL(bool state)
{
    assert(!IsTarget());

    SetControl(PIN_ACT, state);
    SetSignal(PIN_SEL, state);
}

bool RpiBus::GetIO()
{
    const bool state = GetSignal(PIN_IO_MASK);

    if (!IsTarget()) {
        // Change the data input/output direction by IO signal
        SetControl(PIN_DTD, state ? DTD_IN : DTD_OUT);

        if (state) {
            for (const int pin : DATA_PINS) {
                SetSignal(pin, false);
            }
        }
    }

    return state;
}

void RpiBus::SetIO(bool state)
{
    assert(IsTarget());

    SetSignal(PIN_IO, state);

// Change the data input/output direction by IO signal
    SetControl(PIN_DTD, state ? DTD_OUT : DTD_IN);

    if (!state) {
        for (int pin : DATA_PINS) {
            SetSignal(pin, false);
        }
    }
}

inline void RpiBus::SetDAT(uint8_t dat)
{
    uint32_t fsel = gpfsel[GPIO_FSEL_1];
    // Mask for the DT0-DT7 and DP pins
    fsel &= 0b11111000000000000000000000000000;
    fsel |= tblDatSet[1][dat];
    gpfsel[GPIO_FSEL_1] = fsel;
    gpio[GPIO_FSEL_1] = fsel;
}

void RpiBus::InitializeSignals()
{
    for (const int s : SIGNAL_TABLE) {
        PinSetSignal(s, false);
        PinConfig(s, GPIO_INPUT);
        ConfigurePullDown(s);
    }
}

void RpiBus::CreateWorkTable()
{
    array<bool, 256> tblParity;

    const auto tblSize = static_cast<uint32_t>(tblParity.size());

    // Create parity table
    for (uint32_t i = 0; i < tblSize; ++i) {
        uint32_t bits = i;
        uint32_t parity = 0;
        for (int j = 0; j < 8; ++j) {
            parity ^= bits & 1;
            bits >>= 1;
        }
        parity = ~parity;
        tblParity[i] = parity & 1;
    }

    // Mask data defaults
    for (auto &tbl : tblDatMsk) {
        tbl.fill(-1);
    }

    for (uint32_t i = 0; i < tblSize; ++i) {
        // Bit string for inspection
        uint32_t bits = i;

        // Get parity
        if (tblParity[i]) {
            bits |= (1 << 8);
        }

        // Bit check
        for (const int pin : DATA_PINS) {
            // Offset of the Function Select register for this pin (3 bits per pin)
            const int index = pin / 10;
            const int shift = (pin % 10) * 3;

            // Mask data (GPIO pin is an output pin)
            tblDatMsk[index][i] &= ~(7 << shift);

            // Value (GPIO pin is set to 1)
            if (bits & 1) {
                tblDatSet[index][i] |= (1 << shift);
            }

            bits >>= 1;
        }
    }
}

void RpiBus::SetControl(int pin, bool state)
{
    assert(pin >= PIN_ATN && pin <= PIN_SEL);

    PinSetSignal(pin, state);
}

//---------------------------------------------------------------------------
//
// Set output signal value
//
// Sets the output value. Used with:
//   PIN_ENB, ACT, TAD, IND, DTD, BSY, SignalTable
//
//---------------------------------------------------------------------------
void RpiBus::SetSignal(int pin, bool state)
{
    const int index = pin / 10;
    assert(index <= 2);
    const int shift = (pin % 10) * 3;
    uint32_t data = gpfsel[index];
    if (state) {
        data |= (1 << shift);
    } else {
        data &= ~(7 << shift);
    }
    gpio[index] = data;
    gpfsel[index] = data;
}

void RpiBus::DisableIRQ()
{
    switch (pi_type) {
    case PiType::PI_1:
        // Stop system timer interrupt with interrupt controller
        irpt_enb = irp_ctl[IRPT_ENB_IRQ_1];
        irp_ctl[IRPT_DIS_IRQ_1] = irpt_enb & 0xf;
        break;

    case PiType::PI_2:
    case PiType::PI_3:
        // RPI2,3 disable core timer IRQ
        tint_core = sched_getcpu() + QA7_CORE0_TINTC;
        tint_ctl = qa7_regs[tint_core];
        qa7_regs[tint_core] = 0;
        break;

    case PiType::PI_4:
        // RPI4 disables interrupts via the GIC
        gicc_pmr_saved = *gicc_mpr;
        *gicc_mpr = 0;
        break;

    default:
        assert(false);
        break;
    }

    irq_disabled = true;
}

void RpiBus::EnableIRQ()
{
    switch (pi_type) {
    case PiType::PI_1:
        // Restart the system timer interrupt with the interrupt controller
        irp_ctl[IRPT_ENB_IRQ_1] = irpt_enb & 0xf;
        break;

    case PiType::PI_2:
    case PiType::PI_3:
        // RPI2,3 re-enable core timer IRQ
        qa7_regs[tint_core] = tint_ctl;
        break;

    case PiType::PI_4:
        // RPI4 enables interrupts via the GIC
        *gicc_mpr = gicc_pmr_saved;
        break;

    default:
        assert(false);
        break;
    }

    irq_disabled = false;
}

// Pin direction setting (input/output)
void RpiBus::PinConfig(int pin, int mode)
{
#ifdef BOARD_STANDARD
    if (pin < 0) {
        return;
    }
#endif

    const int index = pin / 10;
    const uint32_t mask = ~(7 << ((pin % 10) * 3));
    gpio[index] = (gpio[index] & mask) | ((mode & 0x7) << ((pin % 10) * 3));
}

void RpiBus::ConfigurePullDown(int pin)
{
#ifdef BOARD_STANDARD
    if (pin < 0) {
        return;
    }
#endif

    pin &= 0x1f;
    if (pi_type == PiType::PI_4) {
        const int shift = pin << 1;
        uint32_t bits = gpio[GPIO_PUPPDN0 + (pin >> 4)];
        bits &= ~(3 << shift);
        gpio[GPIO_PUPPDN0 + (pin >> 4)] = bits;
    } else {
        // 2 us
        constexpr timespec ts = { .tv_sec = 0, .tv_nsec = 2'000 };

        gpio[GPIO_PUD] = 0;
        nanosleep(&ts, nullptr);
        gpio[GPIO_CLK_0] = 1 << pin;
        nanosleep(&ts, nullptr);
        gpio[GPIO_PUD] = 0;
        gpio[GPIO_CLK_0] = 0;
    }
}

// Set output pin
void RpiBus::PinSetSignal(int pin, bool state)
{
#ifdef BOARD_STANDARD
    if (pin < 0) {
        return;
    }
#endif

    gpio[state ? GPIO_SET_0 : GPIO_CLR_0] = 1 << pin;
}

void RpiBus::SetSignalDriveStrength(uint32_t drive)
{
    const uint32_t data = pads[PAD_0_27];
    pads[PAD_0_27] = (0xfffffff8 & data) | drive | 0x5a000000;
}

// Read data from bus
inline void RpiBus::Acquire()
{
    SetSignals(*level);
}

// nanosleep() does not provide the required resolution, which causes issues when reading data from the bus.
// Furthermore, nanosleep() requires interrupts to be enabled.
void RpiBus::WaitNanoSeconds(bool daynaport) const
{
    // Either Daynaport delay or bus settle delay
    const uint32_t count = armt_addr[ARMT_FREERUN] + (daynaport ? daynaport_count : bus_settle_count);
    while (armt_addr[ARMT_FREERUN] < count) {
        // Intentionally empty
    }
}

RpiBus::PiType RpiBus::CheckForPi()
{
    ifstream in("/proc/device-tree/model");
    if (!in) {
        warn("This platform is not a Raspberry Pi, functionality is limited");
        return RpiBus::PiType::UNKNOWN;
    }

    stringstream s;
    s << in.rdbuf();

    return GetPiType(s.str());
}

RpiBus::PiType RpiBus::GetPiType(const string &model)
{
    if (!model.starts_with("Raspberry Pi ") || model.size() < 13) {
        warn("This platform is not a Raspberry Pi, functionality is limited");
        return RpiBus::PiType::UNKNOWN;
    }

    int type;
    if(model.find("Zero 2") != string::npos) {
        type = static_cast<int>(RpiBus::PiType::PI_3);
    }
    else {
        type = model.find("Zero") != string::npos ||
        model.find("Raspberry Pi Model B Plus") != string::npos ? 1 : model.substr(13, 1)[0] - '0';
    }
    if (type <= 0 || type > 4) {
        warn("Unsupported Raspberry Pi model '{}', functionality is limited", model);
        return RpiBus::PiType::UNKNOWN;
    }

    return static_cast<RpiBus::PiType>(type);
}
