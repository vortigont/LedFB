/*
    This file is a part of LedFB library project
    https://github.com/vortigont/LedFB

    Copyright © 2023 Emil Muratov (vortigont)
*/


/*

All of FastLED's controller templates includes gpio parameter. That makes
mandatory to define gpio number/RGB color order on compile-time due to optimisation for good old AVRs or specific platforms.

ESP32 uses RMT driver via commutation matrix, so actually it does not care about build time optimisation for gpio.
With a bit of modified templates from FastLED, I can create a LEDController with a run-time definable gpio and color order.

with this wrapper class clockless LED strips could be configured run time in a way like this


#include "w2812-rmt.hpp"

// define or load from some eeprom/FS gpio number
int gpio_num = 10;
// define or load from some eeprom/FS color order
EOrder color_order = GRB;
// define or load from some eeprom/FS number of leds required
int CRGB_buffersize = 256;

CRGB* CRGB_buffer;

setup(){
// allocate mem for buffer
CRGB_buffer = new CRGB[numofleds];
// create stripe driver for WS2812B
ESP32RMT_WS2812B wsstrip(gpio_num, color_order);
// attach the driver to FastLED engine
FastLED.addLeds(&wsstrip, CRGB_buffer, CRGB_buffersize);  
}



/Vortigont/

A reference issues
https://github.com/FastLED/FastLED/issues/282
https://github.com/FastLED/FastLED/issues/826


*/
#pragma once
#include <FastLED.h>
#ifdef ESP32

/*
    in FastLED versions >3007003 RmtController class constructor arguments has changed,
    need to provide different one wrappers for newer versions.
    In version 3007007 RmtController class constructor args has changed again :()
    so I simply ignore this unstable API
*/
#if FASTLED_VERSION <3007007 && FASTLED_VERSION >3007003
#error FastLED versions between 3.7.4 and 3.7.6 are not supported
#endif

/*
    Inheritance map:

    static CLEDController &addLeds(CLEDController *pLed, struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0);


class CPixelLEDController : public CLEDController
    class ClocklessController : public CPixelLEDController
        class WS2812Controller800Khz : public ClocklessController
            class WS2812B : public WS2812Controller800Khz
*/

#if FASTLED_VERSION <= 3007008
/// Template extension of the CLEDController class
/// in comparision with CPixelLEDController this template does NOT have EOrder template parameter that defines RGB color ordering
/// it's a bit slower due to switch/case for each color type, but in case you do not care much it could be much handy for handling
/// run-time defined color order configurations. The performance impact on esp32 is negligible.
///
/// @tparam showPolicy a class type that implements pixel data output policy
/// @tparam LANES how many parallel lanes of output to write
/// @tparam MASK bitmask for the output lanes
template<typename showPolicy, int LANES=1, uint32_t MASK=0xFFFFFFFF>
class CPixelLEDControllerUnordered : public CLEDController {
    // color order
    const EOrder _rgb_order;

protected:
    /// Send the LED data to the strip
    /// @param pixels the PixelController object for the LED data
    //virtual void showPixels(PixelController<RGB_ORDER,LANES,MASK> & pixels) = 0;
    template<typename T>
    void showPixels(T& pixels){
        showPolicy& policy = static_cast<showPolicy&>(*this);
        policy.showPixelsPolicy(pixels);
    }

