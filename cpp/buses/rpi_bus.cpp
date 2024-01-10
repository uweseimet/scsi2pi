//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <map>
#include <cstring>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <spdlog/spdlog.h>
#include "rpi_bus.h"

//---------------------------------------------------------------------------
//
//	imported from bcm_host.c
//
//---------------------------------------------------------------------------
uint32_t RpiBus::get_dt_ranges(const char *filename, uint32_t offset)
{
    uint32_t address = ~0;
    if (FILE *fp = fopen(filename, "rb"); fp) {
        fseek(fp, offset, SEEK_SET);
        if (array<uint8_t, 4> buf; fread(buf.data(), 1, buf.size(), fp) == buf.size()) {
            address = (int)buf[0] << 24 | (int)buf[1] << 16 | (int)buf[2] << 8 | (int)buf[3] << 0;
        }
        fclose(fp);
    }
    return address;
}

uint32_t RpiBus::bcm_host_get_peripheral_address()
{
#ifdef __linux__
    uint32_t address = get_dt_ranges("/proc/device-tree/soc/ranges", 4);
    if (!address) {
        address = get_dt_ranges("/proc/device-tree/soc/ranges", 8);
    }
    address = (address == (uint32_t)~0) ? 0x20000000 : address;
    return address;
#else
    return 0;
#endif
}

bool RpiBus::Init(bool target)
{
    GpioBus::Init(target);

    // Get the base address
    baseaddr = bcm_host_get_peripheral_address();

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        spdlog::error("Error: Unable to open /dev/mem");
        return false;
    }

    // Map peripheral region memory
    void *map = mmap(nullptr, 0x1000100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, baseaddr);
    if (map == MAP_FAILED) {
        spdlog::error("Error: Unable to map memory: " + string(strerror(errno)));
        close(fd);
        return false;
    }

    // Determine the type of raspberry pi from the base address
    if (baseaddr == 0xfe000000) {
        rpitype = 4;
    } else if (baseaddr == 0x3f000000) {
        rpitype = 2;
    } else {
        rpitype = 1;
    }

    // GPIO
    gpio = static_cast<uint32_t*>(map);
    gpio += GPIO_OFFSET / sizeof(uint32_t);
    level = &gpio[GPIO_LEV_0];

    // PADS
    pads = static_cast<uint32_t*>(map);
    pads += PADS_OFFSET / sizeof(uint32_t);

    // Interrupt controller
    irpctl = static_cast<uint32_t*>(map);
    irpctl += IRPT_OFFSET / sizeof(uint32_t);

    // Quad-A7 control
    qa7regs = static_cast<uint32_t*>(map);
    qa7regs += QA7_OFFSET / sizeof(uint32_t);

    // Map GIC memory
    if (rpitype == 4) {
        map = mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_SHARED, fd, ARM_GICD_BASE);
        if (map == MAP_FAILED) {
            close(fd);
            return false;
        }
        gicd = static_cast<uint32_t*>(map);
        gicc = static_cast<uint32_t*>(map);
        gicc += (ARM_GICC_BASE - ARM_GICD_BASE) / sizeof(uint32_t);
    } else {
        gicd = nullptr;
        gicc = nullptr;
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
#ifdef USE_SEL_EVENT_ENABLE
    fd = open("/dev/gpiochip0", 0);
    if (fd == -1) {
        spdlog::error("Unable to open /dev/gpiochip0. If s2p is running, please shut it down first.");
        return false;
    }

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
        spdlog::error("Unable to register event request. If s2p is running, please shut it down first.");
        close(fd);
        return false;
    }

    // Close GPIO chip file handle
    close(fd);

    // epoll initialization
    epfd = epoll_create(1);
    epoll_event ev = { };
    ev.events = EPOLLIN | EPOLLPRI;
    ev.data.fd = selevreq.fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, selevreq.fd, &ev);
#else
    // Edge detection setting
#if SIGNAL_CONTROL_MODE == 2
    gpio[GPIO_AREN_0] = 1 << PIN_SEL;
#else
    gpio[GPIO_AFEN_0] = 1 << PIN_SEL;
