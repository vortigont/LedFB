/*
    This file is a part of LedFB library project
    https://github.com/vortigont/LedFB

    Copyright © 2023-2024 Emil Muratov (vortigont)
*/

#pragma once
#include "ledfb.hpp"
#include "w2812-rmt.hpp"
#ifdef LEDFB_WITH_HUB75_I2S
  #include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#endif

#ifdef ESP32
/**
 * @brief Dispaly engine based on ESP32 RMT backend for ws2818 LED strips  
 * 
 * @tparam RGB_ORDER 
 */
class ESP32RMTDisplayEngine : public DisplayEngine<CRGB> {
    // which buffer is active - true->canvas, false->backbuff
    bool _active_buff{true};
    std::shared_ptr<CLedCDB>  canvas;      // canvas buffer where background data is stored
    std::shared_ptr<CLedCDB>  backbuff;    // back buffer, where we will mix data with overlay before sending to LEDs
    //std::weak_ptr<CLedCDB>    overlay;     // overlay buffer weak pointer

    // FastLED controller
    CLEDController *cled = nullptr;
    // led strip driver
    ESP32RMT_WS2812B *wsstrip;

    /**
     * @brief show buffer content on display
     * it will draw a canvas content (or render an overlay over it if necessary)
     * 
     */
    void engine_show() override;

public:
    /**
     * @brief Construct a new Overlay Engine object
     * 
     * @param gpio - gpio to bind ESP32 RMT engine
     */
    ESP32RMTDisplayEngine(int gpio, EOrder rgb_order);

    /**
     * @brief Construct a new Overlay Engine object
     * 
     * @param gpio - gpio to bind
     * @param buffsize - CLED buffer object
     */
    ESP32RMTDisplayEngine(int gpio, EOrder rgb_order, std::shared_ptr<CLedCDB> buffer);

    /**
     * @brief Construct a new Overlay Engine object
     * 
     * @param gpio - gpio to bind
     * @param buffsize - desired LED buffer size
     */
    ESP32RMTDisplayEngine(int gpio, EOrder rgb_order, size_t buffsize) : ESP32RMTDisplayEngine(gpio, rgb_order, std::make_shared<CLedCDB>(buffsize)) {};

    /**
     * @brief take a buffer pointer and attach it to ESP32 RMT engine
     * 
     * @param fb - a pointer to CLedC buffer object
     * @return true - on success
     * @return false - if canvas has been attached attached already
     */
    bool attachCanvas(std::shared_ptr<CLedCDB> &fb);

//    std::shared_ptr<PixelDataBuffer<CRGB>> getCanvas() override { return canvas; }

    /**
     * @brief Get a pointer to Overlay buffer
     * Note: consumer MUST release a pointer once overlay operations is no longer needed
     * 
     * @return std::shared_ptr<PixelDataBuffer<CRGB>> 
     */
//    std::shared_ptr<PixelDataBuffer<CRGB>> getOverlay() override;

    /**
     * @brief wipe buffers and draw a blank screen
     * 
     */
    void clear() override;

    /**
     * @brief change display brightness
     * if supported by engine
     * 
     * @param b - target brightness
     * @return uint8_t - current brightness level
     */
    uint8_t brightness(uint8_t b) override;

    /**
     * @brief activate double buffer
     * 
     * @param active 
     */
    void doubleBuffer(bool active) override;

    bool doubleBuffer() const override { return backbuff.use_count(); }

    /**
     * @brief swap content of front and back buffer
     * 
     */
    void flipBuffer() override;

    /**
     * @brief switch rendering between back and front buffer
     * 
     * @return bool - true if acive buffer is front buffer, false if active buffer is back buffer
     */
    bool toggleBuffer() override;

    std::shared_ptr<PixelDataBuffer<CRGB>> getBuffer() override { return canvas; }

    std::shared_ptr<PixelDataBuffer<CRGB>> getBackBuffer() override { return backbuff.use_count() ? backbuff : canvas; }

    std::shared_ptr<PixelDataBuffer<CRGB>> getActiveBuffer() override { return _active_buff ? canvas : backbuff; };

    /**
     * @brief copy back buffer to front buffer
     * 
     */
    void copyBack2Front() override;

    /**
     * @brief copy front buffer to back buffer
     * 
     */
    void copyFront2Back() override;

private:
    /**
     * @brief apply overlay to canvas
     * pixels with _transparent_color will be skipped
     */
    void _ovr_overlap();

