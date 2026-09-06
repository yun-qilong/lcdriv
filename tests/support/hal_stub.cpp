#include "hal_stub.hpp"

#include <cstring>
#include <utility>

namespace hal
{

Transcript g_transcript;

} // namespace hal

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size,
                                   uint32_t Timeout)
{
    (void)Timeout;
    hal::TxCall call;
    call.h = hspi;
    call.bytes.assign(pData, pData + Size);
    hal::g_transcript.tx.push_back(std::move(call));
    hal::g_transcript.events.push_back({'T', hal::g_transcript.tx.size() - 1});
    return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size,
                                  uint32_t Timeout)
{
    (void)Timeout;
    hal::RxCall call;
    call.h = hspi;
    call.n = Size;
    hal::g_transcript.rx.push_back(call);
    hal::g_transcript.events.push_back({'R', hal::g_transcript.rx.size() - 1});

    for (uint16_t i = 0; i < Size; ++i)
    {
        if (hal::g_transcript.rxPresetPos < hal::g_transcript.rxPreset.size())
            pData[i] = hal::g_transcript.rxPreset[hal::g_transcript.rxPresetPos++];
        else
            pData[i] = 0x00;
    }
    return HAL_OK;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState)
{
    hal::GpioWrite w;
    w.port = GPIOx;
    w.pin = GPIO_Pin;
    w.state = PinState;
    hal::g_transcript.gpio.push_back(w);
    hal::g_transcript.events.push_back({'G', hal::g_transcript.gpio.size() - 1});
}

void HAL_Delay(uint32_t Delay)
{
    hal::g_transcript.delays.push_back(Delay);
    hal::g_transcript.events.push_back({'D', hal::g_transcript.delays.size() - 1});
}

HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size)
{
    hal::DmaStart ds;
    ds.h = hspi;
    ds.bytes.assign(pData, pData + Size);
    hal::g_transcript.dmaStarts.push_back(std::move(ds));
    hal::g_transcript.events.push_back({'M', hal::g_transcript.dmaStarts.size() - 1});
    return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_RegisterCallback(SPI_HandleTypeDef *hspi, HAL_SPI_CallbackIDTypeDef cbId,
                                           void (*callback)(SPI_HandleTypeDef *))
{
    (void)cbId;
    hspi->txCpltCallback = callback;
    return HAL_OK;
}

namespace hal
{

void fireTxDmaComplete(SPI_HandleTypeDef *hspi)
{
    DmaCompletion dc;
    dc.fn = hspi->txCpltCallback;
    dc.h = hspi;
    g_transcript.dmaCompletions.push_back(dc);
    g_transcript.events.push_back({'C', g_transcript.dmaCompletions.size() - 1});
    if (hspi->txCpltCallback)
    {
        hspi->txCpltCallback(hspi);
    }
}

} // namespace hal
