/*
    This file is a part of LedFB library project
    https://github.com/vortigont/LedFB

    Copyright © 2023 Emil Muratov (vortigont)
*/

#pragma once
#include "colormath.h"
#include <vector>
#include <memory>
#include <variant>
#include <functional>
#include <list>
#include "Arduino_GFX.h"
#include "ledstripe.hpp"
#include "FastLED.h"



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
     * same as dim(255 - v)
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

// overload pattern and deduction guide. Lambdas provide call operator
template<class... Ts> struct Overload : Ts... { using Ts::operator()...; };
template<class... Ts> Overload(Ts...) -> Overload<Ts...>;

/**
 * @brief GFX class for LedFB
 * it provides Arduino_GFX API for uderlaying buffer with either CRGB or uint16_t color container
 * 
 */
class LedFB_GFX : public Arduino_GFX {

protected:
    // LedFB container variant
    std::variant< std::shared_ptr< LedFB<CRGB> >, std::shared_ptr< LedFB<uint16_t> >  > _fb;

public:
    /**
     * @brief Construct a new LedFB_GFX object from a LedFB<CRGB> 24 bit color
     * 
     * @param buff - a shared pointer to the LedFB object
     */
    LedFB_GFX(std::shared_ptr< LedFB<CRGB> > buff) : Arduino_GFX(buff->w(), buff->h()), _fb(buff) {}

    /**
     * @brief Construct a new LedFB_GFX object from a LedFB<uint16_t> 16 bit color
     * 
     * @param buff - a shared pointer to the LedFB object
     */
    LedFB_GFX(std::shared_ptr< LedFB<uint16_t> > buff) : Arduino_GFX(buff->w(), buff->h()), _fb(buff) {}

    virtual ~LedFB_GFX() = default;



    // Arduino GFX overrides
    bool begin(int32_t speed = GFX_NOT_DEFINED) override { return true; };
    void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override;

    void writePixelPreclipped(int16_t x, int16_t y, CRGB color);

    // mapped writePixel methods
    __attribute__((always_inline)) inline void writePixel(int16_t x, int16_t y, uint16_t color){ writePixelPreclipped(x, y, color); };
    __attribute__((always_inline)) inline void writePixel(int16_t x, int16_t y, CRGB color){ writePixelPreclipped(x, y, color); };

    void fillScreen(uint16_t color);// override;

    // Adafruit-like methods for CRGB
    void fillScreen(CRGB color);


    // ***** Additional graphics functions *****

    /**
     * @brief Blend 1-bit image at the specified (x,y) position using alpha transparency
     * 
     * @param x Top left corner x coordinate
     * @param y Top left corner y coordinate
     * @param bitmap byte array with monochrome bitmap
     * @param w Width of bitmap in pixels
     * @param h Height of bitmap in pixels
     * @param front_color 16-bit 5-6-5 Color to draw image with
     * @param front_alpha image alpha transparency
     * @param back_color 16-bit 5-6-5 Color to draw background with
     * @param back_alpha image background alpha transparency
     * @param invert interchange canvas and text
     */
    void blendBitmap(int16_t x, int16_t y,
                    const uint8_t* bitmap, int16_t w, int16_t h,
                    uint16_t front_color, uint8_t front_alpha,
                    uint16_t back_color = 0, uint8_t back_alpha = 0);

    /**
     * @brief draw bitmap, fading background canvas by specified amount
     * 
     * @param x Top left corner x coordinate
     * @param y Top left corner y coordinate
     * @param bitmap byte array with monochrome bitmap
     * @param w Width of bitmap in pixels
     * @param h Height of bitmap in pixels
     * @param front_color 16-bit 5-6-5 Color to draw image with
     * @param fadeBy amount of fade applied to background
     */
    void fadeBitmap(int16_t x, int16_t y,
                    const uint8_t* bitmap, int16_t w, int16_t h,
                    uint16_t front_color, uint8_t fadeBy);


    // Color conversion
    /**
     * @brief Given RGB '565' format color, return expanded CRGB
     * 
     * @param c 
     * @return CRGB 
     */
    static CRGB colorCRGB(uint16_t c){ return CRGB( ((c>>11 & 0x1f) * 527 + 23) >> 6, ((c>>5 & 0xfc) * 259 + 33) >> 6, ((c&0x1f) * 527 + 23) >> 6); }

    /**
     * @brief Given 24-bit CRGB, return a 'packed' 16-bit color value in '565' RGB format (5 bits red, 6 bits green, 5 bits blue).
     * 
     * @param c CRGB
     * @return uint16_t 
     */
    static uint16_t color565(CRGB c){ return c.r >> 3 << 11 | c.g >> 2 << 5 | c.b >> 3; }


protected:
    // Additional methods