    /**
     * @brief switch active output to a back buffe
     * used in case if canvas is marked as persistent and should not be changed with overlay data
     * 
     */
    void _switch_to_bb();
};
#endif //ifdef ESP32



#ifdef LEDFB_WITH_HUB75_I2S

/**
 * @brief CRGB buffer backed with esp32 HUB75_i2s_dma lib
 * i2s DMA has it's own DMA buffer to output data, but it's a one-way buffer
 * you can write there, but can't read. So for some applications you are forced to have external buffer for pixel data anyway
 * this class could be a good option to have a bounded buffer
 */
class HUB75PanelDB : public PixelDataBuffer<CRGB> {

public:
    // I2S DMA buffer object
    MatrixPanel_I2S_DMA hub75;

    /**
     * @brief Construct a new HUB75PanelDB object
     * will create a new CRGB buffer and bind it with MatrixPanel_I2S_DMA object
     * 
     * @param config HUB75_I2S_CFG struct
     */
    HUB75PanelDB(const HUB75_I2S_CFG &config) : PixelDataBuffer(config.mx_width*config.mx_height), hub75(config) { hub75.begin(); }

    /**
     * @brief write data buffer content to HUB75 DMA buffer
     * 
     */
    void show();

    /**
     * @brief a more faster way to clear buffer+DMA then clear()+show()
     * 
     */
    void wipe(){ clear(); hub75.clearScreen(); };

    /**
     * @brief HUB75Panel is not supported (yet)
     */
    bool resize(size_t s) override { return false; };
};


/**
 * @brief Display engine based in HUB75 RGB panel
 * 
 */
class ESP32HUB75_DisplayEngine : public DisplayEngine<CRGB> {
    MatrixPanel_I2S_DMA hub75;
    // which buffer is active - true->canvas, false->backbuff
    bool _active_buff{true};
    std::shared_ptr<PixelDataBuffer<CRGB>>  canvas;      // canvas buffer where background data is stored
    std::shared_ptr<PixelDataBuffer<CRGB>>  backbuff;    // back buffer weak pointer

    /**
     * @brief show buffer content on display
     * it will draw a canvas content (or render an overlay over it if necessary)
     * 
     */
    void engine_show() override;

public:
    /**
     * @brief Construct a new Overlay Engine object
     * 
     * @param gpio - gpio to bind ESP32 RMT engine
     */
    ESP32HUB75_DisplayEngine(const HUB75_I2S_CFG &config);

    /**
     * @brief Construct a new Overlay Engine object
     * 
     */
    ESP32HUB75_DisplayEngine(std::shared_ptr<HUB75PanelDB> canvas);

    //std::shared_ptr<PixelDataBuffer<CRGB>> getCanvas() override { return canvas; }
    //std::shared_ptr<PixelDataBuffer<CRGB>> getCanvas() override { return canvas; }

    /**
     * @brief Get a pointer to Overlay buffer
     * Note: consumer MUST release a pointer once overlay operations is no longer needed
     * 
     * @return std::shared_ptr<PixelDataBuffer<CRGB>> 
     */
    //std::shared_ptr<PixelDataBuffer<CRGB>> getOverlay() override;

    /**
     * @brief wipe buffers and draw a blank screen
     * 
     */
    void clear() override;

    /**
     * @brief change display brightness
     * if supported by engine
     * 
     * @param b - target brightness
     * @return uint8_t - current brightness level
     */
    uint8_t brightness(uint8_t b) override { hub75.setBrightness(b); return b; };

    /**
     * @brief activate double buffer
     * 
     * @param active 
     */
    void doubleBuffer(bool active) override;

    bool doubleBuffer() const override { return backbuff.use_count(); }

    /**
     * @brief swap content of front and back buffer
     * 
     */
    void flipBuffer() override;

    /**
     * @brief switch rendering between back and front buffer
     * 
     * @return bool - true if acive buffer is front buffer, false if active buffer is back buffer
     */
    bool toggleBuffer() override;

    std::shared_ptr<PixelDataBuffer<CRGB>> getBuffer() override { return canvas; }

    std::shared_ptr<PixelDataBuffer<CRGB>> getBackBuffer() override { return backbuff.use_count() ? backbuff : canvas; }

    std::shared_ptr<PixelDataBuffer<CRGB>> getActiveBuffer() override { return _active_buff ? canvas : backbuff; };

    /**
     * @brief copy back buffer to front buffer
     * 
     */
    void copyBack2Front() override;

    /**
     * @brief copy front buffer to back buffer
     * 
     */
    void copyFront2Back() override;
};
#endif  // LEDFB_WITH_HUB75_I2S
