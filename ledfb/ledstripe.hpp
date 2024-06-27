/*
    This file is a part of LedFB library project
    https://github.com/vortigont/LedFB

    Copyright © 2023-2024 Emil Muratov (vortigont)
*/

#pragma once
#include <stddef.h>
#include <stdint.h>

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

