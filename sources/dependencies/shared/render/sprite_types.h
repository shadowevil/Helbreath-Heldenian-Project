// sprite_types.h: Shared sprite types for renderer abstraction
//
// Part of the shared interface layer between client and renderers
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

namespace hb::shared::sprite {

// Frame rectangle - matches PAKLib::sprite_rect binary layout exactly
struct SpriteRect {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    int16_t pivotX;
    int16_t pivotY;
};

// Bounding rectangle from last draw operation
// Used for hit testing and collision detection
struct BoundRect {
    int left = 0;
    int top = -1;  // -1 indicates invalid/not drawn
    int right = 0;
    int bottom = 0;

    bool IsValid() const { return top != -1; }
};

// Alpha blend presets matching legacy transparency levels
enum class AlphaPreset {
    Opaque = 100,   // No transparency
    Alpha70 = 70,   // 70% opacity
    Alpha50 = 50,   // 50% opacity
    Alpha25 = 25    // 25% opacity
};

// Blend modes for sprite rendering
enum class BlendMode {
    Alpha,          // Standard alpha blending: result = src * alpha + dst * (1-alpha)
    Additive,       // Additive blending: result = src + dst (for light effects)
    AdditiveOffset, // Additive blend with per-pixel color offset (DDraw PutTransSpriteRGB)
    Average,        // 50/50 averaging: result = (src + dst) / 2 (original PutTransSprite2)
    Multiply        // Multiply blending: result = src * dst (DDraw PutDarkSprite, for dark fog/shadow)
};

// Drawing parameters for sprite rendering
struct DrawParams {
    // Alpha/transparency (0.0 = invisible, 1.0 = opaque)
    float m_alpha = 1.0f;

    // Color tint offset (-255 to +255 for each channel)
    int16_t m_tint_r = 0;
    int16_t m_tint_g = 0;
    int16_t m_tint_b = 0;
    bool m_has_tint = false;

    // Rendering flags
    bool m_use_color_key = true;    // false = NoColorKey variants
    bool m_shadow = false;      // Shadow projection effect
    bool m_reverse = false;     // Reverse blend effect
    bool m_fade = false;        // Fade effect
    bool m_additive = false;    // Deprecated: use blendMode instead
    bool m_nearest_filter = false; // Force nearest-neighbor filtering (no bilinear smoothing)
    bool m_ignore_pivot = false;   // Draw at exact position, ignoring the sprite's baked-in pivot offset
    BlendMode m_blend_mode = BlendMode::Alpha;  // Blend mode for rendering

    // Static factory methods for common configurations
    static DrawParams opaque() {
        return {};
    }

    static DrawParams alpha_blend(float a) {
        DrawParams p;
        p.m_alpha = a;
        return p;
    }

    static DrawParams alpha_blend(AlphaPreset preset) {
        DrawParams p;
        p.m_alpha = static_cast<float>(preset) / 100.0f;
        return p;
    }

    static DrawParams tint(int16_t r, int16_t g, int16_t b) {
        DrawParams p;
        p.m_tint_r = r;
        p.m_tint_g = g;
        p.m_tint_b = b;
        p.m_has_tint = true;
        return p;
    }

    static DrawParams tinted_alpha(int16_t r, int16_t g, int16_t b, float a) {
        DrawParams p;
        p.m_alpha = a;
        p.m_tint_r = r;
        p.m_tint_g = g;
        p.m_tint_b = b;
        p.m_has_tint = true;
        return p;
    }

    static DrawParams shadow() {
        DrawParams p;
        p.m_shadow = true;
        return p;
    }

    static DrawParams no_color_key() {
        DrawParams p;
        p.m_use_color_key = false;
        return p;
    }

    static DrawParams fade() {
        DrawParams p;
        p.m_fade = true;
        return p;
    }

    static DrawParams additive(float a = 1.0f) {
        DrawParams p;
        p.m_alpha = a;
        p.m_blend_mode = BlendMode::Additive;
        return p;
    }

    // Additive without color key - matches original PutTransSprite_NoColorKey
    // Black pixels naturally become transparent through additive blending (adding 0 = no change)
    static DrawParams additive_no_color_key(float a = 1.0f) {
        DrawParams p;
        p.m_alpha = a;
        p.m_use_color_key = false;
        p.m_blend_mode = BlendMode::Additive;
        return p;
    }

    // Additive with tint offset and no color key - matches original PutTransSpriteRGB
    // Applies RGB offset to source channels before additive blending
    static DrawParams additive_tinted(int16_t r, int16_t g, int16_t b) {
        DrawParams p;
        p.m_tint_r = r;
        p.m_tint_g = g;
        p.m_tint_b = b;
        p.m_use_color_key = false;
        p.m_blend_mode = BlendMode::AdditiveOffset;
        p.m_has_tint = true;
        return p;
    }

    // Additive with color boost for light effects - multiplies sprite by bright color before adding
    static DrawParams additive_colored(int16_t r, int16_t g, int16_t b, float a = 1.0f) {
        DrawParams p;
        p.m_alpha = a;
        p.m_tint_r = r;
        p.m_tint_g = g;
        p.m_tint_b = b;
        p.m_blend_mode = BlendMode::Additive;
        p.m_has_tint = true;
        return p;
    }

    // 50/50 averaging blend: result = (src + dst) / 2
    // Makes sprites semi-transparent by blending equally with background
    static DrawParams average() {
        DrawParams p;
        p.m_blend_mode = BlendMode::Average;
        return p;
    }

    // Multiply blend: result = src * dst (darkening effect)
    // Dark sprite pixels darken the background, white pixels preserve it
    // Used for dark fog, shadows, and ground darkening effects (DDraw PutDarkSprite)
    static DrawParams multiply(float a = 1.0f) {
        DrawParams p;
        p.m_alpha = a;
        p.m_blend_mode = BlendMode::Multiply;
        return p;
    }
};

} // namespace hb::shared::sprite
