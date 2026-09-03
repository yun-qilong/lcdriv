#pragma once

#include "core.hpp"

template <int P>
class PanelMgr
{
    static_assert(P >= 1, "PanelMgr: P must be at least 1");

  public:
    PanelMgr(const GpioPin (&cs)[P], const GpioPin (&rst)[P]);

    [[nodiscard]] bool isBusy() const
    {
        return busy_;
    }

    [[nodiscard]] bool select(int panel);
    void deselect();
    void reset(int panel);

  private:
    GpioPin cs_[P];
    GpioPin rst_[P];
    volatile bool busy_ = false;
};

template <int P>
inline PanelMgr<P>::PanelMgr(const GpioPin (&cs)[P], const GpioPin (&rst)[P])
{
    for (int i = 0; i < P; ++i)
    {
        cs_[i] = cs[i];
        rst_[i] = rst[i];
    }
}

template <int P>
inline bool PanelMgr<P>::select(int panel)
{
    if (busy_)
    {
        return false;
    }
    HAL_GPIO_WritePin(cs_[panel].port, cs_[panel].pin, GPIO_PIN_RESET);
    busy_ = true;
    return true;
}

template <int P>
inline void PanelMgr<P>::deselect()
{
    for (int i = 0; i < P; ++i)
    {
        HAL_GPIO_WritePin(cs_[i].port, cs_[i].pin, GPIO_PIN_SET);
    }
    busy_ = false;
}

template <int P>
inline void PanelMgr<P>::reset(int panel)
{
    GPIO_TypeDef *port = rst_[panel].port;
    uint16_t pin = rst_[panel].pin;
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
    HAL_Delay(120);
}
