#pragma once
#include <cstdint>

#include "core.hpp"

template <BusType bus, int P = 1, bool dma = false>
class Bus;

template <int P, bool dma>
class Bus<BusType::SPI, P, dma>
{
  public:
    explicit Bus(SPI_HandleTypeDef *spi);
    void send(const uint8_t *buf, uint16_t n);
    void read(uint8_t *buf, uint16_t n);

  private:
    SPI_HandleTypeDef *spi_;
};

template <int P, bool dma>
inline Bus<BusType::SPI, P, dma>::Bus(SPI_HandleTypeDef *spi) : spi_(spi)
{
}

template <int P, bool dma>
inline void Bus<BusType::SPI, P, dma>::send(const uint8_t *buf, uint16_t n)
{
    if (n == 0)
    {
        return;
    }
    HAL_SPI_Transmit(spi_, const_cast<uint8_t *>(buf), n, HAL_MAX_DELAY);
}

template <int P, bool dma>
inline void Bus<BusType::SPI, P, dma>::read(uint8_t *buf, uint16_t n)
{
    if (n == 0)
    {
        return;
    }
    HAL_SPI_Receive(spi_, buf, n, HAL_MAX_DELAY);
}
