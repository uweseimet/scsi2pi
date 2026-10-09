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
#include "gpio_bus.h"

class RpiBus final : public GpioBus
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

    RpiBus(PiType type, Bus::BusProperties p) : GpioBus(p.standard_board), pi_type(type), enable_irq(p.enable_irqs)
    {
    }

    static PiType GetPiType(const string& = "/proc/device-tree/model");

private:

    string SetUp(bool) override;
    void CleanUp() override;

    void InitializeSignals() const;

    void CreateWorkTable();

    void SetSignal(int, bool) const override;

    void DisableIRQ();
    void EnableIRQ();

    void SetDataDirIn(bool) const override;

    // GPIO pin direction setting
    void PinConfig(int, int) const;

    void PinSetSignal(int, bool) const override;

    // Set GPIO pin pull up/down resistor setting
    void DisablePulls(int) const;

    // Set GPIO drive strength
    void SetSignalDriveStrength(uint32_t) const;

    int TargetCommandHandShake(data_in_t) override;
    int TargetReceiveHandShake(data_in_t) override;
    int TargetSendHandShake(data_out_t, int = SEND_NO_DELAY) override;
    uint8_t InitiatorMsgInHandShake() override;
    int InitiatorReceiveHandShake(data_in_t) override;
    int InitiatorSendHandShake(data_out_t) override;

    // Bus signal acquisition
    void Acquire() const override
    {
        SetSignals(*level);
    }

    void SetDAT(uint8_t) const override;

    void WaitNanoSeconds(bool) const override;

    const PiType pi_type;

    const bool enable_irq;
    bool irq_disabled = false;

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

    // GIC CPU interface register
    volatile uint32_t *gicc_mpr = nullptr;

    // RAM copy of GPFSEL0-2  values (GPIO Function Select), mutable because these values are external state.
    // Reading the current data from the copy is faster than directly reading them from the ports.
    mutable array<uint32_t, 3> gpfsel = { };

    // GPIO input level
    volatile uint32_t *level = nullptr;

    // Data setting table for data pins
    array<uint32_t, 256> tblDatSet = { };

    class IrqLock final
    {

    public:

        explicit IrqLock(RpiBus &b) : bus(b)
        {
            bus.DisableIRQ();
        }
        ~IrqLock()
        {
            bus.EnableIRQ();
        }
        IrqLock(const IrqLock&) = delete;
        IrqLock& operator=(const IrqLock&) = delete;

    private:

        RpiBus &bus;
    };
};
