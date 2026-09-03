#include "support/hal_stub.hpp"
#include "support/mockBus.hpp"

#include "lcdriv.hpp"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace
{
std::vector<uint8_t> slice(const std::vector<uint8_t> &v, std::size_t from, std::size_t len)
{
    return std::vector<uint8_t>(v.begin() + static_cast<std::ptrdiff_t>(from),
                                v.begin() + static_cast<std::ptrdiff_t>(from + len));
}
} // namespace

class TestControllerILI9341 : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        hal::g_transcript.reset();
        bus.reset();
    }

    GPIO_TypeDef dcPort;
    GpioPin dc{&dcPort, GPIO_PIN_0};
    MockBus bus;
    Controller<ControllerType::ILI9341, 240, 320> ctrl{dc};
};

TEST_F(TestControllerILI9341, writeCommandSendsOneByteWithDcLow)
{
    ctrl.writeCommand(bus, 0x2A);

    ASSERT_EQ(bus.sends.size(), 1u);
    EXPECT_EQ(bus.sends[0], std::vector<uint8_t>({0x2A}));
    ASSERT_FALSE(hal::g_transcript.gpio.empty());
    const auto &g = hal::g_transcript.gpio.back();
    EXPECT_EQ(g.port, &dcPort);
    EXPECT_EQ(g.pin, GPIO_PIN_0);
    EXPECT_EQ(g.state, GPIO_PIN_RESET);
}

TEST_F(TestControllerILI9341, writeDataSendsBytesWithDcHigh)
{
    const uint8_t data[2] = {0x00, 0xEF};
    ctrl.writeData(bus, data, 2);

    ASSERT_EQ(bus.sends.size(), 1u);
    EXPECT_EQ(bus.sends[0], std::vector<uint8_t>({0x00, 0xEF}));
    ASSERT_FALSE(hal::g_transcript.gpio.empty());
    const auto &g = hal::g_transcript.gpio.back();
    EXPECT_EQ(g.port, &dcPort);
    EXPECT_EQ(g.pin, GPIO_PIN_0);
    EXPECT_EQ(g.state, GPIO_PIN_SET);
}

TEST_F(TestControllerILI9341, writeDataZeroLengthNoOp)
{
    const uint8_t data[1] = {0x01};
    ctrl.writeData(bus, data, 0);

    EXPECT_TRUE(bus.sends.empty());
    EXPECT_TRUE(hal::g_transcript.gpio.empty());
}

TEST_F(TestControllerILI9341, writeDataMaxLengthSingle)
{
    std::vector<uint8_t> v(65535);
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        v[i] = static_cast<uint8_t>(i);
    }
    ctrl.writeData(bus, v.data(), 65535);

    ASSERT_EQ(bus.sends.size(), 1u);
    EXPECT_EQ(bus.sends[0], v);
}

TEST_F(TestControllerILI9341, pushFramePortraitWindowChunkedPixels)
{
    std::vector<uint8_t> px(153600);
    for (std::size_t i = 0; i < px.size(); ++i)
    {
        px[i] = static_cast<uint8_t>(i);
    }
    ctrl.pushFrame(bus, px.data());

    ASSERT_EQ(bus.sends.size(), 8u);
    EXPECT_EQ(bus.sends[0], std::vector<uint8_t>({0x2A}));
    EXPECT_EQ(bus.sends[1], std::vector<uint8_t>({0x00, 0x00, 0x00, 0xEF}));
    EXPECT_EQ(bus.sends[2], std::vector<uint8_t>({0x2B}));
    EXPECT_EQ(bus.sends[3], std::vector<uint8_t>({0x00, 0x00, 0x01, 0x3F}));
    EXPECT_EQ(bus.sends[4], std::vector<uint8_t>({0x2C}));
    EXPECT_EQ(bus.sends[5].size(), 65535u);
    EXPECT_EQ(bus.sends[6].size(), 65535u);
    EXPECT_EQ(bus.sends[7].size(), 22530u);
    EXPECT_EQ(bus.sends[5], slice(px, 0, 65535));
    EXPECT_EQ(bus.sends[6], slice(px, 65535, 65535));
    EXPECT_EQ(bus.sends[7], slice(px, 131070, 22530));

    ASSERT_EQ(hal::g_transcript.gpio.size(), 6u);
    EXPECT_EQ(hal::g_transcript.gpio[0].state, GPIO_PIN_RESET);
    EXPECT_EQ(hal::g_transcript.gpio[1].state, GPIO_PIN_SET);
    EXPECT_EQ(hal::g_transcript.gpio[2].state, GPIO_PIN_RESET);
    EXPECT_EQ(hal::g_transcript.gpio[3].state, GPIO_PIN_SET);
    EXPECT_EQ(hal::g_transcript.gpio[4].state, GPIO_PIN_RESET);
    EXPECT_EQ(hal::g_transcript.gpio[5].state, GPIO_PIN_SET);
}

