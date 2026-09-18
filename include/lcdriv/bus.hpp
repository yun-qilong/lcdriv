#pragma once
#include <algorithm>
#include <cstdint>

#include "core.hpp"
#include "panelMgr.hpp"

template <BusType bus, int NumPanels = 1, bool dma = false>
class Bus;

const uint16_t kMaxChunk = 65535;

template <int NumPanels, bool dma>
class Bus<BusType::SPI, NumPanels, dma>
{
  public:
    explicit Bus(SPI_HandleTypeDef *spi, PanelMgr<NumPanels> *panelMgr)
        : spi_(spi), panelMgr_(panelMgr)
    {
    }

    Bus(const Bus &) = delete;
    Bus &operator=(const Bus &) = delete;
    Bus(Bus &&) = delete;
    Bus &operator=(Bus &&) = delete;

    void send(const uint8_t *buf, uint16_t n)
    {
        if (n == 0)
        {
            return;
        }
        HAL_SPI_Transmit(spi_, const_cast<uint8_t *>(buf), n, HAL_MAX_DELAY);
    }

    void read(uint8_t *buf, uint16_t n)
    {
        if (n == 0)
        {
            return;
        }
        HAL_SPI_Receive(spi_, buf, n, HAL_MAX_DELAY);
    }

    void sendBulk(const uint8_t *buf, uint32_t n, uint16_t maxChunk = kMaxChunk)
    {
        if (n == 0)
        {
            return;
        }

        remaining_ = n;
        next_ = buf;
        maxChunk_ = maxChunk;
        if constexpr (dma)
        {
            active_ = this;
            HAL_SPI_RegisterCallback(spi_, HAL_SPI_TX_COMPLETE_CB_ID, &onTxComplete);
            transmitNext();
        }
        else
        {
            while (remaining_ > 0)
            {
                transmitNext();
            }
            panelMgr_->deselect();
        }
    }

  private:
    SPI_HandleTypeDef *spi_;
    PanelMgr<NumPanels> *panelMgr_;
    static inline Bus *active_ = nullptr;

    const uint8_t *next_ = nullptr;
    uint32_t remaining_ = 0;
    uint16_t maxChunk_ = 65535;

    const uint8_t *advance(uint16_t &chunk)
    {
        chunk = static_cast<uint16_t>(std::min<uint32_t>(remaining_, maxChunk_));
        const uint8_t *buf = next_;
        next_ = buf + chunk;
        remaining_ -= chunk;
        return buf;
    }

    void transmitNext()
    {
        uint16_t chunk = 0;
        const uint8_t *buf = advance(chunk);
        if constexpr (dma)
        {
            HAL_SPI_Transmit_DMA(spi_, const_cast<uint8_t *>(buf), chunk);
        }
        else
        {
            HAL_SPI_Transmit(spi_, const_cast<uint8_t *>(buf), chunk, HAL_MAX_DELAY);
        }
    }

    void transmitOrComplete()
    {
        if (remaining_ > 0)
        {
            transmitNext();
        }
        else
        {
            active_ = nullptr;
            panelMgr_->releaseBus();
        }
    }

    static void onTxComplete(SPI_HandleTypeDef *hspi)
    {
        (void)hspi;
        if (active_)
        {
            active_->transmitOrComplete();
        }
    }
};