#endif

    // Clear event - GPIO Pin Event Detect Status
    gpio[GPIO_EDS_0] = 1 << PIN_SEL;

#ifdef __linux__
    // Register interrupt handler
    setIrqFuncAddress(IrqHandler);
#endif

    // GPIO interrupt setting
    if (rpitype == 4) {
        // GIC Invalid
        gicd[GICD_CTLR] = 0;

        // Route all interupts to core 0
        for (int i = 0; i < 8; i++) {
            gicd[GICD_ICENABLER0 + i] = 0xffffffff;
            gicd[GICD_ICPENDR0 + i] = 0xffffffff;
            gicd[GICD_ICACTIVER0 + i] = 0xffffffff;
        }
        for (int i = 0; i < 64; i++) {
            gicd[GICD_IPRIORITYR0 + i] = 0xa0a0a0a0;
            gicd[GICD_ITARGETSR0 + i] = 0x01010101;
        }

        // Set all interrupts as level triggers
        for (int i = 0; i < 16; i++) {
            gicd[GICD_ICFGR0 + i] = 0;
        }

        // GIC Invalid
        gicd[GICD_CTLR] = 1;

        // Enable CPU interface for core 0
        gicc[GICC_PMR]  = 0xf0;
        gicc[GICC_CTLR] = 1;

        // Enable interrupts
        gicd[GICD_ISENABLER0 + (GIC_GPIO_IRQ / 32)] = 1 << (GIC_GPIO_IRQ % 32);
    } else {
        // Enable interrupts
        irpctl[IRPT_ENB_IRQ_2] = (1 << (GPIO_IRQ % 32));
    }
#endif

    // Create work table
    CreateWorkTable();

    // Finally, enable ENABLE
    // Show the user that this app is running
    SetControl(PIN_ENB, ENB_ON);

    return true;
}

void RpiBus::CleanUp()
{
    // Release SEL signal interrupt
#ifdef USE_SEL_EVENT_ENABLE
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
#ifndef USE_SEL_EVENT_ENABLE
    Acquire();
    if (!GetSEL()) {
        const timespec ts = { .tv_sec = 0, .tv_nsec = 0};
        nanosleep(&ts, nullptr);
        return false;
    }
#else
    errno = 0;

    if (epoll_event epev; epoll_wait(epfd, &epev, 1, -1) <= 0) {
        if (errno != EINTR) {
            spdlog::warn("epoll_wait failed");
        }
        return false;
    }

    if (gpioevent_data gpev; read(selevreq.fd, &gpev, sizeof(gpev)) < 0) {
        if (errno != EINTR) {
            spdlog::warn("read failed");
        }
        return false;
    }

    Acquire();
#endif

    return true;
}

bool RpiBus::GetBSY() const
{
    return GetSignal(PIN_BSY);
}

