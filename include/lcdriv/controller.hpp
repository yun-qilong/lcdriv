#pragma once
#include <cstdint>

#include "core.hpp"

template <ControllerType ctrl, int M, int N>
class Controller;

template <int M, int N>
class Controller<ControllerType::ILI9341, M, N>
{
    static_assert(M > 0 && N > 0, "Controller: M and N must be positive");

  public:
    static constexpr uint8_t kMadctlDefault = (M > N) ? 0x60 : 0x00;

    static constexpr uint8_t NOP = 0x00;
    static constexpr uint8_t SWRESET = 0x01;
    static constexpr uint8_t SLPOUT = 0x11;
    static constexpr uint8_t GAMSET = 0x26;
    static constexpr uint8_t DISPON = 0x29;
    static constexpr uint8_t CASET = 0x2A;
    static constexpr uint8_t PASET = 0x2B;
    static constexpr uint8_t RAMWR = 0x2C;
    static constexpr uint8_t TEON = 0x35;
    static constexpr uint8_t MADCTL = 0x36;
    static constexpr uint8_t COLMOD = 0x3A;
    static constexpr uint8_t FRMCTR1 = 0xB1;
    static constexpr uint8_t DISCTRL = 0xB6;
    static constexpr uint8_t PWCTR1 = 0xC0;
    static constexpr uint8_t PWCTR2 = 0xC1;
    static constexpr uint8_t VMCTR1 = 0xC5;
    static constexpr uint8_t VMCTR2 = 0xC7;
    static constexpr uint8_t RDID = 0xD3;
    static constexpr uint8_t ENABLE3G = 0xF2;

    Controller(GpioPin dc);

    template <typename B>
    void writeCommand(B &bus, uint8_t cmd);

    template <typename B>
    void writeData(B &bus, const uint8_t *data, uint16_t n);

    template <typename B>
    void pushFrame(B &bus, const uint8_t *px);

    template <typename B>
    void fillScreen(B &bus, uint16_t color);

    template <typename B>
    uint32_t readID(B &bus);

    template <typename B>
    void setOrientation(B &bus, uint8_t madctl);

  private:
    template <typename B>
    void initSequence(B &bus);

    template <BusType, ControllerType, int, int, int, bool>
    friend class LcdDriver;

    template <typename B>
    void writeReg(B &bus, uint8_t cmd, const uint8_t *data, uint16_t n);
    template <typename B>
    void setColRange(B &bus);
    template <typename B>
    void setPageRange(B &bus);
    template <typename B>
    void writePixels(B &bus, const uint8_t *data, uint32_t n);
    template <typename B>
    void writeSolidColor(B &bus, uint16_t color);

    void dcLow();
    void dcHigh();

    GpioPin dc_;
};

