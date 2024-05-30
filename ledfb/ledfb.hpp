/*
    This file is a part of LedFB library project
    https://github.com/vortigont/LedFB

    Copyright © 2023 Emil Muratov (vortigont)
*/

#pragma once
#include <vector>
#include <memory>
#include <variant>
#include <functional>
#include "w2812-rmt.hpp"
#include <Adafruit_GFX.h>
#ifdef LEDFB_WITH_HUB75_I2S
  #include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#endif

/**
 * @brief Base class with CRGB data storage that acts as a pixel buffer storage
 * it provides basic operations with pixel data with no any backend engine to display data
 * 
 */
template <class COLOR_TYPE = CRGB>
class PixelDataBuffer {

protected:
    std::vector<COLOR_TYPE> fb;     // container that holds pixel data

public:
    PixelDataBuffer(size_t size) : fb(size) {}

    /**
     * @brief Copy-Construct a new Led FB object
     *  it also does NOT copy persistence flag
     * @param rhs 
     */
    PixelDataBuffer(PixelDataBuffer const & rhs) : fb(rhs.fb) {};

    /**
     * @brief Copy-assign a new Led FB object
     * @param rhs 
     */
    PixelDataBuffer& operator=(PixelDataBuffer<COLOR_TYPE> const & rhs);

    // move semantics
    /**
     * @brief Move Construct a new PixelDataBuffer object
     * constructor will steal a cled pointer from a rhs object
     * @param rhs 
     */
    PixelDataBuffer(PixelDataBuffer&& rhs) noexcept : fb(std::move(rhs.fb)){};

    /**
     * @brief Move assignment operator
     * @param rhs 
     * @return PixelDataBuffer& 
     */
    PixelDataBuffer& operator=(PixelDataBuffer&& rhs);

    // d-tor
    virtual ~PixelDataBuffer() = default;

    /**
     * @brief return size of FB in pixels
     * 
     */
    virtual size_t size() const { return fb.size(); }

    /**
     * @brief zero-copy swap CRGB data within two framebuffers
     * config struct is also swapped between object instances
     * @param rhs - object to swap with
     */
    virtual void swap(PixelDataBuffer& rhs){ std::swap(fb, rhs.fb); };

    // get direct access to FB array
    std::vector<COLOR_TYPE> &data(){ return fb; }


    /**
     * @brief resize LED buffer to specified size
     * content will be lost on resize
     * 
     * @param s new number of pixels
     */
    virtual bool resize(size_t s);

    /***    access methods      ***/

    /**
     * @brief access CRGB pixel at specified position
     * in case of oob index supplied returns a reference to blackhole
     * @param i offset index
     * @return CRGB& 
     */
    COLOR_TYPE& at(size_t i);

    // access pixel at coordinates x:y (obsolete)
    //CRGB& pixel(unsigned x, unsigned y){ return at(x,y); };

    /**
     * @brief access CRGB pixel at specified position
     * in case of oob index supplied returns a reference to blackhole
     * @param i offset index
     * @return CRGB& 
     */
    COLOR_TYPE& operator[](size_t i){ return at(i); };

    /*
        iterators
        TODO: need proper declaration for this
    */
    typename std::vector<COLOR_TYPE>::iterator begin(){ return fb.begin(); };
    typename std::vector<COLOR_TYPE>::iterator end(){ return fb.end(); };


    /***    color operations      ***/

    /**
     * @brief fill the buffer with solid color
     * 
     */
    virtual void fill(COLOR_TYPE color);

    /**
     * @brief clear buffer to black
     * 
     */
    virtual void clear();

    // stub pixel that is mapped to either nonexistent buffer access or blackholed CLedController mapping
    static COLOR_TYPE stub_pixel;
};

// static definition
template <class COLOR_TYPE> COLOR_TYPE PixelDataBuffer<COLOR_TYPE>::stub_pixel;

/**
 * @brief CledController Data Buffer - class with CRGB data storage (possibly) attached to FastLED's CLEDController
 * and maintaining bound on move/copy/swap operations
 * 
 */