void RpiBus::SetBSY(bool ast)
{
    SetSignal(PIN_BSY, ast);

    if (ast) {
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

void RpiBus::SetSEL(bool ast)
{
    if (!IsTarget() && ast) {
        SetControl(PIN_ACT, true);
    }

    SetSignal(PIN_SEL, ast);
}

bool RpiBus::GetATN() const
{
    return GetSignal(PIN_ATN);
}

void RpiBus::SetATN(bool ast)
{
    SetSignal(PIN_ATN, ast);
}

bool RpiBus::GetACK() const
{
    return GetSignal(PIN_ACK);
}

void RpiBus::SetACK(bool ast)
{
    SetSignal(PIN_ACK, ast);
}

bool RpiBus::GetRST() const
{
    return GetSignal(PIN_RST);
}

void RpiBus::SetRST(bool ast)
{
    SetSignal(PIN_RST, ast);
}

bool RpiBus::GetMSG() const
{
    return GetSignal(PIN_MSG);
}

void RpiBus::SetMSG(bool ast)
{
    SetSignal(PIN_MSG, ast);
}

bool RpiBus::GetCD() const
{
    return GetSignal(PIN_CD);
}

void RpiBus::SetCD(bool ast)
{
    SetSignal(PIN_CD, ast);
}

bool RpiBus::GetIO()
{
    bool ast = GetSignal(PIN_IO);

    if (!IsTarget()) {
        // Change the data input/output direction by IO signal
        if (ast) {
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

    return ast;
}

void RpiBus::SetIO(bool ast)
{
    assert(IsTarget());

    SetSignal(PIN_IO, ast);

    // Change the data input/output direction by IO signal
    if (ast) {
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

void RpiBus::SetREQ(bool ast)
{
    SetSignal(PIN_REQ, ast);
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

void RpiBus::SetControl(int pin, bool ast)
{
    PinSetSignal(pin, ast);
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

//---------------------------------------------------------------------------
//
//	Get input signal value
//
//---------------------------------------------------------------------------
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
void RpiBus::SetSignal(int pin, bool ast)
{
#if SIGNAL_CONTROL_MODE == 0
    const int index = pin / 10;
    const int shift = (pin % 10) * 3;
    uint32_t data = gpfsel[index];
    if (ast) {
        data |= (1 << shift);
    } else {
        data &= ~(0x7 << shift);
    }
    gpio[index] = data;
    gpfsel[index] = data;
#elif SIGNAL_CONTROL_MODE == 1
    if (ast) {
        gpio[GPIO_CLR_0] = 0x1 << pin;
    } else {
        gpio[GPIO_SET_0] = 0x1 << pin;
    }
#elif SIGNAL_CONTROL_MODE == 2
    if (ast) {
        gpio[GPIO_SET_0] = 0x1 << pin;
    } else {
        gpio[GPIO_CLR_0] = 0x1 << pin;
    }
#endif
}

void RpiBus::DisableIRQ()
{
#ifndef NO_IRQ_DISABLE
    if (rpitype == 4) {
        // RPI4 is disabled by GICC
        giccpmr = gicc[GICC_PMR];
        gicc[GICC_PMR] = 0;
    } else if (rpitype == 2) {
        // RPI2,3 disable core timer IRQ
        tintcore = sched_getcpu() + QA7_CORE0_TINTC;
        tintctl = qa7regs[tintcore];
        qa7regs[tintcore] = 0;
    } else {
        // Stop system timer interrupt with interrupt controller
        irptenb = irpctl[IRPT_ENB_IRQ_1];
        irpctl[IRPT_DIS_IRQ_1] = irptenb & 0xf;
    }
#endif
}

void RpiBus::EnableIRQ()
{
#ifndef NO_IRQ_DISABLE
    if (rpitype == 4) {
        // RPI4 enables interrupts via the GICC
        gicc[GICC_PMR] = giccpmr;
    } else if (rpitype == 2) {
        // RPI2,3 re-enable core timer IRQ
        qa7regs[tintcore] = tintctl;
    } else {
        // Restart the system timer interrupt with the interrupt controller
        irpctl[IRPT_ENB_IRQ_1] = irptenb & 0xf;
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

//---------------------------------------------------------------------------
//
//	Pin pull-up/pull-down setting
//
//---------------------------------------------------------------------------
void RpiBus::PullConfig(int pin, int mode)
{
    // Check for invalid pin
    if (pin < 0) {
        return;
    }

    if (rpitype == 4) {
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

//---------------------------------------------------------------------------
//
//	Set output pin
//
//---------------------------------------------------------------------------
void RpiBus::PinSetSignal(int pin, bool ast)
{
    // Check for invalid pin
    if (pin >= 0) {
        gpio[ast ? GPIO_SET_0 : GPIO_CLR_0] = 1 << pin;
    }
}

void RpiBus::SetSignalDriveStrength(uint32_t drive)
{
    const uint32_t data = pads[PAD_0_27];
    pads[PAD_0_27] = (0xFFFFFFF8 & data) | drive | 0x5a000000;
}

//---------------------------------------------------------------------------
//
//	Bus signal acquisition
//
//---------------------------------------------------------------------------
uint32_t RpiBus::Acquire()
{
    signals = *level;

#if SIGNAL_CONTROL_MODE < 2
    // Invert if negative logic (internal processing is unified to positive logic)
    signals = ~signals;
#endif

    return signals;
}
