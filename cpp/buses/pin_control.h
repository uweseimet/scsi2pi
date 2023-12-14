//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include <cstdint>

#pragma once

// Methods that must be implemented by the derived gpiobus classes to control the GPIO pins
class PinControl
{

public:

    PinControl() = default;
    virtual ~PinControl() = default;

    virtual bool GetBSY() const = 0;
    virtual void SetBSY(bool) = 0;

    virtual bool GetSEL() const = 0;
    virtual void SetSEL(bool) = 0;

    virtual bool GetATN() const = 0;
    virtual void SetATN(bool) = 0;

    virtual bool GetACK() const = 0;
    virtual void SetACK(bool) = 0;

    virtual bool GetRST() const = 0;
    virtual void SetRST(bool) = 0;

    virtual bool GetMSG() const = 0;
    virtual void SetMSG(bool) = 0;

    virtual bool GetCD() const = 0;
    virtual void SetCD(bool) = 0;

    virtual bool GetIO() = 0;
    virtual void SetIO(bool) = 0;

    virtual bool GetREQ() const = 0;
    virtual void SetREQ(bool) = 0;

    virtual uint8_t GetDAT() = 0;
    virtual void SetDAT(uint8_t) = 0;

    // GPIO pin direction setting
    virtual void PinConfig(int, int) = 0;

    // GPIO pin pull up/down resistor setting
    virtual void PullConfig(int, int) = 0;

    virtual void SetControl(int, bool) = 0;

    virtual void SetMode(int, int) = 0;
};
