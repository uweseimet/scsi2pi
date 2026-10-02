//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
//---------------------------------------------------------------------------

#include "rp1_pio.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <system_error>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "rpi_bus.h"

// The kernel interface of the rp1-pio driver, provided by the linux-libc-dev package of Raspberry Pi OS
#if __has_include(<misc/rp1_pio_if.h>)
#include <misc/rp1_pio_if.h>
#define HAS_RP1_PIO
#endif

#ifdef HAS_RP1_PIO

namespace
{

constexpr int DATA_PIN_COUNT = 9;

constexpr uint32_t PIN_MASK = ((1U << DATA_PIN_COUNT) - 1) << PIN_DT0 | 1U << PIN_REQ;

// The kernel driver uses 4 DMA buffers of this size per direction
constexpr uint16_t DMA_BUFFER_SIZE = 2048;
constexpr uint16_t DMA_BUFFER_COUNT = 4;

// The maximum number of bytes per transfer, a multiple of the sector size
constexpr size_t MAX_TRANSFER_BYTES = 32768;

// The kernel driver aborts a DMA transfer after 1 s without progress, the same applies to the final handshake
constexpr auto HANDSHAKE_TIMEOUT = chrono::seconds(1);

// 31 delay cycles per instruction at 200 MHz, i.e. 6 x 32 cycles (~1 us) between setting the data and REQ
constexpr uint16_t SETTLE_DELAY = 31 << 8;

// pull; out pindirs, 9 (assert the data and parity bits); settle; set pindirs, 1 (assert REQ);
// wait 0 gpio ACK; set pindirs, 0 (release REQ); wait 1 gpio ACK
constexpr array<uint16_t, 11> SEND_PROGRAM = { 0x80a0, 0x6089 | SETTLE_DELAY, 0xa042 | SETTLE_DELAY, 0xa042
    | SETTLE_DELAY, 0xa042 | SETTLE_DELAY, 0xa042 | SETTLE_DELAY, 0xa042 | SETTLE_DELAY, 0xe081, 0x2000 | PIN_ACK,
    0xe080, 0x2080 | PIN_ACK };

// pull (the byte count - 1); mov x, osr; loop: set pindirs, 1 (assert REQ); wait 0 gpio ACK; in pins, 8 (4 bytes per
// FIFO word); set pindirs, 0 (release REQ); wait 1 gpio ACK; jmp x--, loop.
// Loaded at offset 0 because of the absolute jump target. Stalls at "pull" when done.
constexpr array<uint16_t, 8> RECEIVE_PROGRAM = { 0x80a0, 0xa027, 0xe081, 0x2000 | PIN_ACK, 0x4008 | (7 << 8), 0xe080,
    0x2080 | PIN_ACK, 0x0042 };

// See the PIO chapter of the RP2040 datasheet for the register layout
constexpr uint32_t CLKDIV_1 = 1U << 16;
constexpr uint32_t WRAP_BOTTOM_LSB = 7;
constexpr uint32_t WRAP_TOP_LSB = 12;
constexpr uint32_t IN_SHIFT_RIGHT = 1U << 18;
constexpr uint32_t OUT_SHIFT_RIGHT = 1U << 19;
constexpr uint32_t AUTOPUSH = 1U << 16;
constexpr uint32_t OUT_BASE_LSB = 0;
constexpr uint32_t SET_BASE_LSB = 5;
constexpr uint32_t IN_BASE_LSB = 15;
constexpr uint32_t OUT_COUNT_LSB = 20;
constexpr uint32_t SET_COUNT_LSB = 26;

// Both programs drive REQ with "set"
constexpr uint32_t SET_PIN_REQ = PIN_REQ << SET_BASE_LSB | 1U << SET_COUNT_LSB;

}

Rp1Pio::~Rp1Pio()
{
    // Closing the device releases the state machine, the program memory and the DMA channels
    if (fd != -1) {
        close(fd);
    }
}