template <int M, int N>
inline Controller<ControllerType::ILI9341, M, N>::Controller(GpioPin dc) : dc_(dc)
{
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::writeCommand(B &bus, uint8_t cmd)
{
    dcLow();
    bus.send(&cmd, 1);
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::writeData(B &bus, const uint8_t *data,
                                                                 uint16_t n)
{
    if (n == 0)
    {
        return;
    }
    dcHigh();
    bus.send(data, n);
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::writeReg(B &bus, uint8_t cmd,
                                                                const uint8_t *data, uint16_t n)
{
    writeCommand(bus, cmd);
    writeData(bus, data, n);
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::setColRange(B &bus)
{
    const uint8_t col[4] = {0x00, 0x00, static_cast<uint8_t>((M - 1) >> 8),
                            static_cast<uint8_t>(M - 1)};
    writeReg(bus, CASET, col, 4);
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::setPageRange(B &bus)
{
    const uint8_t page[4] = {0x00, 0x00, static_cast<uint8_t>((N - 1) >> 8),
                             static_cast<uint8_t>(N - 1)};
    writeReg(bus, PASET, page, 4);
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::writePixels(B &bus, const uint8_t *data,
                                                                   uint32_t n)
{
    writeCommand(bus, RAMWR);
    dcHigh();
    const uint8_t *p = data;
    uint32_t remaining = n;
    while (remaining > 0)
    {
        const uint16_t chunk = remaining > 65535U ? 65535U : static_cast<uint16_t>(remaining);
        bus.send(p, chunk);
        p += chunk;
        remaining -= chunk;
    }
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::pushFrame(B &bus, const uint8_t *px)
{
    setColRange(bus);
    setPageRange(bus);
    writePixels(bus, px, static_cast<uint32_t>(M) * static_cast<uint32_t>(N) * 2U);
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::writeSolidColor(B &bus, uint16_t color)
{
    writeCommand(bus, RAMWR);
    dcHigh();
    const uint8_t hi = static_cast<uint8_t>(color >> 8);
    const uint8_t lo = static_cast<uint8_t>(color & 0xFF);
    uint8_t row[2 * M];
    for (int i = 0; i < 2 * M; ++i)
    {
        row[i] = (i % 2 == 0) ? hi : lo;
    }
    for (int y = 0; y < N; ++y)
    {
        bus.send(row, static_cast<uint16_t>(2 * M));
    }
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::fillScreen(B &bus, uint16_t color)
{
    setColRange(bus);
    setPageRange(bus);
    writeSolidColor(bus, color);
}

template <int M, int N>
template <typename B>
inline uint32_t Controller<ControllerType::ILI9341, M, N>::readID(B &bus)
{
    writeCommand(bus, RDID);
    dcHigh();
    uint8_t id[4] = {};
    bus.read(id, 4);
    return (static_cast<uint32_t>(id[1]) << 16) | (static_cast<uint32_t>(id[2]) << 8) |
           static_cast<uint32_t>(id[3]);
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::setOrientation(B &bus, uint8_t madctl)
{
    writeReg(bus, MADCTL, &madctl, 1);
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::initSequence(B &bus)
{
    writeCommand(bus, SWRESET);
    HAL_Delay(120);
    writeCommand(bus, SLPOUT);
    HAL_Delay(120);

    const uint8_t v55 = 0x55;
    writeReg(bus, COLMOD, &v55, 1);
    const uint8_t vMadctl = kMadctlDefault;
    writeReg(bus, MADCTL, &vMadctl, 1);
    const uint8_t v00 = 0x00;
    writeReg(bus, TEON, &v00, 1);
    const uint8_t v23 = 0x23;
    writeReg(bus, PWCTR1, &v23, 1);
    const uint8_t v10 = 0x10;
    writeReg(bus, PWCTR2, &v10, 1);
    const uint8_t vC5[2] = {0x3E, 0x28};
    writeReg(bus, VMCTR1, vC5, 2);
    const uint8_t v86 = 0x86;
    writeReg(bus, VMCTR2, &v86, 1);
    const uint8_t vB1[2] = {0x00, 0x18};
    writeReg(bus, FRMCTR1, vB1, 2);
    const uint8_t vB6[3] = {0x08, 0x82, 0x27};
    writeReg(bus, DISCTRL, vB6, 3);
    const uint8_t vF2 = 0x00;
    writeReg(bus, ENABLE3G, &vF2, 1);
    const uint8_t v26 = 0x01;
    writeReg(bus, GAMSET, &v26, 1);
    writeCommand(bus, DISPON);
    HAL_Delay(20);
}

template <int M, int N>
inline void Controller<ControllerType::ILI9341, M, N>::dcLow()
{
    HAL_GPIO_WritePin(dc_.port, dc_.pin, GPIO_PIN_RESET);
}

template <int M, int N>
inline void Controller<ControllerType::ILI9341, M, N>::dcHigh()
{
    HAL_GPIO_WritePin(dc_.port, dc_.pin, GPIO_PIN_SET);
}