    /// Set all the LEDs on the controller to a given color
    /// @param data the CRGB color to set the LEDs to
    /// @param nLeds the number of LEDs to set to this color
    /// @param scale the RGB scaling value for outputting color
    /// @param rgb_order color order of the stripe that is controlled by this CLEDController
    virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale) {
        switch (_rgb_order){
            // this is the most usual type of ws strips around
            case GRB : {
                PixelController<GRB, LANES, MASK> pixels(data, nLeds, scale, getDither());
                return showPixels(pixels);
            }
            case RBG : {
                PixelController<RBG, LANES, MASK> pixels(data, nLeds, scale, getDither());
                return showPixels(pixels);
            }
            case GBR : {
                PixelController<GBR, LANES, MASK> pixels(data, nLeds, scale, getDither());
                return showPixels(pixels);
            }
            case BRG : {
                PixelController<BRG, LANES, MASK> pixels(data, nLeds, scale, getDither());
                return showPixels(pixels);
            }
            case BGR : {
                PixelController<BGR, LANES, MASK> pixels(data, nLeds, scale, getDither());
                return showPixels(pixels);
            }
            default:
                // consider it as RGB
                PixelController<RGB, LANES, MASK> pixels(data, nLeds, scale, getDither());
                showPixels(pixels);
        }
    }

    /// Write the passed in RGB data out to the LEDs managed by this controller
    /// @param data the RGB data to write out to the strip
    /// @param nLeds the number of LEDs being written out
    /// @param scale the RGB scaling to apply to each LED before writing it out
    virtual void show(const struct CRGB *data, int nLeds, CRGB scale) {
        // nLeds < 0 implies that we want to show them in reverse
        switch (_rgb_order){
            // this is the most usual type of ws strips around
            case GRB : {
                PixelController<GRB, LANES, MASK> pixels(data, nLeds < 0 ? -nLeds : nLeds, scale, getDither());
                if(nLeds < 0) pixels.mAdvance = -pixels.mAdvance;
                return showPixels(pixels);
            }
            case RBG : {
                PixelController<RBG, LANES, MASK> pixels(data, nLeds < 0 ? -nLeds : nLeds, scale, getDither());
                if(nLeds < 0) pixels.mAdvance = -pixels.mAdvance;
                return showPixels(pixels);
            }
            case GBR : {
                PixelController<GBR, LANES, MASK> pixels(data, nLeds < 0 ? -nLeds : nLeds, scale, getDither());
                if(nLeds < 0) pixels.mAdvance = -pixels.mAdvance;
                return showPixels(pixels);
            }
            case BRG : {
                PixelController<BRG, LANES, MASK> pixels(data, nLeds < 0 ? -nLeds : nLeds, scale, getDither());
                if(nLeds < 0) pixels.mAdvance = -pixels.mAdvance;
                return showPixels(pixels);
            }
            case BGR : {
                PixelController<BGR, LANES, MASK> pixels(data, nLeds < 0 ? -nLeds : nLeds, scale, getDither());
                if(nLeds < 0) pixels.mAdvance = -pixels.mAdvance;
                return showPixels(pixels);
            }
            default: {
                // consider it as RGB
                PixelController<RGB, LANES, MASK> pixels(data, nLeds < 0 ? -nLeds : nLeds, scale, getDither());
                if(nLeds < 0) pixels.mAdvance = -pixels.mAdvance;
                showPixels(pixels);
            }
        }
    }

public:
    CPixelLEDControllerUnordered(EOrder rgb_order = GRB) : CLEDController(), _rgb_order(rgb_order) {}

    /// Get the number of lanes of the Controller
    /// @returns LANES from template
    int lanes() const { return LANES; }
};

