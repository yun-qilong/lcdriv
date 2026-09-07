#include "support/hal_stub.hpp"

#include "lcdriv.hpp"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

class TestLcdDriverSpiIli9341 : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        hal::g_transcript.reset();
    }

    SPI_HandleTypeDef h1;
    GPIO_TypeDef portA;
    GPIO_TypeDef portB;
    GpioPin dc{&portA, GPIO_PIN_0};
    GpioPin cs[1] = {{&portA, GPIO_PIN_4}};
    GpioPin rst[1] = {{&portB, GPIO_PIN_1}};
};

TEST_F(TestLcdDriverSpiIli9341, atomicConstructRunsInitSequence)
{
    LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320, 1> lcd(&h1, dc, cs, rst);

    EXPECT_EQ(lcd.width(), 240);
    EXPECT_EQ(lcd.height(), 320);

    // initSequence: SWRESET + SLPOUT + 12 commands + DISPON = 15 commands total
    // Each command = 1 DC-low send. Some have params = extra DC-high sends.
    // Check that init happened (SWRESET is the first command)
    ASSERT_FALSE(hal::g_transcript.tx.empty());
    EXPECT_EQ(hal::g_transcript.tx[0].bytes, std::vector<uint8_t>({0x01})); // SWRESET

    // initSequence has SWRESET(120ms), SLPOUT(120ms), DISPON(20ms)
    // PanelMgr::reset has 5ms, 10ms, 120ms delays (runs before initSequence)
    // So first delays are from reset: 5, 10, 120; then initSequence: 120, 120, 20
    ASSERT_GE(hal::g_transcript.delays.size(), 5u);
    EXPECT_EQ(hal::g_transcript.delays[0], 5u);   // reset: HIGH
    EXPECT_EQ(hal::g_transcript.delays[1], 10u);  // reset: LOW
    EXPECT_EQ(hal::g_transcript.delays[2], 120u); // reset: HIGH
    EXPECT_EQ(hal::g_transcript.delays[3], 120u); // SWRESET
    EXPECT_EQ(hal::g_transcript.delays[4], 120u); // SLPOUT
}

TEST_F(TestLcdDriverSpiIli9341, defaultCtorThenInit)
{
    LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320, 1> lcd;
    EXPECT_TRUE(lcd.init(&h1, dc, cs, rst));
    EXPECT_EQ(lcd.width(), 240);
    EXPECT_EQ(lcd.height(), 320);
}

TEST_F(TestLcdDriverSpiIli9341, pushFrameWritesPixels)
{
    LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320, 1> lcd(&h1, dc, cs, rst);
    hal::g_transcript.reset();

    std::vector<uint8_t> px(153600, 0xAB);
    EXPECT_TRUE(lcd.pushFrame(0, px.data()));

    // pushFrame: CASET(0x2A) + data + PASET(0x2B) + data + RAMWR(0x2C) + sendBulk
    // The sendBulk with dma=false calls HAL_SPI_Transmit chunks + deselect
    // Check that the command sequence starts correctly
    ASSERT_GE(hal::g_transcript.tx.size(), 3u);
    EXPECT_EQ(hal::g_transcript.tx[0].bytes, std::vector<uint8_t>({0x2A})); // CASET
    EXPECT_EQ(hal::g_transcript.tx[2].bytes, std::vector<uint8_t>({0x2B})); // PASET

    // sendBulk recorded via tx (blocking path calls HAL_SPI_Transmit)
    ASSERT_GE(hal::g_transcript.tx.size(), 5u);
    EXPECT_EQ(hal::g_transcript.tx[4].bytes, std::vector<uint8_t>({0x2C})); // RAMWR

    // deselect should have been called (CS high)
    ASSERT_FALSE(hal::g_transcript.gpio.empty());
    EXPECT_EQ(hal::g_transcript.gpio.back().state, GPIO_PIN_SET);
}

