/*
    This file is a part of LedFB library project
    https://github.com/vortigont/LedFB

    Copyright © 2023-2024 Emil Muratov (vortigont)
*/

#include "ledstripe.hpp"


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
