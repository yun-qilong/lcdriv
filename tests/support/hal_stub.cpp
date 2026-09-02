#include "hal_stub.hpp"

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

    std::size_t idx = 0;
    for (uint16_t i = 0; i < Size; ++i)
    {
        if (idx < hal::g_transcript.rxPreset.size())
            pData[i] = hal::g_transcript.rxPreset[idx++];
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
}

void HAL_Delay(uint32_t Delay)
{
    hal::g_transcript.delays.push_back(Delay);
}