class CLedCDB : public PixelDataBuffer<CRGB> {
    /**
     * @brief a poor-man's mutex
     * points to the instance that owns global FastLED's buffer reference
     */
    CLEDController *cled = nullptr;

    /**
     * @brief if this buffer is bound to CLED controller,
     * than reset it's pointer to this buffer's data array
     * required to call on iterator invalidation or move semantics
     */
    void _reset_cled(){ if (cled) {cled->setLeds(fb.data(), fb.size());} };

public:
    // c-tor
    CLedCDB(size_t size) : PixelDataBuffer(size) {}

    /**
     * @brief Copy-assign a new Led FB object
     *  operator only copies data, it does NOT copy/move cled assignment (if any)
     * @param rhs 
     */
    //CLedCDB& operator=(CLedCDB const & rhs);

    // move semantics
    /**
     * @brief Move Construct a new CLedCDB object
     * constructor will steal a cled pointer from a rhs object
     * @param rhs - object to move from
     */
    CLedCDB(CLedCDB&& rhs) noexcept;

    /**
     * @brief Move assignment operator
     * will steal a cled pointer from a rhs object
     * @param rhs 
     * @return CLedCDB& 
     */
    CLedCDB& operator=(CLedCDB&& rhs);

    // d-tor
    ~CLedCDB();

    /**
     * @brief zero-copy swap CRGB data within two framebuffers
     * config struct is also swapped between object instances
     * local CLED binding is updated
     * @param rhs 
     */
    void swap(PixelDataBuffer& rhs) override;

    /**
     * @brief zero-copy swap CRGB data within two framebuffers
     * config struct is also swapped between object instances
     * CLED binding, if any, is updated to point to a newly swapped data both in local and rhs objects
     * @param rhs 
     */
    void swap(CLedCDB& rhs);

    /**
     * @brief bind this framebuffer to a CLEDController instance
     * 
     * @param pLed instanle of the CLEDController
     * @return true if bind success
     * @return false if this instance is already bound to CLEDController or parameter pointer is null
     */
    bool bind(CLEDController *pLed);

    /**
     * @brief swap bindings to CLED controller(s) with another PixelDataBuffer instance
     * if only one of the istances has a bound, then another instance steals it
     * 
     * @param rhs other instance that must have active CLED binding
     */
    void rebind(CLedCDB &rhs);

    /**
     * @brief returns 'true' if buffer is currently bound to CLEDController instance
     */
    bool isBound() const { return cled; }

    /**
     * @brief resize LED buffer to specified size
     * content will be lost on resize
     * 
     * @param s new number of pixels
     */
    bool resize(size_t s) override;

    /**
     * @brief display buffer data to LED strip
     * 
     */
    void show(){ FastLED.show(); }
};

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
#endif  // LEDFB_WITH_HUB75_I2S



// coordinate transformation callback prototype
using transpose_t = std::function<size_t(unsigned w, unsigned h, unsigned x, unsigned y)>;

// a default (x,y) 2D mapper to 1-d vector index
static size_t map_2d(unsigned w, unsigned h, unsigned x, unsigned y) { return y*w+x; };


/**
 * @brief basic 2D buffer
 * provides generic abstraction for 2D topology
 * remaps x,y coordinates to linear pixel vector
 * basic class maps a simple row by row 2D buffer
 */
template <class COLOR_TYPE = CRGB>
class LedFB {

protected:
    // buffer width, height
    uint16_t _w, _h;
    // pixel buffer storage
    std::shared_ptr<PixelDataBuffer<COLOR_TYPE>> buffer;
    // coordinate to buffer index mapper callback
    transpose_t _xymap = map_2d;

public:
    // c-tor
    /**
     * @brief Construct a new LedFB object
     * will spawn a new data buffer with requested dimensions
     * @param w - width
     * @param h - heigh
     */
    LedFB(uint16_t w, uint16_t h);

    /**
     * @brief Construct a new LedFB object
     * will spawn a new data buffer with requested dimensions
     * @param w - width
     * @param h - heigh
     * @param fb - preallocated buffer storage
     */
    LedFB(uint16_t w, uint16_t h, std::shared_ptr<PixelDataBuffer<COLOR_TYPE>> fb);

