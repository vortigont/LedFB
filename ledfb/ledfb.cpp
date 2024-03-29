/*
    This file is a part of LedFB library project
    https://github.com/vortigont/LedFB

    Copyright © 2023 Emil Muratov (vortigont)
*/

#include "ledfb.hpp"

// Timings from FastLED chipsets.h
// WS2812@800kHz - 250ns, 625ns, 375ns
// время "отправки" кадра в матрицу, мс. где 1.5 эмпирический коэффициент
// #define FastLED_SHOW_TIME = WIDTH * HEIGHT * 24 * (0.250 + 0.625) / 1000 * 1.5

// Out-of-bound CRGB placeholder - stub pixel that is mapped to either nonexistent buffer access or blackholed CLedController mapping
static CRGB blackhole;

// *** CLedCDB implementation ***

// move construct
CLedCDB::CLedCDB(CLedCDB&& rhs) noexcept : PixelDataBuffer(std::forward<CLedCDB>(rhs)) {
    cled = rhs.cled;
    if (rhs.cled){  // steal cled pointer, if set
        rhs.cled = nullptr;
    }
    _reset_cled();      // if we moved data from rhs, than need to reset cled controller
    //LOG(printf, "Move Constructing: %u From: %u\n", reinterpret_cast<size_t>(fb.data()), reinterpret_cast<size_t>(rhs.fb.data()));
};

// move assignment
CLedCDB& CLedCDB::operator=(CLedCDB&& rhs){
    fb = std::move(rhs.fb);

    if (cled && rhs.cled && (cled != rhs.cled)){
        /* oops... we are moving from a buff binded to some other cled controller
        * since there is no method to detach active controller from CFastLED
        * than let's use a dirty WA - bind a blackhole to our cled and steal rhs's ptr
        */
        cled->setLeds(&blackhole, 1);
    }

    if (rhs.cled){ cled = rhs.cled; rhs.cled = nullptr; }   // steal a pointer from rhs
    _reset_cled();      // if we moved data from rhs, than need to reset cled controller
    //LOG(printf, "Move assign: %u from: %u\n", reinterpret_cast<size_t>(fb.data()), reinterpret_cast<size_t>(rhs.fb.data()));
    // : fb(std::move(rhs.fb)), cfg(rhs.cfg){ LOG(printf, "Move Constructing: %u From: %u\n", reinterpret_cast<size_t>(&fb), reinterpret_cast<size_t>(&rhs.fb)); };
    return *this;
}

CLedCDB::~CLedCDB(){
    if (cled){
        /* oops... somehow we ended up in destructor with binded cled controller
        * since there is no method to detach active controller from CFastLED
        * than let's use a dirty WA - bind a blackhole to cled untill some other buffer
        * regains control
        */
       cled->setLeds(&blackhole, 1);
    }
}

void CLedCDB::swap(PixelDataBuffer& rhs){
    rhs.swap(*this);
    _reset_cled();
}

void CLedCDB::swap(CLedCDB& rhs){
    std::swap(fb, rhs.fb);
    _reset_cled();
    rhs._reset_cled();
}

bool CLedCDB::resize(size_t s){
    bool result = PixelDataBuffer::resize(s);
    _reset_cled();
    return result;
};

bool CLedCDB::bind(CLEDController *pLed){
    if (!pLed) return false;  // some empty pointer

    /* since there is no method to unbind CRGB buffer from CLEDController,
    than if there is a pointer already exist we refuse to rebind to a new cled.
    It's up to user to deal with it
    */
    if (cled && (cled !=pLed)) return false;

    cled = pLed;
    _reset_cled();
    return true;
}

void CLedCDB::rebind(CLedCDB &rhs){
    //LOG(println, "PixelDataBuffer rebind");
    std::swap(cled, rhs.cled);  // swap pointers, if any
    _reset_cled();
    rhs._reset_cled();
}


#ifdef ESP32
ESP32RMTDisplayEngine::ESP32RMTDisplayEngine(int gpio, EOrder rgb_order){
    wsstrip = new(std::nothrow) ESP32RMT_WS2812B(gpio, rgb_order);
}

ESP32RMTDisplayEngine::ESP32RMTDisplayEngine(int gpio, EOrder rgb_order, std::shared_ptr<CLedCDB> buffer) : canvas(buffer) {
    wsstrip = new(std::nothrow) ESP32RMT_WS2812B(gpio, rgb_order);
    if (wsstrip && canvas){
        // attach buffer to RMT engine
        cled = &FastLED.addLeds(wsstrip, canvas->data().data(), canvas->size());
        // hook framebuffer to contoller
        canvas->bind(cled);
        show();
    }
}

