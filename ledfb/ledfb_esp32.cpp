/*
    This file is a part of LedFB library project
    https://github.com/vortigont/LedFB

    Copyright © 2023-2024 Emil Muratov (vortigont)
*/

//#ifdef ESP32
#include "ledfb_esp32.hpp"

ESP32RMTDisplayEngine::ESP32RMTDisplayEngine(int gpio, EOrder rgb_order){
    wsstrip = new(std::nothrow) ESP32RMT_WS2812B(gpio, rgb_order);
}

ESP32RMTDisplayEngine::ESP32RMTDisplayEngine(int gpio, EOrder rgb_order, std::shared_ptr<CLedCDB> buffer) : canvas(buffer) {
  wsstrip = new(std::nothrow) ESP32RMT_WS2812B(gpio, rgb_order);
  if (wsstrip && canvas){
      // attach buffer to RMT engine
      cled = &FastLED.addLeds(wsstrip, canvas->data().data(), canvas->size());
      // hook framebuffer to controller
      canvas->bind(cled);
      FastLED.show();
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
        FastLED.show();
        return true;
    }

    return false;   // something went either wrong or already been setup 
}

void ESP32RMTDisplayEngine::engine_show(){
  FastLED.show();
}

/*
void ESP32RMTDisplayEngine::engine_show(){
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
*/
void ESP32RMTDisplayEngine::clear(){
    if (!canvas) return;
    canvas->clear();
    if (backbuff){
      backbuff->clear();
        // release BackBuffer, it will be recreated if required
        //canvas->rebind(*backbuff.get());
        //backbuff.reset();
    }
    //auto ovr = overlay.lock();
    //if (ovr) ovr->clear();          // clear overlay
    FastLED.show();
}
/*
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
*/
uint8_t ESP32RMTDisplayEngine::brightness(uint8_t b){
    FastLED.setBrightness(b);
    return FastLED.getBrightness();
}

void ESP32RMTDisplayEngine::doubleBuffer(bool active){
  if (active && !backbuff){
    backbuff = std::make_shared<CLedCDB>(canvas->size());
    return;
  }
  // release back buffer
  if (!active && backbuff){
    if (backbuff->isBound())
      canvas->rebind(*backbuff);

    backbuff.reset();
    _active_buff = true;
  }
}

void ESP32RMTDisplayEngine::flipBuffer(){
  if (backbuff)
    canvas->swap(*backbuff);
}

bool ESP32RMTDisplayEngine::toggleBuffer(){
  if (backbuff){
    canvas->rebind(*backbuff);
    _active_buff = !_active_buff;
  }

  return _active_buff;
}

void ESP32RMTDisplayEngine::copyBack2Front(){
  if (backbuff){
    canvas->data() = backbuff->data();
  }
}

void ESP32RMTDisplayEngine::copyFront2Back(){
  if (backbuff){
    backbuff->data() = canvas->data();
  }
}

//#endif  //ifdef ESP32


//  **********************************
//  *** HUB75 Panel implementation ***
#ifdef LEDFB_WITH_HUB75_I2S
void HUB75PanelDB::show(){
    for (size_t i = 0; i != PixelDataBuffer<CRGB>::fb.size(); ++i){
        hub75.drawPixelRGB888( i % hub75.getCfg().mx_width, i / hub75.getCfg().mx_width, PixelDataBuffer<CRGB>::fb.at(i).r, PixelDataBuffer<CRGB>::fb.at(i).g, PixelDataBuffer<CRGB>::fb.at(i).b);
    }
}


ESP32HUB75_DisplayEngine::ESP32HUB75_DisplayEngine(const HUB75_I2S_CFG &config) : hub75(config){
  canvas = std::make_shared<PixelDataBuffer<CRGB>>(config.mx_height * config.mx_height);
  hub75.begin();
}

void ESP32HUB75_DisplayEngine::clear(){
  if (canvas) canvas->clear();
  if (backbuff) backbuff->clear();
  hub75.clearScreen();
}

void ESP32HUB75_DisplayEngine::engine_show(){
  if (_active_buff){
    for (size_t i = 0; i != canvas->size(); ++i){
      hub75.drawPixelRGB888( i % hub75.getCfg().mx_width, i / hub75.getCfg().mx_width, (*canvas)[i].r, (*canvas)[i].g, (*canvas)[i].b);
    }
  } else {
    for (size_t i = 0; i != backbuff->size(); ++i){
      hub75.drawPixelRGB888( i % hub75.getCfg().mx_width, i / hub75.getCfg().mx_width, (*backbuff)[i].r, (*backbuff)[i].g, (*backbuff)[i].b);
    }
  }

//  for (auto &s : _stack)
//    s.callback( getCanvas().get() );
}
/*
void ESP32HUB75_DisplayEngine::engine_show(){
    if (overlay.expired())
        return canvas->show();

    // need to apply overlay on canvas
    auto ovr = overlay.lock();
    if (canvas->size() != ovr->size()) return;  // a safe-check for buffer sizes

    // draw non key-color pixels from canvas, otherwise from overlay
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
}*/

void ESP32HUB75_DisplayEngine::doubleBuffer(bool active){
  if (active && !backbuff){
    backbuff = std::make_shared<PixelDataBuffer<CRGB>>(canvas->size());
    return;
  }

  // release back buffer
  if (!active && backbuff){
    backbuff.reset();
    _active_buff = true;
  }
}

void ESP32HUB75_DisplayEngine::flipBuffer(){
  if (backbuff)
    canvas->swap(*backbuff);
}

bool ESP32HUB75_DisplayEngine::toggleBuffer(){
  if (backbuff)
    _active_buff = !_active_buff;

  return _active_buff;
}

void ESP32HUB75_DisplayEngine::copyBack2Front(){
  if (backbuff){
    *canvas = *backbuff;
  }
}

void ESP32HUB75_DisplayEngine::copyFront2Back(){
  if (backbuff){
    *backbuff = *canvas;
  }
}

#endif // LEDFB_WITH_HUB75_I2S