    /**
     * @brief Copy Construct a new LedFB object
     * A new instance will inherit a SHARED underlying data buffer member
     * @param rhs source object
     */
    LedFB(LedFB const & rhs);

    virtual ~LedFB() = default;

    /**
     * @brief Copy-assign not implemented (yet)
     * @param rhs 
     */
    LedFB& operator=(LedFB const & rhs) = delete;

    // DIMENSIONS

    // get configured matrix width
    uint16_t w() const {return _w;}
    // get configured matrix height
    uint16_t h() const {return _h;}

    // get size in pixels
    size_t size() const { return buffer->size(); }

    // return length of the longest side
    virtual uint16_t maxDim() const { return _w>_h ? _w : _h; }
    // return length of the shortest side
    virtual uint16_t minDim() const { return _w<_h ? _w : _h; }

    virtual uint16_t maxHeightIndex() const { return h()-1; }
    virtual uint16_t maxWidthIndex()  const { return w()-1; }

    // Topology transformation

    /**
     * @brief Set topology Remap Function
     * it will remap (x,y) coordinate into underlaying buffer vector index
     * could be used for various layouts of led canvases, tiles, stripes, etc...
     * affect access methods like at(), drawPixel(), etc...
     * 
     * @param mapper 
     * @return * assign 
     */
    void setRemapFunction(transpose_t mapper){ _xymap = mapper; };


    // DATA BUFFER OPERATIONS

    /**
     * @brief resize member data buffer to the specified size
     * Note: data buffer might NOT support resize operation, in this case resize does() nothing
     * @param w - new width
     * @param h - new height
     */
    virtual bool resize(uint16_t w, uint16_t h);

    //virtual void switchBuffer(std::shared_ptr<PixelDataBuffer> buff);

    // Pixel data access

    /**
     * @brief access pixel at coordinates x:y
     * if oob coordinates supplied returns blackhole element
     * @param x coordinate starting from top left corner
     * @param y coordinate starting from top left corner
     */
    COLOR_TYPE& at(int16_t x, int16_t y);

    /**
     * @brief access pixel at index
     * 
     * @param idx 
     * @return CRGB& 
     */
    COLOR_TYPE& at(size_t idx){ return buffer->at(idx); };

    /*
        data buffer iterators
        TODO: need proper declaration for this
    */
    typename std::vector<COLOR_TYPE>::iterator begin(){ return buffer->begin(); };
    typename std::vector<COLOR_TYPE>::iterator end(){ return buffer->end(); };


    // FastLED buffer-wide color functions (here just a wrappers, but could be overriden in derived classes)

    /**
     * @brief apply FastLED fadeToBlackBy() func to buffer
     * 
     * @param v 
     */
    void fade(uint8_t v);

    /**
     * @brief apply FastLED nscale8() func to buffer
     * i.e. dim whole buffer to black
     * @param v 
     */
    void dim(uint8_t v);

    /**
     * @brief fill the buffer with solid color
     * 
     */
    void fill(COLOR_TYPE color){ buffer->fill(color); };

    /**
     * @brief clear buffer to black
     * 
     */
    void clear(){ buffer->clear(); };

};

/**
 * @brief Coordinate transformation class, implements a rectangular canvas made from a single piece of a LED Stripe
 * possible orientation and transformations:
 * - V/H mirroring
 * - snake/zigzag rows
 * - transpose rows vs columns, i.e. stripe goes horisontaly (default) or vertically
 * 
 */
class LedStripe {
protected:
    bool _snake;             // matrix type 1:snake( zigzag), 0:parallel
    bool _vertical;          // strip direction: 0 - horizontal, 1 - vertical
    bool _vmirror;           // vertical flip
    bool _hmirror;           // horizontal flip

public:
    /**
     * @brief Construct a new Led Stripe object
     * from a set of tiles and it's orientation
     * 
     * @param t_snake - snake/zigzag pixel chaining
     * @param t_vertical - if true - pixels are chained vertically
     * @param t_vm - if true - pixels colums are inverted (default pixels goes from up to downward, if true, pixels goes from down to upwards)
     * @param t_hm - if true - pixels rows are inverted (default pixels goes from left to right, if true, pixels goes from left to right)
     */
    LedStripe(bool snake = true, bool _vertical = false, bool vm=false, bool hm=false) : _snake(snake), _vertical(_vertical), _vmirror(vm), _hmirror(hm) {};
    virtual ~LedStripe() = default;

