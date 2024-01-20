//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

// Configurable settings
#ifdef __linux__
// Check SEL signal by event instead of polling
#define USE_SEL_EVENT_ENABLE
#endif

// Not having to disable IRQs would ease porting to Pis which other interrupt hardware like maybe the Pi 5
//#define NO_IRQ_DISABLE

#ifndef __linux__
#define NO_IRQ_DISABLE
#endif

#ifdef __linux__
#include <linux/gpio.h>
#endif
#ifdef USE_SEL_EVENT_ENABLE
#include <sys/epoll.h>
#endif
#include "gpio_bus.h"

// Constant declarations (GIC)
const static uint32_t ARM_GICD_BASE = 0xFF841000;
const static uint32_t ARM_GICC_BASE = 0xFF842000;
const static uint32_t ARM_GIC_END = 0xFF847FFF;
const static int GICD_CTLR = 0x000;
const static int GICD_IGROUPR0 = 0x020;
const static int GICD_ISENABLER0 = 0x040;
const static int GICD_ICENABLER0 = 0x060;
const static int GICD_ISPENDR0 = 0x080;
const static int GICD_ICPENDR0 = 0x0A0;
const static int GICD_ISACTIVER0 = 0x0C0;
const static int GICD_ICACTIVER0 = 0x0E0;
const static int GICD_IPRIORITYR0 = 0x100;
const static int GICD_ITARGETSR0 = 0x200;
const static int GICD_ICFGR0 = 0x300;
const static int GICD_SGIR = 0x3C0;
const static int GICC_CTLR = 0x000;
const static int GICC_PMR = 0x001;

// GPIO3
const static int GIC_GPIO_IRQ = (32 + 116);

class RpiBus : public GpioBus
{

public:

    RpiBus() = default;
    ~RpiBus() override = default;

    bool Init(bool = true) override;

    void Reset() override;
    void CleanUp() override;

    bool WaitForSelection() override;

    // Bus signal acquisition
    uint32_t Acquire() override;

    bool GetBSY() const override;
    void SetBSY(bool) override;

    bool GetSEL() const override;
    void SetSEL(bool) override;

    bool GetATN() const override;
    void SetATN(bool) override;

    bool GetACK() const override;
    void SetACK(bool) override;

    bool GetRST() const override;
    void SetRST(bool) override;

    bool GetMSG() const override;
    void SetMSG(bool) override;

    bool GetCD() const override;
    void SetCD(bool) override;

    bool GetIO() override;
    void SetIO(bool ast) override;

    bool GetREQ() const override;
    void SetREQ(bool ast) override;

    uint8_t GetDAT() override;
    void SetDAT(uint8_t) override;

    bool WaitREQ(bool ast) override
    {
        return WaitSignal(PIN_REQ, ast);
    }
    inline bool WaitACK(bool ast) override
    {
        return WaitSignal(PIN_ACK, ast);
    }
    static uint32_t bcm_host_get_peripheral_address();

private:

    void CreateWorkTable();

    void SetControl(int, bool) override;
    void SetMode(int, int) override;

    bool GetSignal(int) const override;
    void SetSignal(int, bool) override;

    void DisableIRQ() override;
    void EnableIRQ() override;

    void PinConfig(int, int) override;
    void PullConfig(int, int) override;
    void PinSetSignal(int, bool) override;

    // Set GPIO drive strength
    void SetSignalDriveStrength(uint32_t);

    static uint32_t get_dt_ranges(const char*, uint32_t);

    uint32_t baseaddr = 0;

    int rpitype = 0;

    // GPIO register
    volatile uint32_t *gpio = nullptr;

    // PADS register
    volatile uint32_t *pads = nullptr;

    // Interrupt control register
    volatile uint32_t *irpctl = nullptr;

    // QA7 register
    volatile uint32_t *qa7regs = nullptr;

#ifndef NO_IRQ_DISABLE
    // Interrupt enabled state
    volatile uint32_t irptenb; // NOSONAR volatile is correct here

    // Interupt control target CPU
    volatile int tintcore; // NOSONAR volatile is correct here

    // Interupt control
    volatile uint32_t tintctl; // NOSONAR volatile is correct here

    // GICC priority setting
    volatile uint32_t giccpmr; // NOSONAR volatile is correct here
#endif

    // GIC Interrupt distributor register
    volatile uint32_t *gicd = nullptr;

    // GIC CPU interface register
    volatile uint32_t *gicc = nullptr;

    // RAM copy of GPFSEL0-4  values (GPIO Function Select)
    array<uint32_t, 4> gpfsel;

    // All bus signals
    uint32_t signals = 0;

    // GPIO input level
    volatile uint32_t *level = nullptr;

#ifdef USE_SEL_EVENT_ENABLE
    // SEL signal event request
    struct gpioevent_request selevreq = { };
    // epoll file descriptor
    int epfd = 0;
    #endif

#if SIGNAL_CONTROL_MODE == 0
    // Data mask table
    array<array<uint32_t, 256>, 3> tblDatMsk;
    // Data setting table
    array<array<uint32_t, 256>, 3> tblDatSet;
    #else
    // Data mask table
    array<uint32_t, 256> tblDatMsk = {};
    // Table setting table
    array<uint32_t, 256> tblDatSet = {};
#endif

    static const array<int, 19> SignalTable;

    const static int GPIO_FSEL_0 = 0;
    const static int GPIO_FSEL_1 = 1;
    const static int GPIO_FSEL_2 = 2;
    const static int GPIO_FSEL_3 = 3;
    const static int GPIO_SET_0 = 7;
    const static int GPIO_CLR_0 = 10;
    const static int GPIO_LEV_0 = 13;
    const static int GPIO_EDS_0 = 16;
    const static int GPIO_REN_0 = 19;
    const static int GPIO_FEN_0 = 22;
    const static int GPIO_HEN_0 = 25;
    const static int GPIO_LEN_0 = 28;
    const static int GPIO_AREN_0 = 31;
    const static int GPIO_AFEN_0 = 34;
    const static int GPIO_PUD = 37;
    const static int GPIO_CLK_0 = 38;
    const static int GPIO_GPPINMUXSD = 52;
    const static int GPIO_PUPPDN0 = 57;
    const static int GPIO_PUPPDN1 = 58;
    const static int GPIO_PUPPDN3 = 59;
    const static int GPIO_PUPPDN4 = 60;
    const static int PAD_0_27 = 11;
    const static int IRPT_PND_IRQ_B = 0;
    const static int IRPT_PND_IRQ_1 = 1;
    const static int IRPT_PND_IRQ_2 = 2;
    const static int IRPT_FIQ_CNTL = 3;
    const static int IRPT_ENB_IRQ_1 = 4;
    const static int IRPT_ENB_IRQ_2 = 5;
    const static int IRPT_ENB_IRQ_B = 6;
    const static int IRPT_DIS_IRQ_1 = 7;
    const static int IRPT_DIS_IRQ_2 = 8;
    const static int IRPT_DIS_IRQ_B = 9;
    const static int QA7_CORE0_TINTC = 16;
    const static int GPIO_IRQ = (32 + 20); // GPIO3

    const static uint32_t IRPT_OFFSET = 0x0000B200;
    const static uint32_t PADS_OFFSET = 0x00100000;
    const static uint32_t GPIO_OFFSET = 0x00200000;
    const static uint32_t QA7_OFFSET = 0x01000000;
};
