#include "support/hal_stub.hpp"

#include "lcdriv.hpp"

int main()
{
    LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320> portrait;
    LcdDriver<BusType::SPI, ControllerType::ILI9341, 320, 240> landscape;
    return portrait.width() == 240 && portrait.height() == 320 && landscape.width() == 320 &&
                   landscape.height() == 240
               ? 0
               : 1;
}