    // getters
    bool snake()   const {return _snake;}
    bool vertical()const {return _vertical;}
    bool vmirror() const {return _vmirror;}
    bool hmirror() const {return _hmirror;}

    // setters
    void snake(bool m) {_snake=m;}
    void vertical(bool m) {_vertical=m;}
    void vmirror(bool m){_vmirror=m;}
    void hmirror(bool m){_hmirror=m;}

    void setLayout(bool snake, bool vert, bool vm, bool hm){ _snake=snake; _vertical=vert; _vmirror=vm; _hmirror=hm;  };

    /**
     * @brief Transpose pixel 2D coordinates (x,y) into framebuffer's 1D array index
     * 0's index is at top-left corner, X axis goes to the 'right', Y axis goes 'down'
     * it calculates array index based on matrix orientation and configuration
     * no checking performed for supplied coordinates to be out of bound of pixel buffer!
     * for signed negative arguments the behaviour is undefined
     * @param x 
     * @param y 
     * @return size_t 
     */
    virtual size_t transpose(unsigned w, unsigned h, unsigned x, unsigned y) const;
};

/**
 * @brief Coordinate transformation class, implements tiled rectangular canvas made from chained blocks of a LedStripe objects
 * i.e. a chained matrixes or panels
 * orientation and transformation rules to a chain of tiles could be applied
 * - V/H mirroring
 * - snake/zigzag rows
 * - transpose rows vs columns, i.e. stripe goes horisontaly (default) or vertically
 * NOTE: all tiles in canvas MUST have same dimensions and orientation
 */
class LedTiles : public LedStripe {
protected:
    unsigned _tile_w, _tile_h, _tile_wcnt, _tile_hcnt;  // width, height and num of tiles
    size_t tiled_transpose(unsigned w, unsigned h, unsigned x, unsigned y) const;

public:
    LedStripe tileLayout;          // tiles layout, i.e. how tiles are chained

    /**
     * @brief Construct a new Led Tiles object
     * from a set of tiles and it's orientation
     * 
     * @param tile_width - single tile's width
     * @param tile_height - single tile's height
     * @param tile_wcnt - number of tiles in a row
     * @param tile_hcnt - number of tiles in col
     * @param t_snake - snake/zigzag chaining
     * @param t_vertical - if true - tiles are chained vertically
     * @param t_vm - if true - tile colums are inverted (default tiles goes from up to downward, if true, tiles goes from down to upwards)
     * @param t_hm - if true - tile rows are inverted (default tiles goes from left to right, if true, tiles goes from left to right)
     */
    LedTiles(unsigned tile_width = 16, unsigned tile_height = 16, unsigned tile_wcnt = 1, unsigned tile_hcnt = 1, bool t_snake = false, bool t_vertical = false, bool t_vm=false, bool t_hm=false) :
        _tile_w(tile_width), _tile_h(tile_height), _tile_wcnt(tile_wcnt), _tile_hcnt(tile_hcnt),
        tileLayout(t_snake, t_vertical, t_vm, t_hm) {};

    virtual ~LedTiles() = default;

    // getters
    unsigned canvas_w() const {return _tile_w*_tile_wcnt;}
    unsigned canvas_h() const {return _tile_h*_tile_hcnt;}
    unsigned tile_w() const {return _tile_w;}
    unsigned tile_h() const {return _tile_h;}
    unsigned tile_wcnt() const {return _tile_wcnt;}
    unsigned tile_hcnt() const {return _tile_hcnt;}

    // setters
    void tile_w(unsigned m) { _tile_w=m; }
    void tile_h(unsigned m) {_tile_h=m;}
    void tile_wcnt(unsigned m) {_tile_wcnt=m;}
    void tile_hcnt(unsigned m) {_tile_hcnt=m;}