TEST_F(TestLcdDriverSpiIli9341, fillScreenWritesRowPattern)
{
    LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320, 1> lcd(&h1, dc, cs, rst);
    hal::g_transcript.reset();

    lcd.fillScreen(0, 0xF800);

    // fillScreen: CASET + PASET + RAMWR + 320 rows of 480 bytes + deselect
    // Total tx: 2 (CASET cmd+data) + 2 (PASET cmd+data) + 1 (RAMWR) + 320 (rows) = 325
    // But writeReg sends cmd then data separately, so:
    // CASET: writeCommand(0x2A) + writeData(4 bytes) = 2 sends
    // PASET: writeCommand(0x2B) + writeData(4 bytes) = 2 sends
    // RAMWR: writeCommand(0x2C) = 1 send
    // rows: 320 sends
    ASSERT_EQ(hal::g_transcript.tx.size(), 5u + 320u);

    // Verify first row: 0xF800 → hi=0xF8, lo=0x00, repeated 240 times = 480 bytes
    const auto &firstRow = hal::g_transcript.tx[5].bytes;
    ASSERT_EQ(firstRow.size(), 480u);
    EXPECT_EQ(firstRow[0], 0xF8);
    EXPECT_EQ(firstRow[1], 0x00);
    EXPECT_EQ(firstRow[478], 0xF8);
    EXPECT_EQ(firstRow[479], 0x00);

    // deselect
    ASSERT_FALSE(hal::g_transcript.gpio.empty());
    EXPECT_EQ(hal::g_transcript.gpio.back().state, GPIO_PIN_SET);
}

TEST_F(TestLcdDriverSpiIli9341, readIDProtocol)
{
    LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320, 1> lcd(&h1, dc, cs, rst);
    hal::g_transcript.reset();
    hal::g_transcript.rxPreset = {0x00, 0x93, 0x41, 0x00};

    uint32_t id = lcd.readID(0);
    EXPECT_EQ(id, 0x934100u);

    // readID: writeCommand(0xD3) + dcHigh + read(4 bytes)
    ASSERT_GE(hal::g_transcript.tx.size(), 1u);
    EXPECT_EQ(hal::g_transcript.tx[0].bytes, std::vector<uint8_t>({0xD3}));
    ASSERT_EQ(hal::g_transcript.rx.size(), 1u);
    EXPECT_EQ(hal::g_transcript.rx[0].n, 4u);

    // deselect
    ASSERT_FALSE(hal::g_transcript.gpio.empty());
    EXPECT_EQ(hal::g_transcript.gpio.back().state, GPIO_PIN_SET);
}

TEST_F(TestLcdDriverSpiIli9341, setOrientationWritesMadtcl)
{
    LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320, 1> lcd(&h1, dc, cs, rst);
    hal::g_transcript.reset();

    lcd.setOrientation(0, 0x60);

    // setOrientation: writeCommand(0x36) + writeData(0x60)
    ASSERT_GE(hal::g_transcript.tx.size(), 2u);
    EXPECT_EQ(hal::g_transcript.tx[0].bytes, std::vector<uint8_t>({0x36}));
    EXPECT_EQ(hal::g_transcript.tx[1].bytes, std::vector<uint8_t>({0x60}));

    // deselect
    ASSERT_FALSE(hal::g_transcript.gpio.empty());
    EXPECT_EQ(hal::g_transcript.gpio.back().state, GPIO_PIN_SET);
}

TEST_F(TestLcdDriverSpiIli9341, multiPanelInitAndPushFrame)
{
    GpioPin cs2[2] = {{&portA, GPIO_PIN_4}, {&portA, GPIO_PIN_5}};
    GpioPin rst2[2] = {{&portB, GPIO_PIN_1}, {&portB, GPIO_PIN_2}};
    LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320, 2> lcd(&h1, dc, cs2, rst2);
    hal::g_transcript.reset();

    // Push to panel 0
    std::vector<uint8_t> px(153600, 0x11);
    EXPECT_TRUE(lcd.pushFrame(0, px.data()));

    // Push to panel 1 (panel 0 should have been deselected by sendBulk)
    hal::g_transcript.reset();
    std::vector<uint8_t> px2(153600, 0x22);
    EXPECT_TRUE(lcd.pushFrame(1, px2.data()));

    // Verify commands were sent
    ASSERT_GE(hal::g_transcript.tx.size(), 5u);
    EXPECT_EQ(hal::g_transcript.tx[0].bytes, std::vector<uint8_t>({0x2A}));
}

TEST_F(TestLcdDriverSpiIli9341, initSequenceFullOrder)
{
    LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320, 1> lcd(&h1, dc, cs, rst);

    // Collect command bytes: each writeCommand sets DC low then sends 1 byte
    std::vector<uint8_t> cmds;
    for (std::size_t i = 0; i + 1 < hal::g_transcript.events.size(); ++i)
    {
        const auto &ev = hal::g_transcript.events[i];
        if (ev.kind == 'G')
        {
            const auto &g = hal::g_transcript.gpio[ev.index];
            if (g.port == &portA && g.pin == GPIO_PIN_0 && g.state == GPIO_PIN_RESET)
            {
                const auto &next = hal::g_transcript.events[i + 1];
                if (next.kind == 'T' && hal::g_transcript.tx[next.index].bytes.size() == 1)
                {
                    cmds.push_back(hal::g_transcript.tx[next.index].bytes[0]);
                }
            }
        }
    }

    std::vector<uint8_t> expected = {0x01, 0x11, 0x3A, 0x36, 0x35, 0xC0, 0xC1,
                                     0xC5, 0xC7, 0xB1, 0xB6, 0xF2, 0x26, 0x29};
    ASSERT_GE(cmds.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_EQ(cmds[i], expected[i]) << "command at index " << i;
    }
}

