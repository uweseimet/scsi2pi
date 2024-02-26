//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
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

    enum class PiType
    {
        unknown = 0,
        pi_1 = 1,
        pi_2 = 2,
        pi_3 = 3,
        pi_4 = 4
    };

    explicit RpiBus(PiType t) : pi_type(t)
    {
    }
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

    PiType pi_type;

    inline static uint32_t core_freq = 0;

    volatile uint32_t *armt_addr = nullptr;

    // GPIO register
    volatile uint32_t *gpio = nullptr;

    // PADS register
    volatile uint32_t *pads = nullptr;

    // Interrupt control register
    volatile uint32_t *irp_ctl = nullptr;

    // QA7 register
    volatile uint32_t *qa7_regs = nullptr;

#ifdef __linux__
    // Interrupt enabled state
    uint32_t irpt_enb;

    // Interupt control target CPU
    int tint_core;

    // Interupt control
    uint32_t tint_ctl;

    // GIC priority setting
    uint32_t gicc_pmr_saved;
    // SEL signal event request
    struct gpioevent_request selevreq = { };

    int epoll_fd = 0;
#endif

    // GIC CPU interface register
    volatile uint32_t *gicc_mpr = nullptr;

    // RAM copy of GPFSEL0-4  values (GPIO Function Select)
    array<uint32_t, 4> gpfsel;

    // All bus signals
    uint32_t signals = 0;

    // GPIO input level
    volatile uint32_t *level = nullptr;

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

    constexpr static int ARMT_CTRL = 2;
    constexpr static int ARMT_FREERUN = 8;

    constexpr static uint32_t ARMT_OFFSET = 0x0000B400;

    constexpr static int GPIO_FSEL_0 = 0;
    constexpr static int GPIO_FSEL_1 = 1;
    constexpr static int GPIO_FSEL_2 = 2;
    constexpr static int GPIO_FSEL_3 = 3;
    constexpr static int GPIO_SET_0 = 7;
    constexpr static int GPIO_CLR_0 = 10;
    constexpr static int GPIO_LEV_0 = 13;
    constexpr static int GPIO_PUD = 37;
    constexpr static int GPIO_CLK_0 = 38;
    constexpr static int GPIO_PUPPDN0 = 57;
    constexpr static int PAD_0_27 = 11;
    constexpr static int IRPT_ENB_IRQ_1 = 4;
    constexpr static int IRPT_DIS_IRQ_1 = 7;
    constexpr static int QA7_CORE0_TINTC = 16;

    constexpr static uint32_t IRPT_OFFSET = 0x0000B200;
    constexpr static uint32_t PADS_OFFSET = 0x00100000;
    constexpr static uint32_t GPIO_OFFSET = 0x00200000;
    constexpr static uint32_t QA7_OFFSET = 0x01000000;

    constexpr static uint32_t PI4_ARM_GICC_BASE = 0xFF842000;
};
