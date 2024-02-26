//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cstdint>

#pragma once

class SignalControl
{

public:

    SignalControl() = default;
    virtual ~SignalControl() = default;

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

    virtual bool GetSignal(int) const = 0;
    virtual void SetSignal(int, bool) = 0;
};