#if FASTLED_VERSION <= 3007003
/* ESP32 RMT clockless controller
  this is a stripped template of ClocklessController from FastLED library that does not include
  gpio number as template parameter but accept it as a class constructor parameter.
  This allowing to create an instance with run-time definable gpio and color order
*/
template <int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ESP32RMT_ClocklessController : public CPixelLEDControllerUnordered<ESP32RMT_ClocklessController<XTRA0, FLIP, WAIT_TIME>>
{
private:

    // -- The actual controller object for ESP32
    ESP32RMTController mRMTController;

    // -- Verify that the pin is valid
    // no static checks for run-time defined gpio
    //static_assert(FastPin<DATA_PIN>::validpin(), "Invalid pin specified");

public:
    ESP32RMT_ClocklessController(uint8_t pin, EOrder rgb_order, unsigned t1, unsigned t2, unsigned t3)
        : CPixelLEDControllerUnordered<ESP32RMT_ClocklessController<XTRA0, FLIP, WAIT_TIME>>(rgb_order),
        mRMTController(pin, t1, t2, t3, FASTLED_RMT_MAX_CHANNELS, FASTLED_RMT_MEM_BLOCKS) {}

    void init(){}

    virtual uint16_t getMaxRefreshRate() const { return 400; }

    // -- Show pixels
    //    This is the main entry point for the controller.
    template<EOrder RGB_ORDER = RGB>
    void showPixelsPolicy(PixelController<RGB_ORDER> & pixels)
    {
        if (FASTLED_RMT_BUILTIN_DRIVER) {
            convertAllPixelData(pixels);
        } else {
            loadPixelData(pixels);
        }

        mRMTController.showPixels();
    }

protected:

    // -- Load pixel data
    //    This method loads all of the pixel data into a separate buffer for use by
    //    by the RMT driver. Copying does two important jobs: it fixes the color
    //    order for the pixels, and it performs the scaling/adjusting ahead of time.
    //    It also packs the bytes into 32 bit chunks with the right bit order.
    template<EOrder RGB_ORDER = RGB>
    void loadPixelData(PixelController<RGB_ORDER> & pixels)
    {
        // -- Make sure the buffer is allocated
        int size_in_bytes = pixels.size() * 3;
        uint8_t * pData = mRMTController.getPixelBuffer(size_in_bytes);

        // -- This might be faster
        while (pixels.has(1)) {
            *pData++ = pixels.loadAndScale0();
            *pData++ = pixels.loadAndScale1();
            *pData++ = pixels.loadAndScale2();
            pixels.advanceData();
            pixels.stepDithering();
        }
    }

    // -- Convert all pixels to RMT pulses
    //    This function is only used when the user chooses to use the
    //    built-in RMT driver, which needs all of the RMT pulses
    //    up-front.
    template<EOrder RGB_ORDER = RGB>
    void convertAllPixelData(PixelController<RGB_ORDER> & pixels)
    {
        // -- Make sure the data buffer is allocated
        mRMTController.initPulseBuffer(pixels.size() * 3);

        // -- Cycle through the R,G, and B values in the right order,
        //    storing the pulses in the big buffer

        uint32_t byteval;
        while (pixels.has(1)) {
            byteval = pixels.loadAndScale0();
            mRMTController.convertByte(byteval);
            byteval = pixels.loadAndScale1();
            mRMTController.convertByte(byteval);
            byteval = pixels.loadAndScale2();
            mRMTController.convertByte(byteval);
            pixels.advanceData();
            pixels.stepDithering();
        }
    }
};

#else  // FASTLED_VERSION > 3007003

/*
    in FastLED versions >3007003 RmtController class constructor arguments has changed,
    need to provide different one wrappers for newer versions
    // FASTLED_VERSION >= 3007007 && FASTLED_VERSION <= 3007008
*/

/* ESP32 RMT clockless controller
  this is a stripped template of ClocklessController from FastLED library that does not include
  gpio number as template parameter but accept it as a class constructor parameter.
  This allowing to create an instance with run-time definable gpio and color order
*/
template <int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ESP32RMT_ClocklessController : public CPixelLEDControllerUnordered<ESP32RMT_ClocklessController<XTRA0, FLIP, WAIT_TIME>>
{
private:

    // -- The actual controller object for ESP32
    RmtController mRMTController;

    // -- Verify that the pin is valid
    // no static checks for run-time defined gpio
    //static_assert(FastPin<DATA_PIN>::validpin(), "Invalid pin specified");

public:
    ESP32RMT_ClocklessController(uint8_t pin, EOrder rgb_order, unsigned t1, unsigned t2, unsigned t3)
        : CPixelLEDControllerUnordered<ESP32RMT_ClocklessController<XTRA0, FLIP, WAIT_TIME>>(rgb_order),
        mRMTController(pin, t1, t2, t3, FASTLED_RMT_MAX_CHANNELS, FASTLED_RMT_BUILTIN_DRIVER) {}

    void init(){}

    virtual uint16_t getMaxRefreshRate() const { return 400; }

    // -- Show pixels
    //    This is the main entry point for the controller.
    template<EOrder RGB_ORDER = RGB>
    void showPixelsPolicy(PixelController<RGB_ORDER> & pixels){
        PixelIterator iterator = pixels.as_iterator(this->getRgbw());
        mRMTController.showPixels(iterator);
    }

};
#endif  // FASTLED_VERSION <= 3007003
#endif  // FASTLED_VERSION <= 3007008



