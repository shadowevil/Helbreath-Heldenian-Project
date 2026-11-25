// sfml_sprite.h: SFML implementation of ISprite interface
//
// Part of SFMLEngine static library
// Handles sprite rendering using SFML textures with support for
// alpha blending, tinting, shadows, and other effects
//////////////////////////////////////////////////////////////////////

#pragma once

#include "sprite.h"
#include "sprite_types.h"
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <string>
#include <vector>
#include <cstdint>

// Forward declaration
class SFMLRenderer;

class SFMLSprite : public hb::shared::sprite::ISprite
{
public:
    // Construction from pre-loaded metadata (no file I/O, no image data); the texture is
    // loaded from image_path on first draw, and an empty image_path is an empty sprite.
    // frames are in image pixels; art_scale > 1 marks a high-resolution sheet (image pixels per logical
    // pixel): frame rects and pivots are reported divided by it, and frames draw at that logical size
    // with smooth, mipmapped sampling.
    SFMLSprite(SFMLRenderer* renderer, const std::vector<hb::shared::sprite::SpriteRect>& frames,
               const std::string& image_path, bool alphaEffect = true, int art_scale = 1);

    virtual ~SFMLSprite();

    //------------------------------------------------------------------
    // ISprite Implementation
    //------------------------------------------------------------------

    // Core drawing
    void draw(int x, int y, int frame, const hb::shared::sprite::DrawParams& params = hb::shared::sprite::DrawParams{}) override;
    void DrawToSurface(void* destSurface, int x, int y, int frame, const hb::shared::sprite::DrawParams& params = hb::shared::sprite::DrawParams{}) override;
    void DrawWidth(int x, int y, int frame, int width, bool vertical = false) override;
    void DrawShifted(int x, int y, int shiftX, int shiftY, int frame, const hb::shared::sprite::DrawParams& params = hb::shared::sprite::DrawParams{}) override;

    // Frame information
    int GetFrameCount() const override;
    hb::shared::sprite::SpriteRect GetFrameRect(int frame) const override;
    void GetBoundingRect(int x, int y, int frame, int& left, int& top, int& right, int& bottom) override;
    void CalculateBounds(int x, int y, int frame) override;
    bool GetLastDrawBounds(int& left, int& top, int& right, int& bottom) const override;
    hb::shared::sprite::BoundRect GetBoundRect() const override;

    // Collision detection
    bool CheckCollision(int spriteX, int spriteY, int frame, int pointX, int pointY) override;

    // Resource management — Unload() releases GPU texture and collision image
    // but keeps frame metadata and image path for lazy reload on next draw
    void Preload() override;
    void Unload() override;
    bool IsLoaded() const override;
    void Restore() override;
    bool IsInUse() const override;
    uint32_t GetLastAccessTime() const override;

    //------------------------------------------------------------------
    // SFML-Specific Methods
    //------------------------------------------------------------------

    // get the SFML texture
    const sf::Texture& GetTexture() const { return m_texture; }

    // Alpha degree management
    void set_ambient_light_level(char level);
    char get_ambient_light_level() const { return m_ambient_light_level; }
    bool HasAlphaEffect() const { return m_alphaEffect; }

private:
    //------------------------------------------------------------------
    // Internal Methods
    //------------------------------------------------------------------

    // Read the image file and create the texture and collision image; called lazily on first
    // use and again after Unload()
    bool load_texture();

    // draw implementation
    void DrawInternal(sf::RenderTarget* target, int x, int y, int frame, const hb::shared::sprite::DrawParams& params);

    // A frame's rect in image pixels (the stored rect; the logical one at art scale 1)
    const hb::shared::sprite::SpriteRect& art_rect(int frame) const
    {
        return m_art_frames.empty() ? m_frames[frame] : m_art_frames[frame];
    }

    // Apply alpha degree effect
    void ApplyAmbientLightLevel();

    //------------------------------------------------------------------
    // Member Variables
    //------------------------------------------------------------------

    // hb::shared::render::Renderer reference
    SFMLRenderer* m_renderer;

    // Image file (for lazy loading); empty for an empty sprite
    std::string m_image_path;

    // Frame data from the asset manifest, in logical pixels (what GetFrameRect reports). With art_scale > 1
    // only width, height and pivots are meaningful; x/y are floored and never sampled (m_art_frames is)
    std::vector<hb::shared::sprite::SpriteRect> m_frames;

    // High-resolution sheet: image pixels per logical pixel, and the frames as stored, in image
    // pixels (empty at art scale 1, where m_frames is already in image pixels)
    int m_art_scale;
    std::vector<hb::shared::sprite::SpriteRect> m_art_frames;

    uint16_t m_bitmapWidth;
    uint16_t m_bitmapHeight;

    // SFML texture (32-bit RGBA, lazy loaded)
    sf::Texture m_texture;
    sf::Image m_collisionImage;  // Retained for pixel-perfect collision detection
    bool m_textureLoaded;
    bool m_load_failed;  // Image unreadable: logged once, not retried until Unload() or Restore()

    // Alpha effect
    bool m_alphaEffect;
    char m_ambient_light_level;

    // State
    bool m_inUse;
    hb::shared::sprite::BoundRect m_boundRect;
    uint32_t m_lastAccessTime;
};
