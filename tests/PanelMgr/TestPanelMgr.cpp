#include "support/hal_stub.hpp"

#include "lcdriv.hpp"

#include <vector>

#include <gtest/gtest.h>

class TestPanelMgr : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        hal::g_transcript.reset();
    }

    GPIO_TypeDef p0;
    GPIO_TypeDef p1;
    GPIO_TypeDef p2;
    GpioPin cs_[3] = {{&p0, GPIO_PIN_0}, {&p1, GPIO_PIN_1}, {&p2, GPIO_PIN_2}};
    GpioPin rst_[3] = {{&p0, GPIO_PIN_8}, {&p1, GPIO_PIN_9}, {&p2, GPIO_PIN_10}};
    PanelMgr<3> mgr{cs_, rst_};
};

TEST_F(TestPanelMgr, initiallyIdle)
{
    EXPECT_FALSE(mgr.isBusy());
    EXPECT_TRUE(hal::g_transcript.gpio.empty());
}

TEST_F(TestPanelMgr, selectLowersRequestedCs)
{
    EXPECT_TRUE(mgr.select(1));

    EXPECT_TRUE(mgr.isBusy());
    ASSERT_EQ(hal::g_transcript.gpio.size(), 1u);
    const auto &g = hal::g_transcript.gpio[0];
    EXPECT_EQ(g.port, &p1);
    EXPECT_EQ(g.pin, GPIO_PIN_1);
    EXPECT_EQ(g.state, GPIO_PIN_RESET);
}

TEST_F(TestPanelMgr, selectRejectedWhileBusy)
{
    EXPECT_TRUE(mgr.select(0));
    EXPECT_FALSE(mgr.select(2)); // 已有一块屏被选中 → 拒绝

    EXPECT_TRUE(mgr.isBusy());
    ASSERT_EQ(hal::g_transcript.gpio.size(), 1u); // 只有第一次 select 写了 GPIO
}

TEST_F(TestPanelMgr, deselectRaisesAllCsAndFreesBus)
{
    EXPECT_TRUE(mgr.select(0));
    mgr.deselect();

    EXPECT_FALSE(mgr.isBusy());
    ASSERT_EQ(hal::g_transcript.gpio.size(), 4u);               // select + 3 条 CS 拉高
    EXPECT_EQ(hal::g_transcript.gpio[0].state, GPIO_PIN_RESET); // cs0 选中
    EXPECT_EQ(hal::g_transcript.gpio[1].state, GPIO_PIN_SET);   // cs0 释放
    EXPECT_EQ(hal::g_transcript.gpio[2].state, GPIO_PIN_SET);   // cs1（本就没低，幂等）
    EXPECT_EQ(hal::g_transcript.gpio[3].state, GPIO_PIN_SET);   // cs2
    EXPECT_EQ(hal::g_transcript.gpio[1].port, &p0);
    EXPECT_EQ(hal::g_transcript.gpio[2].port, &p1);
    EXPECT_EQ(hal::g_transcript.gpio[3].port, &p2);

    EXPECT_TRUE(mgr.select(1)); // 释放后可再次选中
}

TEST_F(TestPanelMgr, deselectWhenIdleRaisesAllCs)
{
    mgr.deselect();

    EXPECT_FALSE(mgr.isBusy());
    ASSERT_EQ(hal::g_transcript.gpio.size(), 3u);
    for (const auto &g : hal::g_transcript.gpio)
    {
        EXPECT_EQ(g.state, GPIO_PIN_SET);
    }
}

TEST_F(TestPanelMgr, resetPulsesRstPinWithTimings)
{
    mgr.reset(1);

    // RST[1] = (p1, GPIO_PIN_9)：高 → 低 → 高
    ASSERT_EQ(hal::g_transcript.gpio.size(), 3u);
    EXPECT_EQ(hal::g_transcript.gpio[0].port, &p1);
    EXPECT_EQ(hal::g_transcript.gpio[0].pin, GPIO_PIN_9);
    EXPECT_EQ(hal::g_transcript.gpio[0].state, GPIO_PIN_SET);
    EXPECT_EQ(hal::g_transcript.gpio[1].state, GPIO_PIN_RESET);
    EXPECT_EQ(hal::g_transcript.gpio[2].state, GPIO_PIN_SET);

    // 延时：高 5ms → 低 10ms → 高 120ms
    EXPECT_EQ(hal::g_transcript.delays, std::vector<uint32_t>({5u, 10u, 120u}));

    // reset 不触碰事务忙状态
    EXPECT_FALSE(mgr.isBusy());
}

TEST_F(TestPanelMgr, resetIndependentOfBusyState)
{
    EXPECT_TRUE(mgr.select(0)); // 事务进行中
    mgr.reset(2);               // 复位另一块屏仍可执行（RST 与 CS 互斥无关）

    ASSERT_EQ(hal::g_transcript.gpio.size(), 4u); // select cs0 + reset rst2 三笔
    EXPECT_EQ(hal::g_transcript.gpio[0].port, &p0);
    EXPECT_EQ(hal::g_transcript.gpio[1].port, &p2);
    EXPECT_EQ(hal::g_transcript.gpio[1].pin, GPIO_PIN_10);
    EXPECT_TRUE(mgr.isBusy());

    mgr.deselect();
    EXPECT_FALSE(mgr.isBusy());
}
