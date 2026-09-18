#pragma once

#include "core.hpp"

template <int NumPanels>
class PanelMgr
{
    static_assert(NumPanels >= 1, "PanelMgr: NumPanels must be at least 1");

  public:
    PanelMgr(const GpioPin (&cs)[NumPanels], const GpioPin (&rst)[NumPanels])
    {
        for (int i = 0; i < NumPanels; ++i)
        {
            cs_[i] = cs[i];
            rst_[i] = rst[i];
        }
    }

    [[nodiscard]] bool occupyBus()
    {
        if (busy_)
        {
            return false;
        }
        busy_ = true;
        return true;
    }

    void releaseBus()
    {
        busy_ = false;
        deselect();
    }

    [[nodiscard]] bool isBusy() const
    {
        return busy_;
    }

    void select(int panel)
    {
        HAL_GPIO_WritePin(cs_[panel].port, cs_[panel].pin, GPIO_PIN_RESET);
    }

    void deselect()
    {
        for (int i = 0; i < NumPanels; ++i)
        {
            HAL_GPIO_WritePin(cs_[i].port, cs_[i].pin, GPIO_PIN_SET);
        }
    }

    void reset(int panel)
    {
        GPIO_TypeDef *port = rst_[panel].port;
        uint16_t pin = rst_[panel].pin;
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
        HAL_Delay(20);
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
        HAL_Delay(150);
    }

  private:
    GpioPin cs_[NumPanels];
    GpioPin rst_[NumPanels];
    volatile bool busy_ = false;
};