string Rp1Pio::Init(volatile uint32_t *io, const volatile uint32_t *l)
{
    io_bank = io;
    level = l;

    fd = open("/dev/pio0", O_RDWR | O_CLOEXEC);
    if (fd == -1) {
        return "Can't open /dev/pio0: "s + system_error(errno, generic_category()).what();
    }

    // A mask of 0 claims any unused state machine
    rp1_pio_sm_claim_args claim = { };
    const int s = ioctl(fd, PIO_IOC_SM_CLAIM, &claim);
    if (s < 0) {
        return "Can't claim a PIO state machine: "s + system_error(errno, generic_category()).what();
    }
    sm = static_cast<uint16_t>(s);

    rp1_pio_add_program_args receive = { .num_instrs = RECEIVE_PROGRAM.size(), .origin = 0, .instrs = { } };
    ranges::copy(RECEIVE_PROGRAM, receive.instrs);
    rp1_pio_add_program_args send = { .num_instrs = SEND_PROGRAM.size(), .origin = RP1_PIO_ORIGIN_ANY, .instrs = { } };
    ranges::copy(SEND_PROGRAM, send.instrs);
    int offset = -1;
    if (ioctl(fd, PIO_IOC_ADD_PROGRAM, &receive) != 0 || (offset = ioctl(fd, PIO_IOC_ADD_PROGRAM, &send)) < 0) {
        return "Can't load the PIO programs: "s + system_error(errno, generic_category()).what();
    }
    send_offset = static_cast<uint16_t>(offset);

    for (const uint16_t dir : { RP1_PIO_DIR_TO_SM, RP1_PIO_DIR_FROM_SM }) {
        if (rp1_pio_sm_config_xfer_args xfer = { .sm = sm, .dir = dir, .buf_size = DMA_BUFFER_SIZE, .buf_count =
            DMA_BUFFER_COUNT }; !Ioctl(PIO_IOC_SM_CONFIG_XFER, &xfer)) {
            return "Can't configure PIO DMA: "s + system_error(errno, generic_category()).what();
        }
    }

    SetUpStateMachine(false);

    return "";
}

int Rp1Pio::Send(span<const uint32_t> data)
{
    Start();

    // One word per byte
    auto *words_out = const_cast<uint32_t*>(data.data()); // NOSONAR The driver does not write to the data
    const size_t sent = Transfer(RP1_PIO_DIR_TO_SM, words_out, data.size_bytes()) / sizeof(uint32_t);

    // The DMA transfer has finished, but the last bytes may still be in the FIFO
    const bool success = sent == data.size() && WaitForCompletion(true);

    SetUpStateMachine(false);

    return success ? static_cast<int>(data.size()) : static_cast<int>(sent);
}

int Rp1Pio::Receive(data_in_t buf)
{
    assert(!buf.empty() && !(buf.size() % 4));

    SetUpStateMachine(true);

    rp1_pio_sm_put_args put = { .sm = sm, .blocking = 1, .rsvd = 0, .data = static_cast<uint32_t>(buf.size() - 1) };
    Ioctl(PIO_IOC_SM_PUT, &put);
    Start();

    // Four bytes per word
    words.resize(buf.size() / 4);
    const size_t received = Transfer(RP1_PIO_DIR_FROM_SM, words.data(), buf.size()) / sizeof(uint32_t);

    const bool success = received == words.size() && WaitForCompletion(false);

    SetUpStateMachine(false);

    for (size_t i = 0; i < received; ++i) {
        // Invert because of negative logic, the first byte is in the lowest 8 bits
        const uint32_t w = ~words[i];
        for (size_t j = 0; j < 4; ++j) {
            buf[i * 4 + j] = static_cast<uint8_t>(w >> (8 * j));
        }
    }

    return success ? static_cast<int>(buf.size()) : static_cast<int>(received * 4);
}

bool Rp1Pio::Ioctl(unsigned long request, void *args) const
{
    return ioctl(fd, request, args) >= 0;
}

// Returns the number of bytes transferred
size_t Rp1Pio::Transfer(uint16_t dir, void *data, size_t bytes)
{
    size_t transferred = 0;
    while (transferred < bytes) {
        const size_t count = min(bytes - transferred, MAX_TRANSFER_BYTES);
        rp1_pio_sm_xfer_data_args xfer = { .sm = sm, .dir = dir, .data_bytes = static_cast<uint16_t>(count), .data =
            static_cast<uint8_t*>(data) + transferred };
        if (!Ioctl(PIO_IOC_SM_XFER_DATA, &xfer)) {
            Abort(dir);
            break;
        }
        transferred += count;
    }

    return transferred;
}

// Hand the pins over to the PIO and start the state machine
void Rp1Pio::Start() const
{
    SetPinFunction(RP1_GPIO_FUNC_PIO);
    rp1_pio_sm_set_enabled_args enable = { .mask = static_cast<uint16_t>(1U << sm), .enable = 1, .rsvd = 0 };
    Ioctl(PIO_IOC_SM_SET_ENABLED, &enable);
}

