#include "support/hal_stub.hpp"

#include "lcdriv.hpp"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

class TestBusSPI : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        hal::g_transcript.reset();
    }

    void TearDown() override
    {
        EXPECT_TRUE(hal::g_transcript.gpio.empty());
        EXPECT_TRUE(hal::g_transcript.delays.empty());
    }

    SPI_HandleTypeDef h1;
    SPI_HandleTypeDef h2;
};

TEST_F(TestBusSPI, ctorStoresHandle)
{
    Bus<BusType::SPI> bus(&h1);
    const uint8_t b = 0xAA;
    bus.send(&b, 1);

    ASSERT_EQ(hal::g_transcript.tx.size(), 1u);
    EXPECT_EQ(hal::g_transcript.tx.back().h, &h1);
    EXPECT_EQ(hal::g_transcript.tx.back().bytes, std::vector<uint8_t>({0xAA}));
}

TEST_F(TestBusSPI, sendForwardsSingleByte)
{
    Bus<BusType::SPI> bus(&h1);
    const uint8_t b = 0x01;
    bus.send(&b, 1);

    ASSERT_EQ(hal::g_transcript.tx.size(), 1u);
    EXPECT_EQ(hal::g_transcript.tx.back().bytes, std::vector<uint8_t>({0x01}));
}

TEST_F(TestBusSPI, sendForwardsLongBlock)
{
    Bus<BusType::SPI> bus(&h1);
    std::vector<uint8_t> v(30000);
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        v[i] = static_cast<uint8_t>(i % 251);
    }
    bus.send(v.data(), static_cast<uint16_t>(v.size()));

    ASSERT_EQ(hal::g_transcript.tx.size(), 1u);
    EXPECT_EQ(hal::g_transcript.tx.back().bytes, v);
}

TEST_F(TestBusSPI, sendAcceptsMaxLength)
{
    Bus<BusType::SPI> bus(&h1);
    std::vector<uint8_t> v(65535, 0x55);
    bus.send(v.data(), static_cast<uint16_t>(v.size()));

    ASSERT_EQ(hal::g_transcript.tx.size(), 1u);
    EXPECT_EQ(hal::g_transcript.tx.back().bytes.size(), 65535u);
    EXPECT_EQ(hal::g_transcript.tx.back().bytes, v);
}

TEST_F(TestBusSPI, sendZeroLengthNoOp)
{
    Bus<BusType::SPI> bus(&h1);
    bus.send(nullptr, 0);

    EXPECT_TRUE(hal::g_transcript.tx.empty());
}

TEST_F(TestBusSPI, sendRecordsPerCallInOrder)
{
    Bus<BusType::SPI> bus(&h1);
    const uint8_t a = 1;
    const uint8_t b = 2;
    bus.send(&a, 1);
    bus.send(&b, 1);

    ASSERT_EQ(hal::g_transcript.tx.size(), 2u);
    EXPECT_EQ(hal::g_transcript.tx[0].bytes, std::vector<uint8_t>({1}));
    EXPECT_EQ(hal::g_transcript.tx[1].bytes, std::vector<uint8_t>({2}));
}

TEST_F(TestBusSPI, sendFromBufferOffset)
{
    Bus<BusType::SPI> bus(&h1);
    const std::vector<uint8_t> v = {0x00, 0x01, 0x02, 0x03};
    bus.send(v.data() + 1, 2);

    ASSERT_EQ(hal::g_transcript.tx.size(), 1u);
    EXPECT_EQ(hal::g_transcript.tx.back().bytes, std::vector<uint8_t>({0x01, 0x02}));
}

TEST_F(TestBusSPI, sendLeavesBufferUntouched)
{
    Bus<BusType::SPI> bus(&h1);
    std::vector<uint8_t> v(100);
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        v[i] = static_cast<uint8_t>(i);
    }
    const std::vector<uint8_t> snapshot = v;
    bus.send(v.data(), static_cast<uint16_t>(v.size()));

    EXPECT_EQ(v, snapshot);
}

TEST_F(TestBusSPI, readFillsFromPreset)
{
    Bus<BusType::SPI> bus(&h1);
    hal::g_transcript.rxPreset = {0x93, 0x41, 0x00};
    uint8_t buf[3] = {};
    bus.read(buf, 3);

    ASSERT_EQ(hal::g_transcript.rx.size(), 1u);
    EXPECT_EQ(hal::g_transcript.rx.back().n, 3u);
    std::vector<uint8_t> got(buf, buf + 3);
    EXPECT_EQ(got, std::vector<uint8_t>({0x93, 0x41, 0x00}));
}

TEST_F(TestBusSPI, readConsumesPresetInOrder)
{
    Bus<BusType::SPI> bus(&h1);
    hal::g_transcript.rxPreset = {1, 2, 3, 4};
    uint8_t a[2] = {};
    uint8_t b[2] = {};
    bus.read(a, 2);
    bus.read(b, 2);

    ASSERT_EQ(hal::g_transcript.rx.size(), 2u);
    EXPECT_EQ(std::vector<uint8_t>(a, a + 2), std::vector<uint8_t>({1, 2}));
    EXPECT_EQ(std::vector<uint8_t>(b, b + 2), std::vector<uint8_t>({3, 4}));
}

TEST_F(TestBusSPI, readZeroLengthNoOp)
{
    Bus<BusType::SPI> bus(&h1);
    uint8_t buf[1] = {};
    bus.read(buf, 0);

    EXPECT_TRUE(hal::g_transcript.rx.empty());
}

TEST_F(TestBusSPI, readFillsZeroWhenPresetExhausted)
{
    Bus<BusType::SPI> bus(&h1);
    hal::g_transcript.rxPreset = {0x93};
    uint8_t buf[3] = {};
    bus.read(buf, 3);

    ASSERT_EQ(hal::g_transcript.rx.size(), 1u);
    EXPECT_EQ(hal::g_transcript.rx.back().n, 3u);
    std::vector<uint8_t> got(buf, buf + 3);
    EXPECT_EQ(got, std::vector<uint8_t>({0x93, 0x00, 0x00}));
}

TEST_F(TestBusSPI, mixedOpsKeepHandleAndOrder)
{
    Bus<BusType::SPI> a(&h1);
    Bus<BusType::SPI> b(&h2);
    const uint8_t x = 1;
    const uint8_t y = 2;
    uint8_t z = 0;
    a.send(&x, 1);
    b.send(&y, 1);
    a.read(&z, 1);

    ASSERT_EQ(hal::g_transcript.tx.size(), 2u);
    EXPECT_EQ(hal::g_transcript.tx[0].h, &h1);
    EXPECT_EQ(hal::g_transcript.tx[1].h, &h2);
    ASSERT_EQ(hal::g_transcript.rx.size(), 1u);
    EXPECT_EQ(hal::g_transcript.rx.back().h, &h1);

    const auto &ev = hal::g_transcript.events;
    ASSERT_EQ(ev.size(), 3u);
    EXPECT_EQ(ev[0].kind, 'T');
    EXPECT_EQ(ev[0].index, 0u);
    EXPECT_EQ(ev[1].kind, 'T');
    EXPECT_EQ(ev[1].index, 1u);
    EXPECT_EQ(ev[2].kind, 'R');
    EXPECT_EQ(ev[2].index, 0u);
}