bool ESP32RMTDisplayEngine::attachCanvas(std::shared_ptr<CLedCDB> &fb){
    if (cled) return false; // this function is not idempotent, so refuse to mess with existing controller

    // share data buffer instance
    canvas = fb;

    if (wsstrip && canvas){
        // attach buffer to RMT engine
        cled = &FastLED.addLeds(wsstrip, canvas->data().data(), canvas->size());
        // hook framebuffer to contoller
        canvas->bind(cled);
        show();
        return true;
    }

    return false;   // somethign went either wrong or already been setup 
}

void ESP32RMTDisplayEngine::show(){
    if (!overlay.expired() && _canvas_protect && !backbuff)    // check if I need to switch to back buff due to canvas persistency and overlay data present 
        _switch_to_bb();

    // check if back-buffer is present but no longer needed (if no overlay present or canvas is not persistent anymore)
    if (backbuff && (overlay.expired() || !_canvas_protect)){
        //LOG(println, "BB reset");
        canvas->rebind(*backbuff.get());
        backbuff.reset();
    }

    if (!overlay.expired()) _ovr_overlap(); // apply overlay to either canvas or back buffer, if bb is present
    FastLED.show();
}

void ESP32RMTDisplayEngine::clear(){
    if (!canvas) return;
    canvas->clear();
    if (backbuff){
        // release BackBuffer, it will be recreated if required
        canvas->rebind(*backbuff.get());
        backbuff.reset();
    }
    auto ovr = overlay.lock();
    if (ovr) ovr->clear();          // clear overlay
    FastLED.show();
}

std::shared_ptr<PixelDataBuffer<CRGB>> ESP32RMTDisplayEngine::getOverlay(){
    auto p = overlay.lock();
    if (!p){
        // no overlay exist at the moment
        p = std::make_shared<CLedCDB>(canvas->size());
        overlay = p;
    }
    return p;
}

void ESP32RMTDisplayEngine::_ovr_overlap(){
    auto ovr = overlay.lock();
    if (canvas->size() != ovr->size()) return;  // a safe-check for buffer sizes

    auto ovr_iterator = ovr->begin();

    if (backbuff.get()){
        auto bb_iterator = backbuff->begin();
        // fill BackBuffer with either canvas or overlay based on keycolor
        for (auto i = canvas->begin(); i != canvas->end(); ++i ){
            *bb_iterator = (*ovr_iterator == _transparent_color) ? *i : *ovr_iterator;
            ++ovr_iterator;
            ++bb_iterator;
        }
    } else {
        // apply all non key-color pixels to canvas
        for (auto i = canvas->begin(); i != canvas->end(); ++i ){
            if (*ovr_iterator != _transparent_color)
                *i = *ovr_iterator;
            ++ovr_iterator;
        }
    }   
}

void ESP32RMTDisplayEngine::_switch_to_bb(){
    //LOG(println, "Switch to BB");
    backbuff = std::make_unique<CLedCDB>(canvas->size());
    backbuff->rebind(*canvas);    // switch backend binding
}

uint8_t ESP32RMTDisplayEngine::brightness(uint8_t b){
    FastLED.setBrightness(b);
    return FastLED.getBrightness();
}
#endif  //ifdef ESP32




#ifdef LEDFB_WITH_HUB75_I2S
//  *** HUB75 Panel implementation ***
void HUB75PanelDB::show(){
    for (size_t i = 0; i != PixelDataBuffer<CRGB>::fb.size(); ++i){
        hub75.drawPixelRGB888( i % hub75.getCfg().mx_width, i / hub75.getCfg().mx_width, PixelDataBuffer<CRGB>::fb.at(i).r, PixelDataBuffer<CRGB>::fb.at(i).g, PixelDataBuffer<CRGB>::fb.at(i).b);
    }
}
/*
void HUB75PanelDB::clear(){
    PixelDataBuffer::clear();
    hub75.clearScreen();
}
*/

ESP32HUB75_DisplayEngine::ESP32HUB75_DisplayEngine(const HUB75_I2S_CFG &config) : canvas(new HUB75PanelDB(config)) {
    canvas->hub75.begin();
}

void ESP32HUB75_DisplayEngine::clear(){
    if (!canvas) return;
    canvas->wipe();                 // use fast DMA wipe
    auto ovr = overlay.lock();
    if (ovr) ovr->clear();          // clear overlay

}

