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
    (void)buf;
    (void)n;
}

inline void Bus<BusType::SPI>::read(uint8_t *buf, uint16_t n)
{
    (void)buf;
    (void)n;
}
