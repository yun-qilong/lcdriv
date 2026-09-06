#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

struct GPIO_TypeDef
{
};

struct SPI_HandleTypeDef
{
    int tag = 0;
    void (*txCpltCallback)(SPI_HandleTypeDef *) = nullptr;
};

typedef enum
{
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET
} GPIO_PinState;

#define GPIO_PIN_0 0x0001U
#define GPIO_PIN_1 0x0002U
#define GPIO_PIN_2 0x0004U
#define GPIO_PIN_3 0x0008U
#define GPIO_PIN_4 0x0010U
#define GPIO_PIN_5 0x0020U
#define GPIO_PIN_6 0x0040U
#define GPIO_PIN_7 0x0080U
#define GPIO_PIN_8 0x0100U
#define GPIO_PIN_9 0x0200U
#define GPIO_PIN_10 0x0400U
#define GPIO_PIN_11 0x0800U
#define GPIO_PIN_12 0x1000U
#define GPIO_PIN_13 0x2000U
#define GPIO_PIN_14 0x4000U
#define GPIO_PIN_15 0x8000U

#define HAL_MAX_DELAY 0xFFFFFFFFU

typedef enum
{
    HAL_OK = 0x00U,
    HAL_ERROR = 0x01U,
    HAL_BUSY = 0x02U,
    HAL_TIMEOUT = 0x03U
} HAL_StatusTypeDef;

typedef enum
{
    HAL_SPI_TX_COMPLETE_CB_ID
} HAL_SPI_CallbackIDTypeDef;

namespace hal
{

struct TxCall
{
    const SPI_HandleTypeDef *h;
    std::vector<uint8_t> bytes;
};

struct RxCall
{
    const SPI_HandleTypeDef *h;
    uint16_t n;
};

struct GpioWrite
{
    GPIO_TypeDef *port;
    uint16_t pin;
    GPIO_PinState state;
};

struct CallEvent
{
    char kind;         // 'T'=tx 'R'=rx 'G'=gpio 'D'=delay
    std::size_t index; // 在对应列表中的序号（0 起）
};

struct DmaStart
{
    const SPI_HandleTypeDef *h;
    std::vector<uint8_t> bytes;
};

struct DmaCompletion
{
    void (*fn)(SPI_HandleTypeDef *);
    SPI_HandleTypeDef *h;
};

struct Transcript
{
    std::vector<TxCall> tx;
    std::vector<RxCall> rx;
    std::vector<GpioWrite> gpio;
    std::vector<uint32_t> delays;
    std::vector<CallEvent> events;
    std::vector<uint8_t> rxPreset;
    std::size_t rxPresetPos = 0;
    std::vector<DmaStart> dmaStarts;
    std::vector<DmaCompletion> dmaCompletions;

    void reset()
    {
        tx.clear();
        rx.clear();
        gpio.clear();
        delays.clear();
        events.clear();
        rxPreset.clear();
        rxPresetPos = 0;
        dmaStarts.clear();
        dmaCompletions.clear();
    }

    const TxCall *lastTx() const
    {
        return tx.empty() ? nullptr : &tx.back();
    }
    std::size_t totalTxBytes() const
    {
        std::size_t s = 0;
        for (const auto &t : tx)
            s += t.bytes.size();
        return s;
    }
};

extern Transcript g_transcript;

void fireTxDmaComplete(SPI_HandleTypeDef *hspi);

} // namespace hal

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size,
                                   uint32_t Timeout);

HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size,
                                  uint32_t Timeout);

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);

void HAL_Delay(uint32_t Delay);

HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size);

HAL_StatusTypeDef HAL_SPI_RegisterCallback(SPI_HandleTypeDef *hspi, HAL_SPI_CallbackIDTypeDef cbId,
                                           void (*callback)(SPI_HandleTypeDef *));
