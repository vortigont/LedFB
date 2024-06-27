#include <stdint.h>

/*
    Some usefull links for color math

    https://www.reddit.com/r/FastLED/comments/dhoce6/nblend_function_explanation/
    https://github.com/marmilicious/FastLED_examples/blob/master/fade_up_down_example.ino
    https://github.com/marmilicious/FastLED_examples/blob/master/fade_toward_solid_color.ino
    https://github.com/FastLED/FastLED/issues/238   (Brightening equivalent of nscale8)

    // random TwinkleUp
    https://pastebin.com/yGeeGCRy
    https://github.com/Aircoookie/WLED/blob/1daa97545bfac3ca509b5afc804fe23e64b3f77d/wled00/FX.cpp#L3622


*/


namespace color {

/**
 * Fast RGB565 pixel blending
 * @param fg      The foreground color in uint16_t RGB565 format
 * @param bg      The background color in uint16_t RGB565 format
 * @param alpha   The alpha in range 0-255
 * 
 * https://stackoverflow.com/questions/17436527/error-uint16-t-undeclared
 * https://github.com/tricorderproject/arducordermini/pull/1
 **/
uint16_t alphaBlendRGB565( uint32_t fg, uint32_t bg, uint8_t alpha );


} // namespace color
