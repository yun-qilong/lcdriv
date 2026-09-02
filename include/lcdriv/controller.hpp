#pragma once
#include <cstdint>

#include "core.hpp"

template <ControllerType ctrl, int M, int N>
class Controller
{
    static_assert(IsSupported<BusType::SPI, ctrl>::value,
                  "Controller: unsupported controller type");
};

template <int M, int N>
class Controller<ControllerType::ILI9341, M, N>
{
  public:
    static constexpr uint8_t kMadctlDefault = (M > N) ? 0x60 : 0x00;

    Controller(GPIO_TypeDef *dc_port, uint16_t dc_pin, GPIO_TypeDef *rst_port, uint16_t rst_pin);

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
    void bringUp(B &bus);

    friend class LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N>;

    void dcLow();
    void dcHigh();
    void rstLow();
    void rstHigh();

    GPIO_TypeDef *dc_port_;
    uint16_t dc_pin_;
    GPIO_TypeDef *rst_port_;
    uint16_t rst_pin_;
};

template <int M, int N>
inline Controller<ControllerType::ILI9341, M, N>::Controller(GPIO_TypeDef *dc_port, uint16_t dc_pin,
                                                             GPIO_TypeDef *rst_port,
                                                             uint16_t rst_pin)
    : dc_port_(dc_port), dc_pin_(dc_pin), rst_port_(rst_port), rst_pin_(rst_pin)
{
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::writeCommand(B &bus, uint8_t cmd)
{
    (void)bus;
    (void)cmd;
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::writeData(B &bus, const uint8_t *data,
                                                                 uint16_t n)
{
    (void)bus;
    (void)data;
    (void)n;
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::pushFrame(B &bus, const uint8_t *px)
{
    (void)bus;
    (void)px;
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::fillScreen(B &bus, uint16_t color)
{
    (void)bus;
    (void)color;
}

template <int M, int N>
template <typename B>
inline uint32_t Controller<ControllerType::ILI9341, M, N>::readID(B &bus)
{
    (void)bus;
    return 0;
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::setOrientation(B &bus, uint8_t madctl)
{
    (void)bus;
    (void)madctl;
}

template <int M, int N>
template <typename B>
inline void Controller<ControllerType::ILI9341, M, N>::bringUp(B &bus)
{
    (void)bus;
}

template <int M, int N>
inline void Controller<ControllerType::ILI9341, M, N>::dcLow()
{
}

template <int M, int N>
inline void Controller<ControllerType::ILI9341, M, N>::dcHigh()
{
}

template <int M, int N>
inline void Controller<ControllerType::ILI9341, M, N>::rstLow()
{
}

template <int M, int N>
inline void Controller<ControllerType::ILI9341, M, N>::rstHigh()
{
}
