#pragma once
#include <type_traits>

enum class BusType
{
    SPI,
    I2C
};
enum class ControllerType
{
    ILI9341,
    ST7789
};

template <BusType bus, ControllerType ctrl>
struct IsSupported : std::false_type
{
};

template <>
struct IsSupported<BusType::SPI, ControllerType::ILI9341> : std::true_type
{
};

template <BusType bus, ControllerType ctrl, int M, int N>
class LcdDriver;

template <int M, int N>
class LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N>;
