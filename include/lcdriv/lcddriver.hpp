#pragma once
#include <cstdint>
#include <optional>

#include "core.hpp"

#include "bus.hpp"
#include "controller.hpp"

template <BusType bus, ControllerType ctrl, int M, int N>
class LcdDriver
{
    static_assert(IsSupported<bus, ctrl>::value,
                  "LcdDriver: unsupported (bus, controller) combination");
};

template <int M, int N>
class LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N>
{
  public:
    static constexpr int kBytes = M * N * 2;

    LcdDriver() = default;

    bool init(SPI_HandleTypeDef *spi, GPIO_TypeDef *cs_port, uint16_t cs_pin, GPIO_TypeDef *dc_port,
              uint16_t dc_pin, GPIO_TypeDef *rst_port, uint16_t rst_pin);

    explicit LcdDriver(SPI_HandleTypeDef *spi, GPIO_TypeDef *cs_port, uint16_t cs_pin,
                       GPIO_TypeDef *dc_port, uint16_t dc_pin, GPIO_TypeDef *rst_port,
                       uint16_t rst_pin);

    void pushFrame(const uint8_t *px);
    void fillScreen(uint16_t color);
    uint32_t readID();
    void setOrientation(uint8_t madctl);

    [[nodiscard]] int width() const
    {
        return M;
    }
    [[nodiscard]] int height() const
    {
        return N;
    }

  private:
    bool assembly_(SPI_HandleTypeDef *spi, GPIO_TypeDef *cs_port, uint16_t cs_pin,
                   GPIO_TypeDef *dc_port, uint16_t dc_pin, GPIO_TypeDef *rst_port,
                   uint16_t rst_pin);

    void beginTransaction_();
    void endTransaction_();

    std::optional<Bus<BusType::SPI>> bus_;
    std::optional<Controller<ControllerType::ILI9341, M, N>> ctrl_;
    GPIO_TypeDef *cs_port_ = nullptr;
    uint16_t cs_pin_ = 0;
};

template <int M, int N>
inline bool LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N>::init(
    SPI_HandleTypeDef *spi, GPIO_TypeDef *cs_port, uint16_t cs_pin, GPIO_TypeDef *dc_port,
    uint16_t dc_pin, GPIO_TypeDef *rst_port, uint16_t rst_pin)
{
    return assembly_(spi, cs_port, cs_pin, dc_port, dc_pin, rst_port, rst_pin);
}

template <int M, int N>
inline LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N>::LcdDriver(
    SPI_HandleTypeDef *spi, GPIO_TypeDef *cs_port, uint16_t cs_pin, GPIO_TypeDef *dc_port,
    uint16_t dc_pin, GPIO_TypeDef *rst_port, uint16_t rst_pin)
{
    assembly_(spi, cs_port, cs_pin, dc_port, dc_pin, rst_port, rst_pin);
}

template <int M, int N>
inline void LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N>::pushFrame(const uint8_t *px)
{
    (void)px;
}

template <int M, int N>
inline void LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N>::fillScreen(uint16_t color)
{
    (void)color;
}

template <int M, int N>
inline uint32_t LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N>::readID()
{
    return 0;
}

template <int M, int N>
inline void LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N>::setOrientation(uint8_t madctl)
{
    (void)madctl;
}

template <int M, int N>
inline bool LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N>::assembly_(
    SPI_HandleTypeDef *spi, GPIO_TypeDef *cs_port, uint16_t cs_pin, GPIO_TypeDef *dc_port,
    uint16_t dc_pin, GPIO_TypeDef *rst_port, uint16_t rst_pin)
{
    (void)spi;
    (void)cs_port;
    (void)cs_pin;
    (void)dc_port;
    (void)dc_pin;
    (void)rst_port;
    (void)rst_pin;
    return true;
}

template <int M, int N>
inline void LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N>::beginTransaction_()
{
}

template <int M, int N>
inline void LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N>::endTransaction_()
{
}
