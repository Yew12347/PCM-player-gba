#ifndef BN_SPRITE_ITEMS_UNIFONT_H
#define BN_SPRITE_ITEMS_UNIFONT_H

#include "bn_sprite_item.h"

//{{BLOCK(unifont_bn_gfx)

//======================================================================
//
//	unifont_bn_gfx, 16x7712@4, 
//	+ palette 16 entries, not compressed
//	+ 1928 tiles Metatiled by 2x2 not compressed
//	Total size: 32 + 61696 = 61728
//
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_UNIFONT_BN_GFX_H
#define GRIT_UNIFONT_BN_GFX_H

#define unifont_bn_gfxTilesLen 61696
extern const bn::tile unifont_bn_gfxTiles[1928];

#define unifont_bn_gfxPalLen 32
extern const bn::color unifont_bn_gfxPal[16];

#endif // GRIT_UNIFONT_BN_GFX_H

//}}BLOCK(unifont_bn_gfx)

namespace bn::sprite_items
{
    constexpr inline sprite_item unifont(sprite_shape_size(sprite_shape::SQUARE, sprite_size::NORMAL), 
            sprite_tiles_item(span<const tile>(unifont_bn_gfxTiles, 1928), bpp_mode::BPP_4, compression_type::NONE, 482), 
            sprite_palette_item(span<const color>(unifont_bn_gfxPal, 16), bpp_mode::BPP_4, compression_type::NONE));
}

#endif

