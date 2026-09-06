#pragma once
#include <cstdint>
#include <type_traits>

// 库级公共 GPIO 引脚描述（端口 + 引脚），供组件构造注入使用（PanelMgr 屏表、
// Driver 装配入参）。依赖 include 顺序契约：GPIO_TypeDef 由使用方 TU 先让 HAL
// （或测试替身）可见（同各组件头）。
struct GpioPin
{
    GPIO_TypeDef *port;
    uint16_t pin;
};

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

template <BusType bus, ControllerType ctrl, int M, int N, int P = 1, bool dma = false>
class LcdDriver;

template <int M, int N, int P, bool dma>
class LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N, P, dma>;
