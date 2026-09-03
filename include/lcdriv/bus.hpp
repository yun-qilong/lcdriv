#pragma once
#include <cstdint>

#include "core.hpp"

template <BusType bus>
class Bus;

template <>
class Bus<BusType::SPI>
{
  public:
    explicit Bus(SPI_HandleTypeDef *spi);
    void send(const uint8_t *buf, uint16_t n);
    void read(uint8_t *buf, uint16_t n);

  private:
    SPI_HandleTypeDef *spi_;
};

inline Bus<BusType::SPI>::Bus(SPI_HandleTypeDef *spi) : spi_(spi) {}

inline void Bus<BusType::SPI>::send(const uint8_t *buf, uint16_t n)
{
    if (n == 0)
    {
        return;
    }
    HAL_SPI_Transmit(spi_, const_cast<uint8_t *>(buf), n, HAL_MAX_DELAY);
}

inline void Bus<BusType::SPI>::read(uint8_t *buf, uint16_t n)
{
    if (n == 0)
    {
        return;
    }
    HAL_SPI_Receive(spi_, buf, n, HAL_MAX_DELAY);
}