    // Adjust tiles size and canvas dimensions
    void setTileDimensions(unsigned w, unsigned h, unsigned wcnt, unsigned hcnt){ _tile_w=w; _tile_h=h; _tile_wcnt=wcnt; _tile_hcnt=hcnt; }

    /**
     * @brief  transpose x,y coordinates of a pixel in a rectangular canvas
     * into a 1D vector index mapping to chained tiles of matrixes
     * 
     * @param w - full canvas width
     * @param h - full canvas height
     * @param x - pixel's x
     * @param y - pixel's y
     * @return size_t - pixel's index in a 1D vector
     */
    virtual size_t transpose(unsigned w, unsigned h, unsigned x, unsigned y) const override;
};


/**
 * @brief abstract overlay engine
 * it works as a renderer for canvas, creating/mixing overlay/back buffer with canvas
 */
class DisplayEngine {

public:
    // virtual d-tor
    virtual ~DisplayEngine(){};

    /**
     * @brief Get a reference to canvas buffer
     * 
     * @return PixelDataBuffer* 
     */
    virtual std::shared_ptr<PixelDataBuffer<CRGB>> getCanvas() = 0;

    virtual std::shared_ptr<PixelDataBuffer<CRGB>> getOverlay() = 0;

    /**
     * @brief protect canvs buffer from altering by overlay mixing
     * i.e. canvas buffer is used as a persistent storage
     * in that case Engine will try to use back buffer for overlay mixing (if implemented) 
     * @param v - true of false
     */
    void canvasProtect(bool v){ _canvas_protect = v; };

    /**
     * @brief show buffer content on display
     * it will draw a canvas content (or render an overlay over it if necessary)
     * 
     */
    virtual void show(){};

    /**
     * @brief wipe buffers and draw a blank screen
     * 
     */
    virtual void clear(){};

    /**
     * @brief change display brightness
     * if supported by engine
     * 
     * @param b - target brightness
     * @return uint8_t - current brightness level
     */
    virtual uint8_t brightness(uint8_t b){ return 0; };

protected:
    CRGB _transparent_color = CRGB::Black;

    /**
     * @brief flag that marks canvas buffer as persistent that should NOT be changed
     * by overlay mixing operations. In that case overlay is applied on top of a canvas copy in back buffer (if possible)
     * 
     */
    bool _canvas_protect = false;
};


#ifdef ESP32
/**
 * @brief Dispaly engine based on ESP32 RMT backend for ws2818 LED strips  
 * 
 * @tparam RGB_ORDER 
 */
class ESP32RMTDisplayEngine : public DisplayEngine {
protected:
    std::shared_ptr<CLedCDB>  canvas;      // canvas buffer where background data is stored
    std::unique_ptr<CLedCDB>  backbuff;    // back buffer, where we will mix data with overlay before sending to LEDs
    std::weak_ptr<CLedCDB>    overlay;     // overlay buffer weak pointer

    // FastLED controller
    CLEDController *cled = nullptr;
    // led strip driver
    ESP32RMT_WS2812B *wsstrip;

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

    std::shared_ptr<PixelDataBuffer<CRGB>> getCanvas() override { return canvas; }

    /**
     * @brief Get a pointer to Overlay buffer
     * Note: consumer MUST release a pointer once overlay operations is no longer needed
     * 
     * @return std::shared_ptr<PixelDataBuffer<CRGB>> 
     */
    std::shared_ptr<PixelDataBuffer<CRGB>> getOverlay() override;

    /**
     * @brief show buffer content on display
     * it will draw a canvas content (or render an overlay over it if necessary)
     * 
     */
    void show() override;

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
 * @brief Display engine based in HUB75 RGB panel
 * 
 */
class ESP32HUB75_DisplayEngine : public DisplayEngine {
protected:
    std::shared_ptr<HUB75PanelDB>  canvas;      // canvas buffer where background data is stored
    std::weak_ptr<PixelDataBuffer<CRGB>>    overlay;     // overlay buffer weak pointer

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

