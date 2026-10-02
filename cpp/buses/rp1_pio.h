//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Pi 5: The REQ/ACK handshake of DATA IN and DATA OUT runs on a state machine of the RP1 PIO.
// Polling the RP1 GPIO over PCIe takes too long for initiators like the Mac Plus, which use blind
// transfers without checking REQ for each byte.
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include "shared/s2p_defs.h"

class Rp1Pio
{

public:

    Rp1Pio() = default;
    ~Rp1Pio();
    Rp1Pio(const Rp1Pio&) = delete;
    Rp1Pio& operator=(const Rp1Pio&) = delete;

    // The pointers are the IO_BANK0 registers and the SYS_RIO0 input level register
    string Init(volatile uint32_t*, const volatile uint32_t*);

    // Each word contains the output enable bits of a data byte and its parity (DT0-DT7, DP)
    int Send(span<const uint32_t>);

    // The number of bytes must be a multiple of 4
    int Receive(data_in_t);

private:

    bool Ioctl(unsigned long, void*) const;

    size_t Transfer(uint16_t, void*, size_t);

    void Start() const;

    void SetUpStateMachine(bool);

    void SetPinFunction(uint32_t) const;

    bool WaitForCompletion(bool) const;

    void Abort(uint16_t);

    int fd = -1;

    uint16_t sm = 0;

    uint16_t send_offset = 0;

    volatile uint32_t *io_bank = nullptr;

    const volatile uint32_t *level = nullptr;

    vector<uint32_t> words;
};
