#include "support/hal_stub.hpp"

#include "lcdriv.hpp"

#include <gtest/gtest.h>

static_assert(IsSupported<BusType::SPI, ControllerType::ILI9341>::value,
              "SPI+ILI9341 must be supported");
static_assert(!IsSupported<BusType::I2C, ControllerType::ILI9341>::value,
              "I2C+ILI9341 not implemented");
static_assert(!IsSupported<BusType::SPI, ControllerType::ST7789>::value,
              "SPI+ST7789 not implemented");
static_assert(!IsSupported<BusType::I2C, ControllerType::ST7789>::value,
              "I2C+ST7789 not implemented");

static_assert(LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320>::kBytes == 153600,
              "240x320 frame bytes");
static_assert(LcdDriver<BusType::SPI, ControllerType::ILI9341, 320, 240>::kBytes == 153600,
              "320x240 frame bytes");
static_assert(LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 100>::kBytes == 48000,
              "small frame bytes");

static_assert(Controller<ControllerType::ILI9341, 240, 320>::kMadctlDefault == 0x00,
              "portrait MADCTL");
static_assert(Controller<ControllerType::ILI9341, 320, 240>::kMadctlDefault == 0x60,
              "landscape MADCTL candidate");

using PortraitDriver = LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320>;
using LandscapeDriver = LcdDriver<BusType::SPI, ControllerType::ILI9341, 320, 240>;

TEST(CompileTime, DefaultCtor_AndDimensions)
{
    PortraitDriver portrait;
    LandscapeDriver landscape;

    EXPECT_EQ(portrait.width(), 240);
    EXPECT_EQ(portrait.height(), 320);
    EXPECT_EQ(landscape.width(), 320);
    EXPECT_EQ(landscape.height(), 240);
}

TEST(CompileTime, ConstantsExposed)
{
    EXPECT_EQ(static_cast<int>(PortraitDriver::kBytes), 153600);
    EXPECT_EQ(static_cast<int>(Controller<ControllerType::ILI9341, 240, 320>::kMadctlDefault),
              0x00);
}