void ESP32HUB75_DisplayEngine::show(){
    if (overlay.expired())
        return canvas->show();

    // need to apply overlay on canvas
    auto ovr = overlay.lock();
    if (canvas->size() != ovr->size()) return;  // a safe-check for buffer sizes

    // draw non key-color pixels from canvas, otherwise from canvas
    for (size_t i=0; i != canvas->size(); ++i ){
        CRGB c = ovr->at(i) == _transparent_color ? canvas->at(i) : ovr->at(i);

        canvas->hub75.drawPixelRGB888( i % canvas->hub75.getCfg().mx_width, i / canvas->hub75.getCfg().mx_width, c.r, c.g, c.b);
    }
}

std::shared_ptr<PixelDataBuffer<CRGB>> ESP32HUB75_DisplayEngine::getOverlay(){
    auto p = overlay.lock();
    if (!p){
        // no overlay exist at the moment
        p = std::make_shared<PixelDataBuffer<CRGB>>( PixelDataBuffer<CRGB>(canvas->size()) );
        overlay = p;
    }
    return p;
}
#endif LEDFB_WITH_HUB75_I2S




// *** Topology mapping classes implementation ***

// matrix stripe layout transformation
size_t LedStripe::transpose(unsigned w, unsigned h, unsigned x, unsigned y) const {
    if ( _vertical ){
        // vertically ordered stripes
        bool virtual_v_mirror =  (_snake && (_hmirror ? w-x-1 : x )%2) ? !_vmirror : _vmirror; // for snake-shaped strip, invert vertical odd columns either from left or from right side
        size_t xx = _hmirror ? w - x-1 : x;
        size_t yy = virtual_v_mirror ? h-y-1 : y;
        return xx * h + yy;
    } else {
        bool virtual_h_mirror = (_snake && (_vmirror ? h-y-1 : y)%2) ? !_hmirror : _hmirror; // for snake-shaped displays, invert horizontal odd rows either from the top or bottom
        size_t xx = virtual_h_mirror ? w - x-1 : x;
        size_t yy = _vmirror ? h-y-1 : y;
        return yy * w + xx;
    }
}

size_t LedTiles::transpose(unsigned w, unsigned h, unsigned x, unsigned y) const {
    if (_tile_wcnt == 1 && _tile_hcnt == 1)
        return LedStripe::transpose(w,h,x,y);

    return tiled_transpose(w,h,x,y);
}

size_t LedTiles::tiled_transpose(unsigned w, unsigned h, unsigned x, unsigned y) const {
    // find tile's coordinate where our target pixel is located
    unsigned tile_x = x / _tile_w , tile_y = y / _tile_h;
    // find transposed tile's number
    unsigned tile_num = tileLayout.transpose(_tile_wcnt, _tile_hcnt, tile_x, tile_y);

    // find transposed pixel's number in specific tile
    unsigned px_in_tile = LedStripe::transpose(_tile_w,_tile_h, x%_tile_w, y%_tile_h);

    // return cummulative pixel's number in 1D vector
    auto i =  _tile_w*_tile_h*tile_num + px_in_tile;
    //Serial.printf("tiledXY:%d,%d, tnum:%d, pxit:%d, idx:%d\n", tile_x, tile_y, tile_num, px_in_tile, i);
    return i;
}

void LedFB_GFX::drawPixel(int16_t x, int16_t y, uint16_t color) {
    int16_t t;
    switch (rotation) {
    case 1:
      t = x;
      x = width() - 1 - y;
      y = t;
      break;
    case 2:
      x = width() - 1 - x;
      y = height() - 1 - y;
      break;
    case 3:
      t = x;
      x = y;
      y = height() - 1 - t;
      break;
    }
    std::visit(
        Overload {
            [this, &x, &y, &color](const auto& variant_item) { _drawPixelC16(variant_item.get(), x,y,color); },
        },
        _fb
    );
}

void LedFB_GFX::fillScreen(uint16_t color) {
    std::visit(
        Overload {
            [this, &color](const auto& variant_item) { _fillScreenC16(variant_item.get(), color); },
        },
        _fb
    );
}

void LedFB_GFX::drawPixel(int16_t x, int16_t y, CRGB color) {
    int16_t t;
    switch (rotation) {
    case 1:
      t = x;
      x = width() - 1 - y;
      y = t;
      break;
    case 2:
      x = width() - 1 - x;
      y = height() - 1 - y;
      break;
    case 3:
      t = x;
      x = y;
      y = height() - 1 - t;
      break;
    }
    std::visit( Overload{ [this, &x, &y, &color](const auto& variant_item) { _drawPixelCRGB(variant_item.get(), x,y,color); }, }, _fb);
}

void LedFB_GFX::fillScreen(CRGB color) {
    std::visit( Overload{ [this, &color](const auto& variant_item) { _fillScreenCRGB(variant_item.get(), color); }, }, _fb);
}