TEST_F(TestControllerILI9341, pushFrameLandscapeWindow)
{
    Controller<ControllerType::ILI9341, 320, 240> ctrlL{dc};
    std::vector<uint8_t> px(153600);
    ctrlL.pushFrame(bus, px.data());

    ASSERT_EQ(bus.sends.size(), 8u);
    EXPECT_EQ(bus.sends[1], std::vector<uint8_t>({0x00, 0x00, 0x01, 0x3F}));
    EXPECT_EQ(bus.sends[3], std::vector<uint8_t>({0x00, 0x00, 0x00, 0xEF}));
    EXPECT_EQ(bus.sends[5].size(), 65535u);
    EXPECT_EQ(bus.sends[6].size(), 65535u);
    EXPECT_EQ(bus.sends[7].size(), 22530u);
}

TEST_F(TestControllerILI9341, pushFrameSmallSinglePixelChunk)
{
    Controller<ControllerType::ILI9341, 240, 100> ctrlS{dc};
    std::vector<uint8_t> px(48000);
    ctrlS.pushFrame(bus, px.data());

    ASSERT_EQ(bus.sends.size(), 6u);
    EXPECT_EQ(bus.sends[3], std::vector<uint8_t>({0x00, 0x00, 0x00, 0x63}));
    EXPECT_EQ(bus.sends[5].size(), 48000u);
}

TEST_F(TestControllerILI9341, fillScreenColorRowStream)
{
    ctrl.fillScreen(bus, 0xF800);

    ASSERT_EQ(bus.sends.size(), 5u + 320u);
    EXPECT_EQ(bus.sends[0], std::vector<uint8_t>({0x2A}));
    EXPECT_EQ(bus.sends[1], std::vector<uint8_t>({0x00, 0x00, 0x00, 0xEF}));
    EXPECT_EQ(bus.sends[2], std::vector<uint8_t>({0x2B}));
    EXPECT_EQ(bus.sends[3], std::vector<uint8_t>({0x00, 0x00, 0x01, 0x3F}));
    EXPECT_EQ(bus.sends[4], std::vector<uint8_t>({0x2C}));
    std::vector<uint8_t> row(480);
    for (std::size_t i = 0; i < row.size(); ++i)
    {
        row[i] = (i % 2 == 0) ? 0xF8 : 0x00;
    }
    for (std::size_t i = 0; i < 320; ++i)
    {
        EXPECT_EQ(bus.sends[5 + i], row);
    }
}

TEST_F(TestControllerILI9341, readIDDiscardDummyByte)
{
    bus.preset = {0x00, 0x93, 0x41, 0x00};
    const uint32_t id = ctrl.readID(bus);

    EXPECT_EQ(id, 0x934100u);
    ASSERT_EQ(bus.sends.size(), 1u);
    EXPECT_EQ(bus.sends[0], std::vector<uint8_t>({0xD3}));
    EXPECT_EQ(bus.readCalls, 1u);
    EXPECT_EQ(bus.lastReadN, 4u);
    ASSERT_EQ(hal::g_transcript.gpio.size(), 2u);
    EXPECT_EQ(hal::g_transcript.gpio[0].state, GPIO_PIN_RESET);
    EXPECT_EQ(hal::g_transcript.gpio[1].state, GPIO_PIN_SET);
}

TEST_F(TestControllerILI9341, readIDNonZeroDummyByte)
{
    bus.preset = {0xFF, 0x93, 0x41, 0x00};
    EXPECT_EQ(ctrl.readID(bus), 0x934100u);
}

TEST_F(TestControllerILI9341, readIDModuleIdPassthrough)
{
    bus.preset = {0x00, 0x93, 0x41, 0x12};
    EXPECT_EQ(ctrl.readID(bus), 0x934112u);
}

TEST_F(TestControllerILI9341, setOrientationWritesMadtcl)
{
    ctrl.setOrientation(bus, 0x60);

    ASSERT_EQ(bus.sends.size(), 2u);
    EXPECT_EQ(bus.sends[0], std::vector<uint8_t>({0x36}));
    EXPECT_EQ(bus.sends[1], std::vector<uint8_t>({0x60}));
    ASSERT_EQ(hal::g_transcript.gpio.size(), 2u);
    EXPECT_EQ(hal::g_transcript.gpio[0].state, GPIO_PIN_RESET);
    EXPECT_EQ(hal::g_transcript.gpio[1].state, GPIO_PIN_SET);
}