// Disable the state machine, release the pins and prepare the program for the next transfer
void Rp1Pio::SetUpStateMachine(bool receive)
{
    rp1_pio_sm_set_enabled_args disable = { .mask = static_cast<uint16_t>(1U << sm), .enable = 0, .rsvd = 0 };
    Ioctl(PIO_IOC_SM_SET_ENABLED, &disable);

    SetPinFunction(RpiBus::RP1_FUNCSEL_SYS_RIO);

    const uint16_t start = receive ? 0 : send_offset;
    const uint16_t end = start + static_cast<uint16_t>(receive ? RECEIVE_PROGRAM.size() : SEND_PROGRAM.size()) - 1;
    rp1_pio_sm_init_args init = { .sm = sm, .initial_pc = start, .config = {
        .clkdiv = CLKDIV_1,
        .execctrl = static_cast<uint32_t>(start) << WRAP_BOTTOM_LSB | static_cast<uint32_t>(end) << WRAP_TOP_LSB,
        .shiftctrl = IN_SHIFT_RIGHT | OUT_SHIFT_RIGHT | (receive ? AUTOPUSH : 0),
        .pinctrl = SET_PIN_REQ
            | (receive ? PIN_DT0 << IN_BASE_LSB : PIN_DT0 << OUT_BASE_LSB | DATA_PIN_COUNT << OUT_COUNT_LSB) } };
    Ioctl(PIO_IOC_SM_INIT, &init);

    rp1_pio_sm_clear_fifos_args clear = { .sm = sm };
    Ioctl(PIO_IOC_SM_CLEAR_FIFOS, &clear);
    rp1_pio_sm_set_pins_args pins = { .sm = sm, .rsvd = 0, .values = 0, .mask = PIN_MASK };
    Ioctl(PIO_IOC_SM_SET_PINS, &pins);
    rp1_pio_sm_set_pindirs_args pindirs = { .sm = sm, .rsvd = 0, .dirs = 0, .mask = PIN_MASK };
    Ioctl(PIO_IOC_SM_SET_PINDIRS, &pindirs);
    rp1_pio_sm_restart_args restart = { .mask = static_cast<uint16_t>(1U << sm) };
    Ioctl(PIO_IOC_SM_RESTART, &restart);
    // jmp start
    rp1_pio_sm_exec_args exec = { .sm = sm, .instr = start, .blocking = 0, .rsvd = 0 };
    Ioctl(PIO_IOC_SM_EXEC, &exec);
}

// Switch the data pins and REQ between PIO and software control
void Rp1Pio::SetPinFunction(uint32_t function) const
{
    for (int pin = PIN_DT0; pin < PIN_DT0 + DATA_PIN_COUNT; ++pin) {
        io_bank[pin * 2 + 1] = (io_bank[pin * 2 + 1] & ~RpiBus::RP1_FUNCSEL_MASK) | function;
    }
    io_bank[PIN_REQ * 2 + 1] = (io_bank[PIN_REQ * 2 + 1] & ~RpiBus::RP1_FUNCSEL_MASK) | function;
}

// Wait for the handshake of the last byte, i.e. until REQ and ACK have been released for several consecutive reads
bool Rp1Pio::WaitForCompletion(bool check_fifo) const
{
    const auto deadline = chrono::steady_clock::now() + HANDSHAKE_TIMEOUT;
    int idle_count = 0;
    while (idle_count < 4) {
        if (chrono::steady_clock::now() > deadline) {
            return false;
        }

        rp1_pio_sm_fifo_state_args fifo = { .sm = sm, .tx = 1, .rsvd = 0, .level = 0, .empty = 1, .full = 0 };
        if (check_fifo) {
            Ioctl(PIO_IOC_SM_FIFO_STATE, &fifo);
        }

        // Signals are active low
        const uint32_t signals = *level;
        if (fifo.empty && (signals & (1U << PIN_REQ)) && (signals & (1U << PIN_ACK))) {
            ++idle_count;
        }
        else {
            idle_count = 0;
        }
    }

    return true;
}

// The initiator stopped the handshake. Reconfiguring the DMA terminates the pending transfer.
void Rp1Pio::Abort(uint16_t dir)
{
    SetUpStateMachine(false);

    rp1_pio_sm_config_xfer_args xfer = { .sm = sm, .dir = dir, .buf_size = DMA_BUFFER_SIZE, .buf_count =
        DMA_BUFFER_COUNT };
    Ioctl(PIO_IOC_SM_CONFIG_XFER, &xfer);
}

#else

Rp1Pio::~Rp1Pio() = default;

string Rp1Pio::Init(volatile uint32_t*, const volatile uint32_t*)
{
    return "This build does not support the RP1 PIO";
}

int Rp1Pio::Send(span<const uint32_t>)
{
    return 0;
}

int Rp1Pio::Receive(data_in_t)
{
    return 0;
}

#endif
