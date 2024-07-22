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




/*
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
            [this, &x, &y, &color](const auto& variant_item) { _drawPixel565(variant_item.get(), x,y,color); },
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
*/

void LedFB_GFX::fillScreen(uint16_t color) {
    std::visit(
        Overload {
            [this, &color](const auto& variant_item) { _fillScreen565(variant_item.get(), color); },
        },
        _fb
    );
}

void LedFB_GFX::fillScreen(CRGB color) {
    std::visit( Overload{ [this, &color](const auto& variant_item) { _fillScreenCRGB(variant_item.get(), color); }, }, _fb);
}

void LedFB_GFX::writePixelPreclipped(int16_t x, int16_t y, uint16_t color){ 
  int16_t t;
  switch (_rotation) {
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
          [this, &x, &y, &color](const auto& variant_item) { _drawPixel565(variant_item.get(), x,y,color); },
      },
      _fb
  );
};

void LedFB_GFX::writePixelPreclipped(int16_t x, int16_t y, CRGB color){ 
  int16_t t;
  switch (_rotation) {
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
};

void LedFB_GFX::drawBitmap_alphablend(int16_t x, int16_t y,
                    const uint8_t* bitmap, int16_t w, int16_t h,
                    CRGB colorFront, uint8_t alphaFront,
                    CRGB colorBack, uint8_t alphaBack)
{

  if (!bitmap) return;

  int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
  uint8_t byte = 0;

  //startWrite();
  for (int16_t j = 0; j < h; j++, y++)
  {
    for (int16_t i = 0; i < w; i++)
    {
      if (i & 7)
        byte <<= 1;
      else
        byte = bitmap[j * byteWidth + i / 8];

      int16_t __x = x+i;
      CRGB c( (byte & 0x80) ? colorFront : colorBack );
      uint8_t alpha((byte & 0x80) ? alphaFront : alphaBack);

      if (alpha == 0)
        continue;
      else if (alpha == 255)
        writePixel(__x, y, c);
      else
        std::visit( Overload{ [this, &__x, &y, &c, &alpha](const auto& variant_item) { _nblendCRGB(variant_item.get(), __x, y, c, alpha); }, }, _fb);

      //nblend(canvas->at( x+i, y), byte & 0x80 ? CRGB::Red : CRGB::White, byte & 0x80 ? 208 : 5);  // = alphaBlend(fb->at(x+i, y), byte & 0x80 ? CRGB::Blue : CRGB::Gray,  byte & 0x80 ? 5 : 250 );
      //color::alphaBlendRGB565
    }
  }
  //endWrite();
}

void LedFB_GFX::drawBitmap_bgfade(int16_t x, int16_t y,
                    const uint8_t* bitmap, int16_t w, int16_t h,
                    CRGB colorFront, uint8_t fadeBy)
{
  if (!bitmap) return;
  int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
  uint8_t byte = 0;


  //startWrite();
  for (int16_t j = 0; j < h; j++, y++)
  {
    for (int16_t i = 0; i < w; i++)
    {
      if (i & 7)
        byte <<= 1;
      else
        byte = bitmap[j * byteWidth + i / 8];

      int16_t __x = x+i;
      if (byte & 0x80){
        // write bitmap pixel with foreground color
        writePixel(__x, y, colorFront);
      } else {
        // fade background where bitmap is zero
        std::visit( Overload{ [this, &__x, &y, &fadeBy](const auto& variant_item) { _nscale8(variant_item.get(), __x,y,fadeBy); }, }, _fb);
      }
    }
  }
  //endWrite();
}


void LedFB_GFX::_nscale8( LedFB<uint16_t> *b, int16_t x, int16_t y, uint8_t fadeBy){
  CRGB c(colorCRGB(b->at(x,y)));
  c.nscale8(fadeBy);
  b->at(x,y) = color565(c);
}

void LedFB_GFX::drawBitmap_scale_colors(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, CRGB colorFront, CRGB colorBack){
  if (!bitmap) return;
  int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
  uint8_t byte = 0;

  //startWrite();
  for (int16_t j = 0; j != h; j++, y++)
  {
    for (int16_t i = 0; i != w; i++)
    {
      if (i & 7)
        byte <<= 1;
      else
        byte = bitmap[j * byteWidth + i / 8];

      int16_t __x = x+i;
      CRGB c( (byte & 0x80) ? colorFront : colorBack );

      if (c == CRGB::Black)
        writePixel(__x, y, CRGB::Black);
      else if (c == CRGB::White)
        continue;
      else
        std::visit( Overload{ [this, &__x, &y, &c](const auto& variant_item) { _nscale8(variant_item.get(), __x,y,c); }, }, _fb);
    }

  }
  //endWrite();
}