    std::shared_ptr<PixelDataBuffer<CRGB>> getCanvas() override { return canvas; }

    /**
     * @brief Get a pointer to Overlay buffer
     * Note: consumer MUST release a pointer once overlay operations is no longer needed
     * 
     * @return std::shared_ptr<PixelDataBuffer<CRGB>> 
     */
    std::shared_ptr<PixelDataBuffer<CRGB>> getOverlay() override;

    /**
     * @brief show buffer content on display
     * it will draw a canvas content (or render an overlay over it if necessary)
     * 
     */
    void show() override;

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
    uint8_t brightness(uint8_t b) override { canvas->hub75.setBrightness(b); return b; };

};
#endif  // LEDFB_WITH_HUB75_I2S





// overload pattern and deduction guide. Lambdas provide call operator
template<class... Ts> struct Overload : Ts... { using Ts::operator()...; };
template<class... Ts> Overload(Ts...) -> Overload<Ts...>;

/**
 * @brief GFX class for LedFB
 * it provides Adafruit API for uderlaying buffer with either CRGB or uint16 color container
 * 
 */
class LedFB_GFX : public Adafruit_GFX {

    void _drawPixelCRGB( LedFB<CRGB> *b, int16_t x, int16_t y, CRGB c){ b->at(x,y) = c; };
    void _drawPixelCRGB( LedFB<uint16_t> *b, int16_t x, int16_t y, CRGB c){ b->at(x,y) = colorCRGBto16(c); };

    void _drawPixelC16( LedFB<CRGB> *b, int16_t x, int16_t y, uint16_t c){ b->at(x,y) = color16toCRGB(c); };
    void _drawPixelC16( LedFB<uint16_t> *b, int16_t x, int16_t y, uint16_t c){ b->at(x,y) = c; };

    void _fillScreenCRGB(LedFB<CRGB> *b, CRGB c){ b->fill(c); };
    void _fillScreenCRGB(LedFB<uint16_t> *b, CRGB c){ b->fill(colorCRGBto16(c)); };

    void _fillScreenC16(LedFB<CRGB> *b, uint16_t c){ b->fill(color16toCRGB(c)); };
    void _fillScreenC16(LedFB<uint16_t> *b, uint16_t c){ b->fill(c); };

/*
    template<typename V, typename X, typename Y, typename C>
    void _visit_drawPixelCRGB(const V& variant, X x, Y y, C color){
        std::visit( Overload{ [&x, &y, &color](const auto& variant_item) { _drawPixelCRGB(variant_item, x, y, color); }, }, variant);
    };
*/

protected:
    // LedFB container variant
    std::variant< std::shared_ptr< LedFB<CRGB> >, std::shared_ptr< LedFB<uint16_t> >  > _fb;

public:
    /**
     * @brief Construct a new LedFB_GFX object from a LedFB<CRGB> 24 bit color
     * 
     * @param buff - a shared pointer to the LedFB object
     */
    LedFB_GFX(std::shared_ptr< LedFB<CRGB> > buff) : Adafruit_GFX(buff->w(), buff->h()), _fb(buff) {}

    /**
     * @brief Construct a new LedFB_GFX object from a LedFB<uint16_t> 16 bit color
     * 
     * @param buff - a shared pointer to the LedFB object
     */
    LedFB_GFX(std::shared_ptr< LedFB<uint16_t> > buff) : Adafruit_GFX(buff->w(), buff->h()), _fb(buff) {}

    virtual ~LedFB_GFX() = default;

    // Adafruit overrides
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void fillScreen(uint16_t color) override;

    // Adafruit-like methods for CRGB
    void drawPixel(int16_t x, int16_t y, CRGB color);
    void fillScreen(CRGB color);

    // Color conversion
    static CRGB color16toCRGB(uint16_t c){ return CRGB( ((c>>11 & 0x1f) * 527 + 23) >> 6, ((c>>5 & 0xfc) * 259 + 33) >> 6, ((c&0x1f) * 527 + 23) >> 6); }
    static uint16_t colorCRGBto16(CRGB c){ return c.r >> 3 << 11 | c.g >> 2 << 5 | c.b >> 3; }


