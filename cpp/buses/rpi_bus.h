//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <linux/gpio.h>
#include <sys/epoll.h>
#include "bus.h"

class RpiBus final : public Bus
{

public:

    enum class PiType
    {
        UNKNOWN = 0,
        PI_1 = 1,
        PI_2 = 2,
        PI_3 = 3,
        PI_4 = 4
    };

    RpiBus(PiType type, bool, bool = true);

    bool IsRaspberryPi() const override
    {
        return true;
    }

    static PiType GetPiType(const string& = "/proc/device-tree/model");

private:

    string SetUp(bool) override;
    void CleanUp() override;

    void Reset() const override;

    void InitializeSignals() const;

    void CreateWorkTable();

    void SetSignal(int, bool) const override;

    void DisableIRQ() override;
    void EnableIRQ() override;

    void SetDataDirIn(bool) const override;

    // GPIO pin direction setting
    void PinConfig(int, int) const;

    void PinSetSignal(int, bool) const;

    // Set GPIO pin pull up/down resistor setting to PULLDOWN
    void ConfigurePullDown(int) const;

    // Set GPIO drive strength
    void SetSignalDriveStrength(uint32_t) const;

    // Bus signal acquisition
    void Acquire() const override
    {
        SetSignals(*level);
    }

    void SetBSY(bool) const override;

    void SetSEL(bool) const override;

    void SetDAT(uint8_t) const override;

    void WaitNanoSeconds(bool) const override;

    uint8_t WaitForSelection() override;

    const PiType pi_type;

    const bool enable_irqs;

    // Set to -1 for the STANDARD board
    int pin_ind = PIN_IND;
    int pin_tad = PIN_TAD;
    int pin_dtd = PIN_DTD;

    uint32_t bus_settle_count = 0;
    uint32_t daynaport_count = 0;

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

    // Interrupt control target CPU
    int tint_core = 0;

    // Interrupt control
    uint32_t tint_ctl = 0;

    // GIC priority setting
    uint32_t gicc_pmr_saved = 0;

    // SEL signal event request
    struct gpioevent_request selevreq = { };

    int epoll_fd = -1;

    // GIC CPU interface register
    volatile uint32_t *gicc_mpr = nullptr;

    // RAM copy of GPFSEL0-2  values (GPIO Function Select), mutable because these values are external state.
    // Reading the current data from the copy is faster than directly reading them from the ports.
    mutable array<uint32_t, 3> gpfsel = { };

    // GPIO input level
    volatile uint32_t *level = nullptr;

    // Data setting table for data pins
    array<uint32_t, 256> tblDatSet = { };

    static constexpr array<int, 19> SIGNAL_TABLE = { PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4, PIN_DT5, PIN_DT6,
        PIN_DT7, PIN_DP, PIN_SEL, PIN_ATN, PIN_RST, PIN_ACK, PIN_BSY, PIN_MSG, PIN_CD, PIN_IO, PIN_REQ };

    static constexpr array<int, 9> DATA_PINS = { PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4, PIN_DT5, PIN_DT6, PIN_DT7,
        PIN_DP };

    static constexpr int ARMT_CTRL = 2;
    static constexpr int ARMT_FREERUN = 8;

    static constexpr uint32_t ARMT_OFFSET = 0x0000B400;

    static constexpr int GPIO_INPUT = 0;
    static constexpr int GPIO_OUTPUT = 1;

    static constexpr int GPIO_FSEL_0 = 0;
    static constexpr int GPIO_FSEL_1 = 1;
    static constexpr int GPIO_FSEL_2 = 2;
    static constexpr int GPIO_SET_0 = 7;
    static constexpr int GPIO_CLR_0 = 10;
    static constexpr int GPIO_LEV_0 = 13;
    static constexpr int GPIO_PUD = 37;
    static constexpr int GPIO_CLK_0 = 38;
    static constexpr int GPIO_PUPPDN0 = 57;
    static constexpr int PAD_0_27 = 11;
    static constexpr int IRPT_ENB_IRQ_1 = 4;
    static constexpr int IRPT_DIS_IRQ_1 = 7;
    static constexpr int QA7_CORE0_TINTC = 16;

    static constexpr uint32_t IRPT_OFFSET = 0x0000B200;
    static constexpr uint32_t PADS_OFFSET = 0x00100000;
    static constexpr uint32_t GPIO_OFFSET = 0x00200000;
    static constexpr uint32_t QA7_OFFSET = 0x01000000;

    static constexpr uint32_t PI4_ARM_GICC_CTLR = 0xFF842000;

    static constexpr uint32_t DATA_MASK = 0b11111000000000000000000000000000;
};
