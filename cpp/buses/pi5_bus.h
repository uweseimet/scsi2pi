//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Pi 5: The GPIO pins are connected to the RP1 chip, which is completely different from the GPIO of the other Pis.
//
//---------------------------------------------------------------------------

#pragma once

#include <memory>
#include <span>
#include <vector>
#include <linux/gpio.h>
#include "bus.h"
#include "rp1_pio.h"

class Pi5Bus final : public Bus
{

public:

    explicit Pi5Bus(bool);

    bool IsRaspberryPi() const override
    {
        return true;
    }

    int TargetReceiveHandShake(data_in_t) override;
    int TargetSendHandShake(data_out_t, int = SEND_NO_DELAY) override;

    // RP1 register layout (see the RP1 peripherals datasheet), offsets in 32-bit words
    static constexpr int IO_BANK_OFFSET = 0;
    static constexpr int RIO_OFFSET = 0x10000 / 4;
    static constexpr int PADS_OFFSET = 0x20000 / 4;
    static constexpr int RIO_OUT = 0;
    static constexpr int RIO_OE = 1;
    static constexpr int RIO_SYNC_IN = 2;
    static constexpr int XOR = 0x1000 / 4;
    static constexpr int SET = 0x2000 / 4;
    static constexpr int CLR = 0x3000 / 4;

    // Function select of a pin in its IO_BANK0 control register
    static constexpr uint32_t FUNCSEL_MASK = 0x1f;
    static constexpr uint32_t FUNCSEL_SYS_RIO = 5;
    static constexpr uint32_t FUNCSEL_PIO = 7;

    static constexpr int GetControlRegister(int pin)
    {
        return IO_BANK_OFFSET + pin * 2 + 1;
    }

private:

    string SetUp(bool) override;
    void CleanUp() override;

    void Reset() const override;

    void Acquire() const override
    {
        SetSignals(gpio[RIO_OFFSET + RIO_SYNC_IN]);
    }

    void SetBSY(bool) const override;

    void SetSEL(bool) const override;

    void SetDAT(uint8_t) const override;

    void SetSignal(int, bool) const override;

    void SetDataDirIn(bool) const override;

    void WaitNanoSeconds(bool) const override;

    uint8_t WaitForSelection() override;

    void InitializeSignals() const;

    void PinConfig(int, bool) const;

    void PinSetSignal(int, bool) const;

    void SetOutputEnable(uint32_t, bool) const;

    void SetPads(uint32_t) const;

    // IO_BANK0, SYS_RIO0 and PADS_BANK0
    span<volatile uint32_t> gpio;

    // Set to -1 for the STANDARD board
    int pin_ind = PIN_IND;
    int pin_tad = PIN_TAD;
    int pin_dtd = PIN_DTD;

    // SEL signal event request
    gpioevent_request selevreq = { };

    int epoll_fd = -1;

    // The output enable bits of the data pins and their parity for each byte
    array<uint32_t, 256> data_oe = { };

    // RAM copy of the current output enable bits of the data pins, so that SetDAT() needs a single register write
    mutable uint32_t current_data_oe = 0;

    // Handshakes for DATA IN and DATA OUT run on the RP1 PIO if available
    unique_ptr<Rp1Pio> pio;
    vector<uint32_t> pio_words;

    static constexpr size_t MAP_SIZE = 0x30000;

    static constexpr uint32_t PAD_OUTPUT_DISABLE = 1U << 7;
    static constexpr uint32_t PAD_INPUT_ENABLE = 1U << 6;
    static constexpr uint32_t PAD_DRIVE_MASK = 3U << 4;
    static constexpr uint32_t PAD_DRIVE_8MA = 2U << 4;
    static constexpr uint32_t PAD_DRIVE_12MA = 3U << 4;
    static constexpr uint32_t PAD_PULL_MASK = 3U << 2;

    static constexpr uint32_t DATA_MASK = 0b1'1111'1111U << PIN_DT0;

    // Shorter transfers (e.g. STATUS or MESSAGE IN) are not worth the PIO setup overhead
    static constexpr size_t MIN_PIO_BYTES = 16;

    // RP1 pad updates lag the register write, so the bus settle delay is longer than on the other Pis
    static constexpr auto BUS_SETTLE_DELAY = chrono::nanoseconds(1'000);
    static constexpr auto DAYNAPORT_DELAY = chrono::nanoseconds(100'000);

    static constexpr auto SIGNAL_TABLE = to_array<int>( { PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4, PIN_DT5, PIN_DT6,
        PIN_DT7, PIN_DP, PIN_SEL, PIN_ATN, PIN_RST, PIN_ACK, PIN_BSY, PIN_MSG, PIN_CD, PIN_IO, PIN_REQ });
};
