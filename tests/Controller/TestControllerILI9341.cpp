#include "support/hal_stub.hpp"
#include "support/mockBus.hpp"

#include "lcdriv.hpp"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

class TestControllerILI9341 : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        hal::g_transcript.reset();
        bus.reset();
    }

    GPIO_TypeDef dcPort;
    GPIO_TypeDef csPort;
    GPIO_TypeDef rstPort;
    GpioPin dc{&dcPort, GPIO_PIN_0};
    GpioPin cs[1] = {{&csPort, GPIO_PIN_4}};
    GpioPin rst[1] = {{&rstPort, GPIO_PIN_1}};
    PanelMgr<1> mgr{cs, rst};
    MockBus bus;
    Controller<ControllerType::ILI9341, 240, 320, 1> ctrl{dc, &mgr};
};

TEST_F(TestControllerILI9341, pushFramePortraitWindowBulkPixels)
{
    std::vector<uint8_t> px(153600);
    for (std::size_t i = 0; i < px.size(); ++i)
    {
        px[i] = static_cast<uint8_t>(i);
    }
    ctrl.pushFrame(bus, px.data(), 0);

    // setColRange (select+deselect) + setPageRange (select+deselect) +
    // writePixels (select, RAMWR cmd, sendBulk) → 5 sends + 1 bulkCall
    ASSERT_EQ(bus.sends.size(), 5u);
    EXPECT_EQ(bus.sends[0], std::vector<uint8_t>({0x2A}));
    EXPECT_EQ(bus.sends[1], std::vector<uint8_t>({0x00, 0x00, 0x00, 0xEF}));
    EXPECT_EQ(bus.sends[2], std::vector<uint8_t>({0x2B}));
    EXPECT_EQ(bus.sends[3], std::vector<uint8_t>({0x00, 0x00, 0x01, 0x3F}));
    EXPECT_EQ(bus.sends[4], std::vector<uint8_t>({0x2C}));

    ASSERT_EQ(bus.bulkCalls.size(), 1u);
    EXPECT_EQ(bus.bulkCalls[0].n, 153600u);
    EXPECT_EQ(bus.bulkCalls[0].buf, px.data());
}

TEST_F(TestControllerILI9341, pushFrameLandscapeWindow)
{
    PanelMgr<1> mgrL{cs, rst};
    Controller<ControllerType::ILI9341, 320, 240, 1> ctrlL{dc, &mgrL};
    std::vector<uint8_t> px(153600);
    ctrlL.pushFrame(bus, px.data(), 0);

    ASSERT_EQ(bus.sends.size(), 5u);
    ASSERT_EQ(bus.bulkCalls.size(), 1u);

    EXPECT_EQ(bus.sends[1], std::vector<uint8_t>({0x00, 0x00, 0x01, 0x3F}));
    EXPECT_EQ(bus.sends[3], std::vector<uint8_t>({0x00, 0x00, 0x00, 0xEF}));
    EXPECT_EQ(bus.bulkCalls[0].n, 153600u);
}

TEST_F(TestControllerILI9341, pushFrameSmallSingleBulk)
{
    PanelMgr<1> mgrS{cs, rst};
    Controller<ControllerType::ILI9341, 240, 100, 1> ctrlS{dc, &mgrS};
    std::vector<uint8_t> px(48000);
    ctrlS.pushFrame(bus, px.data(), 0);

    ASSERT_EQ(bus.sends.size(), 5u);
    ASSERT_EQ(bus.bulkCalls.size(), 1u);

    EXPECT_EQ(bus.sends[3], std::vector<uint8_t>({0x00, 0x00, 0x00, 0x63}));
    EXPECT_EQ(bus.bulkCalls[0].n, 48000u);
}

TEST_F(TestControllerILI9341, fillScreenColorRowStream)
{
    ctrl.fillScreen(bus, 0xF800, 0);

    // setColRange + setPageRange + writeSolidColor(RAMWR + 320 rows + deselect)
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
    const uint32_t id = ctrl.readID(bus, 0);

    EXPECT_EQ(id, 0x934100u);
    ASSERT_EQ(bus.sends.size(), 1u);
    EXPECT_EQ(bus.sends[0], std::vector<uint8_t>({0xD3}));
    EXPECT_EQ(bus.readCalls, 1u);
    EXPECT_EQ(bus.lastReadN, 4u);
    // select(CS low) → dcLow → send → dcHigh → read → deselect(CS high)
    ASSERT_EQ(hal::g_transcript.gpio.size(), 4u);
    EXPECT_EQ(hal::g_transcript.gpio[0].state, GPIO_PIN_RESET); // cs select
    EXPECT_EQ(hal::g_transcript.gpio[1].state, GPIO_PIN_RESET); // dcLow
    EXPECT_EQ(hal::g_transcript.gpio[2].state, GPIO_PIN_SET);   // dcHigh
    EXPECT_EQ(hal::g_transcript.gpio[3].state, GPIO_PIN_SET);   // deselect
}

TEST_F(TestControllerILI9341, readIDNonZeroDummyByte)
{
    bus.preset = {0xFF, 0x93, 0x41, 0x00};
    EXPECT_EQ(ctrl.readID(bus, 0), 0x934100u);
}

TEST_F(TestControllerILI9341, readIDModuleIdPassthrough)
{
    bus.preset = {0x00, 0x93, 0x41, 0x12};
    EXPECT_EQ(ctrl.readID(bus, 0), 0x934112u);
}

TEST_F(TestControllerILI9341, setOrientationWritesMadtcl)
{
    ctrl.setOrientation(bus, 0x60, 0);

    ASSERT_EQ(bus.sends.size(), 2u);
    EXPECT_EQ(bus.sends[0], std::vector<uint8_t>({0x36}));
    EXPECT_EQ(bus.sends[1], std::vector<uint8_t>({0x60}));
    // select(CS low) → dcLow → send → dcHigh → send → deselect(CS high)
    ASSERT_EQ(hal::g_transcript.gpio.size(), 4u);
    EXPECT_EQ(hal::g_transcript.gpio[0].state, GPIO_PIN_RESET); // cs select
    EXPECT_EQ(hal::g_transcript.gpio[1].state, GPIO_PIN_RESET); // dcLow
    EXPECT_EQ(hal::g_transcript.gpio[2].state, GPIO_PIN_SET);   // dcHigh
    EXPECT_EQ(hal::g_transcript.gpio[3].state, GPIO_PIN_SET);   // deselect
}
