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

ESP32HUB75_OverlayEngine::ESP32HUB75_OverlayEngine(const HUB75_I2S_CFG &config) : canvas(new HUB75Panel(config)) {
    canvas->MatrixPanel_I2S_DMA::begin();
}

std::shared_ptr<PixelDataBuffer<CRGB>> ESP32HUB75_OverlayEngine::getOverlay(){
    auto p = overlay.lock();
    if (!p){
        // no overlay exist at the moment
        p = std::make_shared<PixelDataBuffer<CRGB>>( PixelDataBuffer<CRGB>(canvas->size()) );
        overlay = p;
    }
    return p;
}

void ESP32HUB75_OverlayEngine::show(){
    canvas->show();
}

// *** Topology mapping classes implementation ***

// matrix stripe layout transformation
size_t LedStripe::transpose(unsigned w, unsigned h, unsigned x, unsigned y) const {
    unsigned idx = y*w+x;
    if ( _vertical ){
        // verticaly ordered stripes
        bool virtual_mirror = (_snake && x%2) ? !_vmirror : _vmirror;    // for snake-shaped strip, invert vertical odd columns
        size_t xx = virtual_mirror ? w - idx/h-1 : idx/h;
        size_t yy = _hmirror ? h - idx%h-1 : idx%h;
        return yy * w + xx;
    } else {
        // horizontaly ordered stripes
        bool virtual_mirror = (_snake && y%2) ? !_hmirror : _hmirror; // for snake-shaped displays, invert horizontal odd rows
        size_t xx = virtual_mirror ? w - idx%w-1 : idx%w;
        size_t yy = _vmirror ? h - idx/w-1 : idx/w;
        return yy * w + xx;
    }
}



void LedFB_GFX::drawPixel(int16_t x, int16_t y, uint16_t color) {
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
    std::visit( Overload{ [this, &x, &y, &color](const auto& variant_item) { _drawPixelCRGB(variant_item.get(), x,y,color); }, }, _fb);
}

void LedFB_GFX::fillScreen(CRGB color) {
    std::visit( Overload{ [this, &color](const auto& variant_item) { _fillScreenCRGB(variant_item.get(), color); }, }, _fb);
}


//  *** HUB75 Panel implementation ***
void HUB75Panel::show(){
    uint16_t w{getCfg().mx_width}, h{getCfg().mx_width};
    for (size_t i = 0; i != PixelDataBuffer<CRGB>::fb.size(); ++i){
        updateMatrixDMABuffer( i%w, i/w, PixelDataBuffer<CRGB>::fb.at(i).r, PixelDataBuffer<CRGB>::fb.at(i).g, PixelDataBuffer<CRGB>::fb.at(i).b);
    }
}

void HUB75Panel::clear(){
    
}

void ESP32HUB75_OverlayEngine::clear(){
    if (!canvas) return;
    canvas->clear();
    if (backbuff){
        // release BackBuffer, it will be recreated if required
        backbuff.release();
    }
    auto ovr = overlay.lock();
    if (ovr) ovr->clear();          // clear overlay
}