// ── DMA (dma=true) MT tests ─────────────────────────────────────────────

class TestLcdDriverSpiIli9341Dma : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        hal::g_transcript.reset();
    }

    void TearDown() override
    {
        // DMA path triggers deselect via callback; delays not expected
        EXPECT_TRUE(hal::g_transcript.delays.empty());
    }

    SPI_HandleTypeDef h1;
    GPIO_TypeDef portA;
    GPIO_TypeDef portB;
    GpioPin dc{&portA, GPIO_PIN_0};
    GpioPin cs[1] = {{&portA, GPIO_PIN_4}};
    GpioPin rst[1] = {{&portB, GPIO_PIN_1}};
};

TEST_F(TestLcdDriverSpiIli9341Dma, pushFrameReturnsImmediatelyAndDmaStarts)
{
    LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320, 1, true> lcd(&h1, dc, cs, rst);
    hal::g_transcript.reset();

    std::vector<uint8_t> px(153600, 0xAB);
    EXPECT_TRUE(lcd.pushFrame(0, px.data()));

    // DMA should have started (first segment only)
    ASSERT_EQ(hal::g_transcript.dmaStarts.size(), 1u);
    EXPECT_EQ(hal::g_transcript.dmaStarts[0].bytes.size(), 65535u);

    // CS should still be low (busy, not deselected yet) — select pulls CS low
    ASSERT_FALSE(hal::g_transcript.gpio.empty());
    EXPECT_EQ(hal::g_transcript.gpio[0].state, GPIO_PIN_RESET); // CS low
    // No deselect (CS high) should have occurred
    for (const auto &g : hal::g_transcript.gpio)
    {
        if (g.pin == GPIO_PIN_4)
        {
            EXPECT_EQ(g.state, GPIO_PIN_RESET);
        }
    }
}

TEST_F(TestLcdDriverSpiIli9341Dma, pushFrameRejectedWhileDmaInFlight)
{
    LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320, 1, true> lcd(&h1, dc, cs, rst);
    hal::g_transcript.reset();

    std::vector<uint8_t> px(153600, 0xAB);
    EXPECT_TRUE(lcd.pushFrame(0, px.data()));

    // Second pushFrame should be rejected (panel busy)
    std::vector<uint8_t> px2(153600, 0xCD);
    EXPECT_FALSE(lcd.pushFrame(0, px2.data()));

    // No new DMA started
    EXPECT_EQ(hal::g_transcript.dmaStarts.size(), 1u);
}

TEST_F(TestLcdDriverSpiIli9341Dma, dmaChainCompletesAndDeselectsCs)
{
    LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320, 1, true> lcd(&h1, dc, cs, rst);
    hal::g_transcript.reset();

    std::vector<uint8_t> px(153600, 0xAB);
    EXPECT_TRUE(lcd.pushFrame(0, px.data()));

    // Simulate 3 DMA completion events (153600 = 65535 + 65535 + 22530)
    hal::fireTxDmaComplete(&h1);
    hal::fireTxDmaComplete(&h1);
    hal::fireTxDmaComplete(&h1);

    // 3 DMA segments started, then deselect
    EXPECT_EQ(hal::g_transcript.dmaStarts.size(), 3u);
    ASSERT_FALSE(hal::g_transcript.gpio.empty());
    EXPECT_EQ(hal::g_transcript.gpio.back().state, GPIO_PIN_SET);
}

TEST_F(TestLcdDriverSpiIli9341Dma, fillScreenIsBlockingEvenWithDma)
{
    LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320, 1, true> lcd(&h1, dc, cs, rst);
    hal::g_transcript.reset();

    lcd.fillScreen(0, 0xF800);

    // fillScreen uses send (not sendBulk), so no DMA
    EXPECT_TRUE(hal::g_transcript.dmaStarts.empty());
    // But deselect should have been called
    ASSERT_FALSE(hal::g_transcript.gpio.empty());
    EXPECT_EQ(hal::g_transcript.gpio.back().state, GPIO_PIN_SET);
}
