## Template-based LED framebuffer library for FastLED / AdafruitGFX API

This lib is used in [FireLamp_JeeUI](https://github.com/vortigont/FireLamp_JeeUI) project - WS2812 LED / HUB75 Informer FireLamp based on EmbUI framework.

### Features
 - provides common API for FastLED/AdafritGFX/HUB75 I2S engines
 - provides FastLED ESP32-RMT engine wrapper class that allows run-time configurable gpio and COLOR_ORDER types for WS2812 adresable led strips engine


### ESP32-RMT engine wrapper
FastLED lib due to templated constructors it does not allows run-time definable gpio selection and color ordering for WS2812 stripes.
Ref issues: [#282](https://github.com/FastLED/FastLED/issues/282), [#826](https://github.com/FastLED/FastLED/issues/826), [#1594](https://github.com/FastLED/FastLED/issues/1594)
