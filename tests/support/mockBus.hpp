#pragma once
#include <cstdint>
#include <vector>

struct BulkCall
{
    const uint8_t *buf;
    uint32_t n;
    uint16_t maxChunk;
};

struct MockBus
{
    void send(const uint8_t *buf, uint16_t n)
    {
        sends.emplace_back(buf, buf + n);
    }

    void sendBulk(const uint8_t *buf, uint32_t n, uint16_t maxChunk = 65535)
    {
        bulkCalls.push_back({buf, n, maxChunk});
    }

    void read(uint8_t *buf, uint16_t n)
    {
        ++readCalls;
        lastReadN = n;
        for (uint16_t i = 0; i < n; ++i)
        {
            buf[i] = (pos < preset.size()) ? preset[pos++] : 0x00;
        }
    }

    void reset()
    {
        sends.clear();
        bulkCalls.clear();
        readCalls = 0;
        lastReadN = 0;
        preset.clear();
        pos = 0;
    }

    std::vector<std::vector<uint8_t>> sends;
    std::vector<BulkCall> bulkCalls;
    std::size_t readCalls = 0;
    uint16_t lastReadN = 0;
    std::vector<uint8_t> preset;

  private:
    std::size_t pos = 0;
};
