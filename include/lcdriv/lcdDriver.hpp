#pragma once
#include <cstdint>
#include <new>
#include <type_traits>

#include "core.hpp"

#include "bus.hpp"
#include "controller.hpp"
#include "panelMgr.hpp"

template <BusType bus, ControllerType ctrl, int M, int N, int NumPanels, bool dma>
class LcdDriver
{
    static_assert(IsSupported<bus, ctrl>::value,
                  "LcdDriver: unsupported (bus, controller) combination");
};

template <int M, int N, int NumPanels, bool dma>
class LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N, NumPanels, dma>
{
    static_assert(M > 0 && N > 0, "LcdDriver: M and N must be positive");

  public:
    static constexpr int kBytes = M * N * 2;

    LcdDriver() = default;
    LcdDriver(const LcdDriver &) = delete;
    LcdDriver &operator=(const LcdDriver &) = delete;
    LcdDriver(LcdDriver &&) = delete;
    LcdDriver &operator=(LcdDriver &&) = delete;

    bool init(SPI_HandleTypeDef *spi, GpioPin dc, const GpioPin (&cs)[NumPanels],
              const GpioPin (&rst)[NumPanels])
    {
        return assembly_(spi, dc, cs, rst);
    }

    explicit LcdDriver(SPI_HandleTypeDef *spi, GpioPin dc, const GpioPin (&cs)[NumPanels],
                       const GpioPin (&rst)[NumPanels])
    {
        assembly_(spi, dc, cs, rst);
    }

    // CS 由 Controller 内部通过 PanelMgr 管理：
    // - setColRange / setPageRange 各自独立 CS 帧（writeReg）
    // - writePixels 的 CS 由 Bus::sendBulk 在传输完成时释放
    bool pushFrame(int panel, const uint8_t *px)
    {
        if (panelMgr_->occupyBus())
        {
            ctrl_->pushFrame(*bus_, px, panel);
            panelMgr_->releaseBus();
            return true;
        }
        return false;
    }

    void fillScreen(int panel, uint16_t color)
    {
        panelMgr_->occupyBus();
        ctrl_->fillScreen(*bus_, color, panel);
        panelMgr_->releaseBus();
    }

    uint32_t readID(int panel)
    {
        panelMgr_->occupyBus();
        uint32_t id = ctrl_->readID(*bus_, panel);
        panelMgr_->releaseBus();
        return id;
    }

    void setOrientation(int panel, uint8_t madctl)
    {
        panelMgr_->occupyBus();
        ctrl_->setOrientation(*bus_, madctl, panel);
        panelMgr_->releaseBus();
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
    using BusImpl = Bus<BusType::SPI, NumPanels, dma>;
    using CtrlImpl = Controller<ControllerType::ILI9341, M, N, NumPanels>;
    using PanelMgrImpl = PanelMgr<NumPanels>;

    bool assembly_(SPI_HandleTypeDef *spi, GpioPin dc, const GpioPin (&cs)[NumPanels],
                   const GpioPin (&rst)[NumPanels])
    {
        if (bus_)
        {
            bus_->~BusImpl();
        }
        if (ctrl_)
        {
            ctrl_->~CtrlImpl();
        }
        if (panelMgr_)
        {
            panelMgr_->~PanelMgrImpl();
        }

        panelMgr_ = new (&panelMgrBuf_) PanelMgrImpl(cs, rst);
        ctrl_ = new (&ctrlBuf_) CtrlImpl(dc, panelMgr_);
        bus_ = new (&busBuf_) BusImpl(spi, panelMgr_);

        (void)panelMgr_->occupyBus();
        for (int i = 0; i < NumPanels; ++i)
        {
            panelMgr_->reset(i);
            ctrl_->initSequence(*bus_, i);
        }
        panelMgr_->releaseBus();
        return true;
    }

    alignas(BusImpl) std::aligned_storage_t<sizeof(BusImpl), alignof(BusImpl)> busBuf_;
    alignas(PanelMgrImpl)
        std::aligned_storage_t<sizeof(PanelMgrImpl), alignof(PanelMgrImpl)> panelMgrBuf_;
    alignas(CtrlImpl) std::aligned_storage_t<sizeof(CtrlImpl), alignof(CtrlImpl)> ctrlBuf_;

    BusImpl *bus_ = nullptr;
    PanelMgrImpl *panelMgr_ = nullptr;
    CtrlImpl *ctrl_ = nullptr;
};
