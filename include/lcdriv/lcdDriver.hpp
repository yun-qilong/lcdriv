#pragma once
#include <cstdint>
#include <new>
#include <type_traits>

#include "core.hpp"

#include "bus.hpp"
#include "controller.hpp"
#include "panelMgr.hpp"

template <BusType bus, ControllerType ctrl, int M, int N, int P, bool dma>
class LcdDriver
{
    static_assert(IsSupported<bus, ctrl>::value,
                  "LcdDriver: unsupported (bus, controller) combination");
};

template <int M, int N, int P, bool dma>
class LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N, P, dma>
{
    static_assert(M > 0 && N > 0, "LcdDriver: M and N must be positive");

  public:
    static constexpr int kBytes = M * N * 2;

    LcdDriver() = default;
    LcdDriver(const LcdDriver &) = delete;
    LcdDriver &operator=(const LcdDriver &) = delete;
    LcdDriver(LcdDriver &&) = delete;
    LcdDriver &operator=(LcdDriver &&) = delete;

    bool init(SPI_HandleTypeDef *spi, GpioPin dc, const GpioPin (&cs)[P], const GpioPin (&rst)[P])
    {
        return assembly_(spi, dc, cs, rst);
    }

    explicit LcdDriver(SPI_HandleTypeDef *spi, GpioPin dc, const GpioPin (&cs)[P],
                       const GpioPin (&rst)[P])
    {
        assembly_(spi, dc, cs, rst);
    }

    bool pushFrame(int panel, const uint8_t *px)
    {
        if (mgr_->select(panel))
        {
            ctrl_->pushFrame(*bus_, px);
            return true;
        }
        return false;
    }

    void fillScreen(int panel, uint16_t color)
    {
        mgr_->select(panel);
        ctrl_->fillScreen(*bus_, color);
        mgr_->deselect();
    }

    uint32_t readID(int panel)
    {
        mgr_->select(panel);
        uint32_t id = ctrl_->readID(*bus_);
        mgr_->deselect();
        return id;
    }

    void setOrientation(int panel, uint8_t madctl)
    {
        mgr_->select(panel);
        ctrl_->setOrientation(*bus_, madctl);
        mgr_->deselect();
    }

    [[nodiscard]] int width() const
    {
        return M;
    }
    [[nodiscard]] int height() const
    {
        return N;
    }

  private:
    using BusImpl = Bus<BusType::SPI, P, dma>;
    using CtrlImpl = Controller<ControllerType::ILI9341, M, N>;
    using PanelMgrImpl = PanelMgr<P>;

    bool assembly_(SPI_HandleTypeDef *spi, GpioPin dc, const GpioPin (&cs)[P],
                   const GpioPin (&rst)[P])
    {
        if (bus_)
        {
            bus_->~BusImpl();
        }
        if (ctrl_)
        {
            ctrl_->~CtrlImpl();
        }
        if (mgr_)
        {
            mgr_->~PanelMgrImpl();
        }

        mgr_ = new (&mgrBuf_) PanelMgrImpl(cs, rst);
        ctrl_ = new (&ctrlBuf_) CtrlImpl(dc);
        bus_ = new (&busBuf_) BusImpl(spi, mgr_);

        for (int i = 0; i < P; ++i)
        {
            mgr_->select(i);
            mgr_->reset(i);
            ctrl_->initSequence(*bus_);
            mgr_->deselect();
        }
        return true;
    }

    alignas(BusImpl) std::aligned_storage_t<sizeof(BusImpl), alignof(BusImpl)> busBuf_;
    alignas(
        PanelMgrImpl) std::aligned_storage_t<sizeof(PanelMgrImpl), alignof(PanelMgrImpl)> mgrBuf_;
    alignas(CtrlImpl) std::aligned_storage_t<sizeof(CtrlImpl), alignof(CtrlImpl)> ctrlBuf_;

    BusImpl *bus_ = nullptr;
    PanelMgrImpl *mgr_ = nullptr;
    CtrlImpl *ctrl_ = nullptr;
};