#if FASTLED_VERSION > 3007008
// defined in cpixel_ledcontroller.h

/// Template extension of the CLEDController class
/// in comparision with CPixelLEDController this template does NOT have EOrder template parameter that defines RGB color ordering
/// it's a bit slower due to switch/case for each color type, but in case you do not care much it could be much handy for handling
/// run-time defined color order configurations. The performance impact on esp32 is negligible.
///
/// @tparam showPolicy a class type that implements pixel data output policy
/// @tparam LANES how many parallel lanes of output to write
/// @tparam MASK bitmask for the output lanes
template<typename showPolicy, int LANES=1, uint32_t MASK=0xFFFFFFFF>
class CPixelLEDControllerUnordered : public CLEDController {
    // color order
    const EOrder _rgb_order;

protected:
    /// Send the LED data to the strip
    /// @param pixels the PixelController object for the LED data
    //virtual void showPixels(PixelController<RGB_ORDER,LANES,MASK> & pixels) = 0;
    template<typename T>
    void showPixels(T& pixels){
        showPolicy& policy = static_cast<showPolicy&>(*this);
        policy.showPixelsPolicy(pixels);
    }

    /// Set all the LEDs on the controller to a given color
    /// @param data the CRGB color to set the LEDs to
    /// @param nLeds the number of LEDs to set to this color
    /// @param scale_pre_mixed the RGB scaling of color adjustment + global brightness to apply to each LED (in RGB8 mode).
    virtual void showColor(const CRGB& data, int nLeds, uint8_t brightness) override {
        // CRGB premixed, color_correction;
        // getAdjustmentData(brightness, &premixed, &color_correction);
        // ColorAdjustment color_adjustment = {premixed, color_correction, brightness};
        ColorAdjustment color_adjustment = getAdjustmentData(brightness);

        switch (_rgb_order){
            // this is the most usual type of ws strips around
            case GRB : {
                PixelController<GRB, LANES, MASK> pixels(data, nLeds, color_adjustment, getDither());
                return showPixels(pixels);
            }
            case RBG : {
                PixelController<RBG, LANES, MASK> pixels(data, nLeds, color_adjustment, getDither());
                return showPixels(pixels);
            }
            case GBR : {
                PixelController<GBR, LANES, MASK> pixels(data, nLeds, color_adjustment, getDither());
                return showPixels(pixels);
            }
            case BRG : {
                PixelController<BRG, LANES, MASK> pixels(data, nLeds, color_adjustment, getDither());
                return showPixels(pixels);
            }
            case BGR : {
                PixelController<BGR, LANES, MASK> pixels(data, nLeds, color_adjustment, getDither());
                return showPixels(pixels);
            }
            default:
                // consider it as RGB
                PixelController<RGB, LANES, MASK> pixels(data, nLeds, color_adjustment, getDither());
                showPixels(pixels);
        }
    }

