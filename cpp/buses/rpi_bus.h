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

#ifdef __linux__
#include <linux/gpio.h>
#include <sys/epoll.h>
#endif
#include "gpio_bus.h"

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
    void WaitBusSettle() const override;

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

    int pi_type = 0;

    inline static uint32_t corefreq = 0;

    volatile uint32_t *armtaddr = nullptr;

    // GPIO register
    volatile uint32_t *gpio = nullptr;

    // PADS register
    volatile uint32_t *pads = nullptr;

    // Interrupt control register
    volatile uint32_t *irpctl = nullptr;

    // QA7 register
    volatile uint32_t *qa7regs = nullptr;

    // Interrupt enabled state
    uint32_t irptenb;

    // Interupt control target CPU
    int tintcore;

    // Interupt control
    uint32_t tintctl;

    // GICC priority setting
    uint32_t giccpmr;

    // GIC CPU interface register
    volatile uint32_t *gicc = nullptr;

    // RAM copy of GPFSEL0-4  values (GPIO Function Select)
    array<uint32_t, 4> gpfsel;

    // All bus signals
    uint32_t signals = 0;

    // GPIO input level
    volatile uint32_t *level = nullptr;

#ifdef __linux__
    // SEL signal event request
    struct gpioevent_request selevreq = { };

    // epoll file descriptor
    int epoll_fd = 0;
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

    const static int ARMT_CTRL = 2;
    const static int ARMT_FREERUN = 8;

    const static uint32_t ARMT_OFFSET = 0x0000B400;

    const static int GPIO_FSEL_0 = 0;
    const static int GPIO_FSEL_1 = 1;
    const static int GPIO_FSEL_2 = 2;
    const static int GPIO_FSEL_3 = 3;
    const static int GPIO_SET_0 = 7;
    const static int GPIO_CLR_0 = 10;
    const static int GPIO_LEV_0 = 13;
    const static int GPIO_PUD = 37;
    const static int GPIO_CLK_0 = 38;
    const static int GPIO_PUPPDN0 = 57;
    const static int PAD_0_27 = 11;
    const static int IRPT_ENB_IRQ_1 = 4;
    const static int IRPT_DIS_IRQ_1 = 7;
    const static int QA7_CORE0_TINTC = 16;
    // GPIO3
    const static int GPIO_IRQ = (32 + 20);

    const static uint32_t IRPT_OFFSET = 0x0000B200;
    const static uint32_t PADS_OFFSET = 0x00100000;
    const static uint32_t GPIO_OFFSET = 0x00200000;
    const static uint32_t QA7_OFFSET = 0x01000000;

    // Constant declarations (GIC)
    const static uint32_t ARM_GICD_BASE = 0xFF841000;
    const static uint32_t ARM_GICC_BASE = 0xFF842000;
    const static int GICC_PMR = 0x001;
    const static int GIC_GPIO_IRQ = (32 + 116);
};
