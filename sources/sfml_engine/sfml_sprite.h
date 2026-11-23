// sfml_sprite.h: SFML implementation of ISprite interface
//
// Part of SFMLEngine static library
// Handles sprite rendering using SFML textures with support for
// alpha blending, tinting, shadows, and other effects
//////////////////////////////////////////////////////////////////////

#pragma once

#include "sprite.h"
#include "PAK.h"
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
    // Construction from file path (reads metadata only — texture loaded on demand)
    SFMLSprite(SFMLRenderer* renderer, const std::string& pakFilePath, int spriteIndex, bool alphaEffect = true);

    // Construction from pre-loaded metadata (no file I/O, no image data — used by SpriteLoader)
    SFMLSprite(SFMLRenderer* renderer, const std::vector<PAKLib::sprite_rect>& frames,
               const std::string& pakFilePath, int spriteIndex, bool alphaEffect = true);

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
    // but keeps frame metadata and PAK path for lazy reload on next draw
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

    // Create SFML texture from 16-bit image data
    bool create_texture();

    // Re-read sprite image data from PAK file after Unload()
    bool reload_from_pak();

    // draw implementation
    void DrawInternal(sf::RenderTarget* target, int x, int y, int frame, const hb::shared::sprite::DrawParams& params);

    // Apply alpha degree effect
    void ApplyAmbientLightLevel();

    //------------------------------------------------------------------
    // Member Variables
    //------------------------------------------------------------------

    // hb::shared::render::Renderer reference
    SFMLRenderer* m_renderer;

    // PAK file info (for lazy loading)
    std::string m_pakFilePath;
    int m_spriteIndex;

    // Frame data from PAK
    std::vector<PAKLib::sprite_rect> m_frames;

    // PNG image data (cleared after texture creation to save memory)
    std::vector<uint8_t> m_imageData;
    uint16_t m_bitmapWidth;
    uint16_t m_bitmapHeight;

    // SFML texture (32-bit RGBA, lazy loaded)
    sf::Texture m_texture;
    sf::Image m_collisionImage;  // Retained for pixel-perfect collision detection
    bool m_textureLoaded;

    // Alpha effect
    bool m_alphaEffect;
    char m_ambient_light_level;

    // State
    bool m_inUse;
    hb::shared::sprite::BoundRect m_boundRect;
    uint32_t m_lastAccessTime;
};