    void _drawPixelCRGB( LedFB<CRGB> *b, int16_t x, int16_t y, CRGB c){ b->at(x,y) = c; };
    void _drawPixelCRGB( LedFB<uint16_t> *b, int16_t x, int16_t y, CRGB c){ b->at(x,y) = color565(c); };

    void _drawPixel565( LedFB<CRGB> *b, int16_t x, int16_t y, uint16_t c){ b->at(x,y) = colorCRGB(c); };
    void _drawPixel565( LedFB<uint16_t> *b, int16_t x, int16_t y, uint16_t c){ b->at(x,y) = c; };

    void _fillScreenCRGB(LedFB<CRGB> *b, CRGB c){ b->fill(c); };
    void _fillScreenCRGB(LedFB<uint16_t> *b, CRGB c){ b->fill(color565(c)); };

    void _fillScreen565(LedFB<CRGB> *b, uint16_t c){ b->fill(colorCRGB(c)); };
    void _fillScreen565(LedFB<uint16_t> *b, uint16_t c){ b->fill(c); };

    void _nblendCRGB( LedFB<CRGB> *b, int16_t x, int16_t y, CRGB overlay, fract8 amountOfOverlay){ nblend( b->at(x,y), overlay, amountOfOverlay); };
    void _nblendCRGB( LedFB<uint16_t> *b, int16_t x, int16_t y, CRGB overlay, fract8 amountOfOverlay){ b->at(x,y) = color::alphaBlendRGB565(color565(overlay), b->at(x,y), amountOfOverlay); };

    void _nblend565( LedFB<CRGB> *b, int16_t x, int16_t y, uint16_t overlay, fract8 amountOfOverlay){ nblend( b->at(x,y), colorCRGB(overlay), amountOfOverlay); };
    void _nblend565( LedFB<uint16_t> *b, int16_t x, int16_t y, uint16_t overlay, fract8 amountOfOverlay){ b->at(x,y) = color::alphaBlendRGB565( overlay, b->at(x,y), amountOfOverlay); };

    void _nscale8( LedFB<CRGB> *b, int16_t x, int16_t y, uint8_t fadeBy){ b->at(x,y).nscale8(fadeBy); };
    void _nscale8( LedFB<uint16_t> *b, int16_t x, int16_t y, uint8_t fadeBy);

/*
    template<typename V, typename X, typename Y, typename C>
    void _visit_drawPixelCRGB(const V& variant, X x, Y y, C color){
        std::visit( Overload{ [&x, &y, &color](const auto& variant_item) { _drawPixelCRGB(variant_item, x, y, color); }, }, variant);
    };
*/


    //  https://stackoverflow.com/questions/18937701/combining-two-16-bits-rgb-colors-with-alpha-blending

};


/**
 * @brief abstract overlay engine
 * it works as a renderer for canvas, creating/mixing overlay/back buffer with canvas
 */
template <class COLOR_TYPE = CRGB>
class DisplayEngine {

protected:

    /**
     * @brief 
     * 
     */
    //std::unique_ptr< LedFB_GFX > gfx;

    /**
     * @brief pure virtual method implementing rendering buffer content to backend driver
     * 
     */
    virtual void engine_show() = 0;

public:
    DisplayEngine(){ }
    // virtual d-tor
    virtual ~DisplayEngine(){};

    /**
     * @brief show buffer content on display
     * it will draw a canvas content and render an overlay stack on top of it if necessary
     * 
     */
    virtual void show();

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

    /**
     * @brief activate double buffer
     * 
     * @param active 
     */
    virtual void doubleBuffer(bool active) = 0;

    /**
     * @brief check if doublebuffer is enabled
     * 
     * @return true 
     * @return false 
     */
    virtual bool doubleBuffer() const { return false; }

    /**
     * @brief swap content of front and back buffer
     * 
     */
    virtual void flipBuffer() = 0;

    /**
     * @brief switch rendering between back and front buffer
     * 
     * @return bool - true if acive buffer is front buffer, false if active buffer is back buffer
     */
    virtual bool toggleBuffer() = 0;

    virtual std::shared_ptr<PixelDataBuffer<COLOR_TYPE>> getBuffer(){ return getActiveBuffer(); };

    virtual std::shared_ptr<PixelDataBuffer<COLOR_TYPE>> getBackBuffer() = 0;

    virtual std::shared_ptr<PixelDataBuffer<COLOR_TYPE>> getActiveBuffer() = 0;

    /**
     * @brief copy back buffer to front buffer
     * 
     */
    virtual void copyBack2Front() = 0;

    /**
     * @brief copy front buffer to back buffer
     * 
     */
    virtual void copyFront2Back() = 0;

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

/**
 * @brief apply FastLED fadeToBlackBy() func to buffer
 * 
 * @param v 
 */
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



//  ****************************************
//  ************  DisplayEngine ************
//  ****************************************

template <class COLOR_TYPE>
void DisplayEngine<COLOR_TYPE>::show(){
  // call derivative engine show function
  engine_show();
}


