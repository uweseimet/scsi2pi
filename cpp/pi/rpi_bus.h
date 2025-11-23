//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <linux/gpio.h>
#include <sys/epoll.h>
#include "buses/bus.h"

class RpiBus final : public Bus
{

public:

    enum class PiType
    {
        UNKNOWN = 0,
        PI_1 = 1,
        PI_2 = 2,
        PI_3 = 3,
        PI_4 = 4,
        PI_5 = 5
    };

    explicit RpiBus(PiType type) : pi_type(type)
    {
    }
    ~RpiBus() override = default;

    bool Init(bool) override;

    void Reset() override;
    void CleanUp() override;

    bool WaitForSelection() override;

    // Bus signal acquisition
    uint32_t Acquire() override;

    void SetBSY(bool) override;

    void SetSEL(bool) override;

    uint8_t GetDAT() override;
    void SetDAT(uint8_t) override;

    void WaitBusSettle() const override;

    bool IsRaspberryPi() const override
    {
        return true;
    }

    static PiType CheckForPi();

    static PiType GetPiType(const string&);

private:

    void InitializeSignals(int);

    void CreateWorkTable();

    void SetControl(int, bool);

    // Sets signal direction to IN
    void SetModeIn(int);

    bool GetSignal(int) const override;
    void SetSignal(int, bool) override;

    void DisableIRQ() override;
    void EnableIRQ() override;

    void SetDir(bool) override;

    //GPIO pin pull up/down resistor setting
    void PullConfig(int, int);

    //GPIO pin direction setting
    void PinConfig(int, int);

    void PinSetSignal(int, bool);

    // Set GPIO drive strength
    void SetSignalDriveStrength(uint32_t);

    PiType pi_type;

    uint32_t timer_core_freq = 0;

    volatile uint32_t *armt_addr = nullptr;

    // GPIO register
    volatile uint32_t *gpio = nullptr;

    // PADS register
    volatile uint32_t *pads = nullptr;

    // Interrupt control register
    volatile uint32_t *irp_ctl = nullptr;

    // QA7 register
    volatile uint32_t *qa7_regs = nullptr;

    // Interrupt enabled state
    uint32_t irpt_enb = 0;

    // Interupt control target CPU
    int tint_core = 0;

    // Interupt control
    uint32_t tint_ctl = 0;

    // GIC priority setting
    uint32_t gicc_pmr_saved = 0;

    // SEL signal event request
    struct gpioevent_request selevreq = { };

    int epoll_fd = 0;

    // GIC CPU interface register
    volatile uint32_t *gicc_mpr = nullptr;

    // RAM copy of GPFSEL0-2  values (GPIO Function Select)
    // Reading the current data from the copy is faster than directly reading them from the ports
    array<uint32_t, 3> gpfsel;

    // All bus signals
    uint32_t signals = 0;

    // GPIO input level
    volatile uint32_t *level = nullptr;

    // Data mask table
    array<array<uint32_t, 256>, 3> tblDatMsk;
    // Data setting table
    array<array<uint32_t, 256>, 3> tblDatSet = { };

    constexpr static array<int, 19> SIGNAL_TABLE = { PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4, PIN_DT5, PIN_DT6,
        PIN_DT7, PIN_DP, PIN_SEL, PIN_ATN, PIN_RST, PIN_ACK, PIN_BSY, PIN_MSG, PIN_CD, PIN_IO, PIN_REQ };

    constexpr static array<int, 9> DATA_PINS = { PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4, PIN_DT5, PIN_DT6, PIN_DT7,
        PIN_DP };

    constexpr static int ARMT_CTRL = 2;
    constexpr static int ARMT_FREERUN = 8;

    constexpr static uint32_t ARMT_OFFSET = 0x0000B400;

    constexpr static int GPIO_INPUT = 0;
    constexpr static int GPIO_OUTPUT = 1;
    constexpr static int GPIO_PULLNONE = 0;
    constexpr static int GPIO_PULLDOWN = 1;

    constexpr static int GPIO_FSEL_0 = 0;
    constexpr static int GPIO_FSEL_1 = 1;
    constexpr static int GPIO_FSEL_2 = 2;
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
    constexpr static uint32_t PADS_OFFSET_RP1 = 0x000f0000;
    constexpr static uint32_t GPIO_OFFSET = 0x00200000;
    constexpr static uint32_t GPIO_OFFSET_RP1 = 0x000d0000;
    constexpr static uint32_t RIO_OFFSET_RP1 = 0x000e0000;
    constexpr static uint32_t QA7_OFFSET = 0x01000000;

    constexpr static uint32_t PI4_ARM_GICC_CTLR = 0xFF842000;
};