    // Additional methods
};








//  *** TEMPLATES IMPLEMENTATION FOLLOWS *** //

// copy via assignment
template <class COLOR_TYPE>
PixelDataBuffer<COLOR_TYPE>& PixelDataBuffer<COLOR_TYPE>::operator=(PixelDataBuffer<COLOR_TYPE> const& rhs){
    fb = rhs.fb;
    return *this;
}

// move assignment
template <class COLOR_TYPE>
PixelDataBuffer<COLOR_TYPE>& PixelDataBuffer<COLOR_TYPE>::operator=(PixelDataBuffer<COLOR_TYPE>&& rhs){
    fb = std::move(rhs.fb);
    return *this;
}

template <class COLOR_TYPE>
COLOR_TYPE& PixelDataBuffer<COLOR_TYPE>::at(size_t i){ return i < fb.size() ? fb.at(i) : stub_pixel; };      // blackhole is only of type CRGB, need some other specialisations

template <class COLOR_TYPE>
void PixelDataBuffer<COLOR_TYPE>::fill(COLOR_TYPE color){ fb.assign(fb.size(), color); };

template <class COLOR_TYPE>
void PixelDataBuffer<COLOR_TYPE>::clear(){ fill(COLOR_TYPE()); };

template <class COLOR_TYPE>
bool PixelDataBuffer<COLOR_TYPE>::resize(size_t s){
    fb.resize(s);
    clear();
    return fb.size() == s;
};


template <class COLOR_TYPE>
LedFB<COLOR_TYPE>::LedFB(uint16_t w, uint16_t h) : _w(w), _h(h), _xymap(map_2d) {
    buffer = std::make_shared<PixelDataBuffer<COLOR_TYPE>>(PixelDataBuffer<COLOR_TYPE>(w*h));
}

template <class COLOR_TYPE>
LedFB<COLOR_TYPE>::LedFB(uint16_t w, uint16_t h, std::shared_ptr<PixelDataBuffer<COLOR_TYPE>> fb): _w(w), _h(h), buffer(fb), _xymap(map_2d) {
    // a safety check if supplied buffer and dimentions are matching
    if (buffer->size() != w*h)   buffer->resize(w*h);
};

template <class COLOR_TYPE>
LedFB<COLOR_TYPE>::LedFB(LedFB<COLOR_TYPE> const & rhs) : _w(rhs._w), _h(rhs._h), _xymap(map_2d) {
    buffer = rhs.buffer;
    // deep copy
    //buffer = std::make_shared<PixelDataBuffer>(*rhs.buffer.get());
}

template <class COLOR_TYPE>
COLOR_TYPE& LedFB<COLOR_TYPE>::at(int16_t x, int16_t y){
    // since 2D to vector mapping depends on width, need to check if it's not out of bounds
    // otherwise it could possibly be mapped into next y row
    if (x>=_w) return buffer->stub_pixel;
    return ( buffer->at(_xymap(_w, _h, static_cast<uint16_t>(x), static_cast<uint16_t>(y))) );
};

template <class COLOR_TYPE>
bool LedFB<COLOR_TYPE>::resize(uint16_t w, uint16_t h){
    if (buffer->resize(w*h) && (buffer->size() == w*h)){
        _w=w; _h=h;
        return true;
    }
    return false;
}

template <class COLOR_TYPE>
void LedFB<COLOR_TYPE>::fade(uint8_t v){
    // if buffer is of CRGB type
    if constexpr (std::is_same_v<CRGB, COLOR_TYPE>){
        for (auto i = buffer->begin(); i != buffer->end(); ++i)
            (*i).nscale8(255 - v);
    }
    // todo: implement fade for other color types
}

template <class COLOR_TYPE>
void LedFB<COLOR_TYPE>::dim(uint8_t v){
    // if buffer is of CRGB type
    if constexpr (std::is_same_v<CRGB, COLOR_TYPE>){
        for (auto i = buffer->begin(); i != buffer->end(); ++i)
            (*i).nscale8(v);
    }
    // todo: implement fade for other color types
}