    /// Write the passed in RGB data out to the LEDs managed by this controller
    /// @param data the RGB data to write out to the strip
    /// @param nLeds the number of LEDs being written out
    /// @param scale_pre_mixed the RGB scaling of color adjustment + global brightness to apply to each LED (in RGB8 mode).
    virtual void show(const struct CRGB *data, int nLeds, uint8_t brightness) override {
        ColorAdjustment color_adjustment = getAdjustmentData(brightness);
        // nLeds < 0 implies that we want to show them in reverse
        switch (_rgb_order){
            // this is the most usual type of ws strips around
            case GRB : {
                PixelController<GRB, LANES, MASK> pixels(data, nLeds < 0 ? -nLeds : nLeds, color_adjustment, getDither());
                if(nLeds < 0) pixels.mAdvance = -pixels.mAdvance;
                return showPixels(pixels);
            }
            case RBG : {
                PixelController<RBG, LANES, MASK> pixels(data, nLeds < 0 ? -nLeds : nLeds, color_adjustment, getDither());
                if(nLeds < 0) pixels.mAdvance = -pixels.mAdvance;
                return showPixels(pixels);
            }
            case GBR : {
                PixelController<GBR, LANES, MASK> pixels(data, nLeds < 0 ? -nLeds : nLeds, color_adjustment, getDither());
                if(nLeds < 0) pixels.mAdvance = -pixels.mAdvance;
                return showPixels(pixels);
            }
            case BRG : {
                PixelController<BRG, LANES, MASK> pixels(data, nLeds < 0 ? -nLeds : nLeds, color_adjustment, getDither());
                if(nLeds < 0) pixels.mAdvance = -pixels.mAdvance;
                return showPixels(pixels);
            }
            case BGR : {
                PixelController<BGR, LANES, MASK> pixels(data, nLeds < 0 ? -nLeds : nLeds, color_adjustment, getDither());
                if(nLeds < 0) pixels.mAdvance = -pixels.mAdvance;
                return showPixels(pixels);
            }
            default: {
                // consider it as RGB
                PixelController<RGB, LANES, MASK> pixels(data, nLeds < 0 ? -nLeds : nLeds, color_adjustment, getDither());
                if(nLeds < 0) pixels.mAdvance = -pixels.mAdvance;
                showPixels(pixels);
            }
        }
    }

public:
    CPixelLEDControllerUnordered(EOrder rgb_order = GRB) : CLEDController(), _rgb_order(rgb_order) {}

    /// Get the number of lanes of the Controller
    /// @returns LANES from template
    int lanes() const { return LANES; }
};

/* ESP32 RMT clockless controller
  this is a stripped template of ClocklessController from FastLED library that does not include
  gpio number as template parameter but accept it as a class constructor parameter.
  This allowing to create an instance with run-time definable gpio and color order
*/
template <int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ESP32RMT_ClocklessController : public CPixelLEDControllerUnordered<ESP32RMT_ClocklessController<XTRA0, FLIP, WAIT_TIME>>
{
private:

    // -- The actual controller object for ESP32
    RmtController5 mRMTController;

    // -- Verify that the pin is valid
    // no static checks for run-time defined gpio
    //static_assert(FastPin<DATA_PIN>::validpin(), "Invalid pin specified");

public:
    ESP32RMT_ClocklessController(uint8_t pin, EOrder rgb_order, unsigned t1, unsigned t2, unsigned t3)
        : CPixelLEDControllerUnordered<ESP32RMT_ClocklessController<XTRA0, FLIP, WAIT_TIME>>(rgb_order),
        mRMTController(pin, t1, t2, t3) {}

    void init(){}

    virtual uint16_t getMaxRefreshRate() const { return 400; }

    // -- Show pixels
    //    This is the main entry point for the controller.
    template<EOrder RGB_ORDER = RGB>
    void showPixelsPolicy(PixelController<RGB_ORDER> & pixels)
    {
        //PixelIterator iterator = pixels.as_iterator(this->getRgbw());
        mRMTController.showPixels();
    }

};
#endif  // FASTLED_VERSION > 3007008




// WS2812 - 250ns, 625ns, 375ns
// a reduced template for WS2812Controller800Khz that instatiates ESP32's RMT clockless controller with run-time defined gpio number and color order
class ESP32RMT_WS2812Controller800Khz : public ESP32RMT_ClocklessController<> {
public:
    ESP32RMT_WS2812Controller800Khz(uint8_t pin, EOrder rgb_order) : ESP32RMT_ClocklessController(pin, rgb_order, C_NS(250), C_NS(625), C_NS(375)) {}
};

/*
 WS2812 controller class @ 800 KHz.
 with RTM run-time defined gpio and color order
*/
class ESP32RMT_WS2812B : public ESP32RMT_WS2812Controller800Khz {
public:
    ESP32RMT_WS2812B(uint8_t pin, EOrder rgb_order) : ESP32RMT_WS2812Controller800Khz(pin, rgb_order){}
};
#endif  //ifdef ESP32
