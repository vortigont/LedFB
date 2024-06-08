#include "colormath.h"

namespace color {

uint16_t alphaBlendRGB565( uint32_t fg, uint32_t bg, uint8_t alpha ){
    alpha = ( alpha + 4 ) >> 3;
    bg = (bg | (bg << 16)) & 0b00000111111000001111100000011111;
    fg = (fg | (fg << 16)) & 0b00000111111000001111100000011111;
    uint32_t result = ((((fg - bg) * alpha) >> 5) + bg) & 0b00000111111000001111100000011111;
    return (result >> 16) | result;
}

} // namespace color
