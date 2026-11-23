// sfml_sprite.cpp: SFML implementation of ISprite interface
//
// Part of SFMLEngine static library
//////////////////////////////////////////////////////////////////////

#include "sfml_sprite.h"
#include "sfml_renderer.h"
#include "common_types.h"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/RenderTexture.hpp>


SFMLSprite::SFMLSprite(SFMLRenderer* renderer, const std::string& pakFilePath, int spriteIndex, bool alphaEffect)
    : m_renderer(renderer)
    , m_pakFilePath(pakFilePath)
    , m_spriteIndex(spriteIndex)
    , m_bitmapWidth(0)
    , m_bitmapHeight(0)
    , m_textureLoaded(false)
    , m_alphaEffect(alphaEffect)
    , m_ambient_light_level(1)
    , m_inUse(false)
    , m_lastAccessTime(0)
{
    // Read only frame metadata from PAK — no image data loaded
    PAKLib::sprite metadata = PAKLib::get_sprite_metadata_fast(pakFilePath, spriteIndex);
    m_frames = std::move(metadata.sprite_rectangles);
}

SFMLSprite::SFMLSprite(SFMLRenderer* renderer, const std::vector<PAKLib::sprite_rect>& frames,
                       const std::string& pakFilePath, int spriteIndex, bool alphaEffect)
    : m_renderer(renderer)
    , m_pakFilePath(pakFilePath)
    , m_spriteIndex(spriteIndex)
    , m_frames(frames)
    , m_bitmapWidth(0)
    , m_bitmapHeight(0)
    , m_textureLoaded(false)
    , m_alphaEffect(alphaEffect)
    , m_ambient_light_level(1)
    , m_inUse(false)
    , m_lastAccessTime(0)
{
}

SFMLSprite::~SFMLSprite()
{
    Unload();
}

bool SFMLSprite::create_texture()
{
    if (m_textureLoaded)
        return true;

    if (m_imageData.empty())
        return false;

    // Load image data - SFML handles PNG natively with alpha channel support
    if (!m_collisionImage.loadFromMemory(m_imageData.data(), m_imageData.size()))
        return false;

    // get dimensions from loaded image
    sf::Vector2u size = m_collisionImage.getSize();
    m_bitmapWidth = static_cast<uint16_t>(size.x);
    m_bitmapHeight = static_cast<uint16_t>(size.y);

    // Create texture from image
    if (!m_texture.loadFromImage(m_collisionImage))
        return false;

    // Disable smooth filtering to prevent edge artifacts and ensure crisp pixel art
    m_texture.setSmooth(false);

    // Clear PNG data to free memory - we have the texture and collision image now
    m_imageData.clear();
    m_imageData.shrink_to_fit();

    m_textureLoaded = true;
    return true;
}

bool SFMLSprite::reload_from_pak()
{
    if (m_pakFilePath.empty())
        return false;

    PAKLib::sprite spriteData = PAKLib::get_sprite_fast(m_pakFilePath, m_spriteIndex);
    m_imageData = std::move(spriteData.image_data);

    return create_texture();
}

void SFMLSprite::draw(int x, int y, int frame, const hb::shared::sprite::DrawParams& params)
{
    // Lazy load texture on first draw, or reload from PAK after eviction
    if (!m_textureLoaded)
    {
        if (m_imageData.empty())
            reload_from_pak();
        else
            create_texture();
    }

    if (!m_textureLoaded || !m_renderer || frame < 0 || frame >= static_cast<int>(m_frames.size()))
        return;

    // Sync alpha degree with global (day/night mode)
    if (m_alphaEffect && m_ambient_light_level != m_renderer->get_ambient_light_level())
    {
        m_ambient_light_level = m_renderer->get_ambient_light_level();
    }

    m_inUse = true;
    m_lastAccessTime = GameClock::get_time_ms();

    DrawInternal(m_renderer->GetBackBuffer(), x, y, frame, params);

    m_inUse = false;
}

void SFMLSprite::DrawToSurface(void* destSurface, int x, int y, int frame, const hb::shared::sprite::DrawParams& params)
{
    // Lazy load texture on first draw, or reload from PAK after eviction
    if (!m_textureLoaded)
    {
        if (m_imageData.empty())
            reload_from_pak();
        else
            create_texture();
    }

    if (!destSurface || !m_textureLoaded || frame < 0 || frame >= static_cast<int>(m_frames.size()))
        return;

    // Sync alpha degree with global (day/night mode)
    if (m_alphaEffect && m_ambient_light_level != m_renderer->get_ambient_light_level())
    {
        m_ambient_light_level = m_renderer->get_ambient_light_level();
    }

    m_inUse = true;
    m_lastAccessTime = GameClock::get_time_ms();

    sf::RenderTexture* target = static_cast<sf::RenderTexture*>(destSurface);
    DrawInternal(target, x, y, frame, params);

    m_inUse = false;
}

void SFMLSprite::DrawInternal(sf::RenderTexture* target, int x, int y, int frame, const hb::shared::sprite::DrawParams& params)
{
    if (!target)
        return;

    const PAKLib::sprite_rect& frameRect = m_frames[frame];

    // Shadow rendering - replicate DDraw's skewed shadow effect
    if (params.m_shadow)
    {
        // DDraw shadow algorithm:
        // - Reads from bottom of sprite going up
        // - Every 3 source rows -> 1 shadow row (1/3 height)
        // - Skews left by 1 pixel per row going UP
        // - Shadow base (bottom) is anchored at sprite's feet

        int drawX = x + frameRect.pivotX;
        int drawY = y + frameRect.pivotY;

        // Create sprite for this frame
        sf::IntRect srcRect(
            {static_cast<int>(frameRect.x), static_cast<int>(frameRect.y)},
            {static_cast<int>(frameRect.width), static_cast<int>(frameRect.height)}
        );

        sf::Sprite sprite(m_texture, srcRect);

        // Semi-transparent black for shadow
        // At night (m_renderer->get_ambient_light_level() == 2), reduce shadow opacity by 50%
        uint8_t shadowAlpha = (m_renderer->get_ambient_light_level() == 2) ? 70 : 140;
        sprite.setColor(sf::Color(0, 0, 0, shadowAlpha));

        // Set origin to bottom-left of sprite so transforms anchor there (at feet)
        sprite.setOrigin({0.0f, static_cast<float>(frameRect.height)});

        // Shadow dimensions
        float scaleY = 1.0f / 3.0f;
        float shadowHeight = frameRect.height * scaleY;

        // Position: shadow base at sprite's feet (bottom of sprite)
        float baseX = static_cast<float>(drawX);
        float baseY = static_cast<float>(drawY + frameRect.height);

        // Build transform:
        // 1. Translate to feet position
        // 2. Apply shear (skew left going up) and vertical scale
        //
        // The shear matrix skews X based on Y. Since origin is at bottom,
        // and Y goes negative (upward), we want positive shear to skew left.

        sf::Transform transform;
        transform.translate({baseX, baseY});

        // Shear and scale matrix (origin at bottom-left):
        // x' = x + shearX * y
        // y' = scaleY * y
        // Since y is negative (going up from origin), positive shearX moves left
        sf::Transform skewScale(
            1.0f, 1.0f, 0.0f,   // x' = x + 1*y (shear)
            0.0f, scaleY, 0.0f, // y' = scaleY * y
            0.0f, 0.0f, 1.0f
        );
        transform.combine(skewScale);

        sf::RenderStates states;
        states.transform = transform;
        states.blendMode = sf::BlendAlpha;

        target->draw(sprite, states);

        // Update bounds (approximate)
        m_boundRect.left = drawX;
        m_boundRect.top = static_cast<int>(baseY - shadowHeight);
        m_boundRect.right = drawX + frameRect.width;
        m_boundRect.bottom = static_cast<int>(baseY);

        return;
    }

    // Calculate draw position — apply pivot unless caller opted out
    int drawX = params.m_ignore_pivot ? x : x + frameRect.pivotX;
    int drawY = params.m_ignore_pivot ? y : y + frameRect.pivotY;

    // Update bounds
    m_boundRect.left = drawX;
    m_boundRect.top = drawY;
    m_boundRect.right = drawX + frameRect.width;
    m_boundRect.bottom = drawY + frameRect.height;

    // Clip to render target's visible view bounds (world-space coordinates).
    // Using view center/size accounts for views with non-zero origins (e.g. detail LOD).
    sf::View currentView = target->getView();
    sf::Vector2f viewCenter = currentView.getCenter();
    sf::Vector2f viewSize = currentView.getSize();
    int clipLeft = static_cast<int>(viewCenter.x - viewSize.x / 2.0f);
    int clipTop = static_cast<int>(viewCenter.y - viewSize.y / 2.0f);
    int clipRight = static_cast<int>(viewCenter.x + viewSize.x / 2.0f);
    int clipBottom = static_cast<int>(viewCenter.y + viewSize.y / 2.0f);

    int srcX = frameRect.x;
    int srcY = frameRect.y;
    int srcW = frameRect.width;
    int srcH = frameRect.height;

    // Clip left edge
    if (drawX < clipLeft)
    {
        int diff = clipLeft - drawX;
        srcX += diff;
        srcW -= diff;
        drawX = clipLeft;
    }
    // Clip top edge
    if (drawY < clipTop)
    {
        int diff = clipTop - drawY;
        srcY += diff;
        srcH -= diff;
        drawY = clipTop;
    }
    // Clip right edge
    if (drawX + srcW > clipRight)
    {
        srcW = clipRight - drawX;
    }
    // Clip bottom edge
    if (drawY + srcH > clipBottom)
    {
        srcH = clipBottom - drawY;
    }

    // Skip if completely clipped
    if (srcW <= 0 || srcH <= 0)
        return;

    // Create sprite for this frame with clipped source rect
    sf::IntRect srcRect(
        {srcX, srcY},
        {srcW, srcH}
    );

    sf::Sprite sprite(m_texture, srcRect);
    sprite.setPosition({static_cast<float>(drawX), static_cast<float>(drawY)});

    // Check if we need any color modifications
    // Skip multiplicative tint for AdditiveOffset — shader handles the color offset
    bool is_additive_offset = (params.m_blend_mode == hb::shared::sprite::BlendMode::AdditiveOffset);
    bool needs_color_change = (params.m_alpha < 1.0f) ||
                            (params.m_has_tint && !is_additive_offset) ||
                            params.m_shadow || params.m_fade ||
                            (m_ambient_light_level == 2 && m_alphaEffect);

    if (needs_color_change)
    {
        sf::Color color = sf::Color::White;

        // Apply alpha
        if (params.m_alpha < 1.0f)
        {
            color.a = static_cast<uint8_t>(params.m_alpha * 255.0f);
        }

        // Apply tint: SFML multiplies sprite pixels by color/255
        // (255,255,255) = no change, (0,0,0) = black, (255,200,0) = gold
        // Skipped for AdditiveOffset — tint is passed to shader as additive offset
        if (params.m_has_tint && !is_additive_offset)
        {
            int r = params.m_tint_r;
            int g = params.m_tint_g;
            int b = params.m_tint_b;

            // Clamp to valid range
            color.r = static_cast<uint8_t>(r < 0 ? 0 : (r > 255 ? 255 : r));
            color.g = static_cast<uint8_t>(g < 0 ? 0 : (g > 255 ? 255 : g));
            color.b = static_cast<uint8_t>(b < 0 ? 0 : (b > 255 ? 255 : b));
        }

        // Apply shadow effect (darken)
        // Use semi-transparent black with alpha blending - the sprite's own alpha
        // channel will mask where the shadow appears (only on non-transparent pixels)
        if (params.m_shadow)
        {
            color.r = 0;
            color.g = 0;
            color.b = 0;
            color.a = 64;  // Semi-transparent black for shadow darkening
        }

        // Apply fade effect - darkens destination like DDraw's DrawFadeInternal
        // At night (m_renderer->get_ambient_light_level() == 2), reduce fade intensity by 50%
        // Day: 75% alpha -> 25% brightness | Night: 50% alpha -> 50% brightness
        if (params.m_fade)
        {
            color.r = 0;
            color.g = 0;
            color.b = 0;
            color.a = (m_renderer->get_ambient_light_level() == 2) ? 127 : 191;  // Less darkening at night
        }

        // Apply alpha degree darkening (night mode)
        if (m_ambient_light_level == 2 && m_alphaEffect)
        {
            color.r = static_cast<uint8_t>(color.r * 0.7f);
            color.g = static_cast<uint8_t>(color.g * 0.7f);
            color.b = static_cast<uint8_t>(color.b * 0.7f);
        }

        sprite.setColor(color);
    }

    // Set up render states based on blend mode
    sf::RenderStates states;
    bool wasSmooth = m_texture.isSmooth();
    bool needSmoothRestore = false;

    if (params.m_blend_mode == hb::shared::sprite::BlendMode::AdditiveOffset) {
        // Additive blend with per-pixel color offset via fragment shader
        // Matches DDraw PutTransSpriteRGB: dest += clamp(src + (r, g, b))
        states.blendMode = sf::BlendAdd;
        if (!wasSmooth && !params.m_nearest_filter) {
            m_texture.setSmooth(true);
            needSmoothRestore = true;
        }

        const sf::Shader* shader = m_renderer->get_additive_offset_shader();
        if (shader)
        {
            auto* mutableShader = const_cast<sf::Shader*>(shader);
            mutableShader->setUniform("colorOffset", sf::Glsl::Vec3(
                params.m_tint_r / 255.0f,
                params.m_tint_g / 255.0f,
                params.m_tint_b / 255.0f));
            states.shader = shader;
        }

        // Shader handles the color offset — only set alpha via sprite color if needed
        if (params.m_alpha < 1.0f)
        {
            sf::Color c = sf::Color::White;
            c.a = static_cast<uint8_t>(params.m_alpha * 255.0f);
            sprite.setColor(c);
        }
        else
        {
            sprite.setColor(sf::Color::White);
        }
    } else if (params.m_blend_mode == hb::shared::sprite::BlendMode::Additive) {
        states.blendMode = sf::BlendAdd;
        // Enable smooth filtering for additive blending (light effects)
        // This produces smooth gradients like DDraw's per-pixel blending.
        // Skip for bitmap fonts (m_nearest_filter) — bilinear filtering causes
        // glyph edge bleed artifacts, especially on Linux/OpenGL.
        if (!wasSmooth && !params.m_nearest_filter) {
            m_texture.setSmooth(true);
            needSmoothRestore = true;
        }
    } else if (params.m_blend_mode == hb::shared::sprite::BlendMode::Average) {
        // 50/50 averaging: result = (src + dst) / 2
        // Achieved with alpha blending at 50% opacity
        states.blendMode = sf::BlendAlpha;
        sf::Color color = sprite.getColor();
        color.a = 127;  // 50% alpha for averaging blend
        sprite.setColor(color);
    } else if (params.m_blend_mode == hb::shared::sprite::BlendMode::Multiply) {
        // Multiply blend: result = src * dst (darkening effect)
        // Dark sprite pixels darken the background, white pixels preserve it
        // Used for dark fog, shadows, ground darkening (DDraw PutDarkSprite)
        states.blendMode = sf::BlendMultiply;
    } else {
        states.blendMode = sf::BlendAlpha;
    }

    // draw the sprite
    target->draw(sprite, states);

    // Restore smooth setting if we changed it
    if (needSmoothRestore) {
        m_texture.setSmooth(false);
    }
}

void SFMLSprite::DrawWidth(int x, int y, int frame, int width, bool vertical)
{
    // Lazy load texture on first draw, or reload from PAK after eviction
    if (!m_textureLoaded)
    {
        if (m_imageData.empty())
            reload_from_pak();
        else
            create_texture();
    }

    if (!m_textureLoaded || !m_renderer || frame < 0 || frame >= static_cast<int>(m_frames.size()))
        return;

    // Sync alpha degree with global (day/night mode)
    if (m_alphaEffect && m_ambient_light_level != m_renderer->get_ambient_light_level())
    {
        m_ambient_light_level = m_renderer->get_ambient_light_level();
    }

    m_inUse = true;
    m_lastAccessTime = GameClock::get_time_ms();

    const PAKLib::sprite_rect& frameRect = m_frames[frame];

    int drawX = x + frameRect.pivotX;
    int drawY = y + frameRect.pivotY;

    // Clamp width
    int maxSize = vertical ? frameRect.height : frameRect.width;
    if (width > maxSize) width = maxSize;
    if (width < 0) width = 0;

    // Create partial source rect
    sf::IntRect srcRect;
    if (vertical)
    {
        srcRect = sf::IntRect(
            {static_cast<int>(frameRect.x), static_cast<int>(frameRect.y)},
            {static_cast<int>(frameRect.width), width}
        );
    }
    else
    {
        srcRect = sf::IntRect(
            {static_cast<int>(frameRect.x), static_cast<int>(frameRect.y)},
            {width, static_cast<int>(frameRect.height)}
        );
    }

    sf::Sprite sprite(m_texture, srcRect);
    sprite.setPosition({static_cast<float>(drawX), static_cast<float>(drawY)});

    // Use explicit alpha blending to respect PNG transparency
    m_renderer->GetBackBuffer()->draw(sprite, sf::RenderStates(sf::BlendAlpha));

    // Update bounds
    m_boundRect.left = drawX;
    m_boundRect.top = drawY;
    m_boundRect.right = drawX + (vertical ? frameRect.width : width);
    m_boundRect.bottom = drawY + (vertical ? width : frameRect.height);

    m_inUse = false;
}

void SFMLSprite::DrawShifted(int x, int y, int shiftX, int shiftY, int frame, const hb::shared::sprite::DrawParams& params)
{
    // DrawShifted draws a 128x128 subregion of the sprite starting at (shiftX, shiftY)
    // This is used for the guide map to show a viewport window into a large map sprite
    // Matches DDraw's CSprite::PutShiftSpriteFast / PutShiftTransSprite2

    // Lazy load texture on first draw, or reload from PAK after eviction
    if (!m_textureLoaded)
    {
        if (m_imageData.empty())
            reload_from_pak();
        else
            create_texture();
    }

    if (!m_textureLoaded || !m_renderer || frame < 0 || frame >= static_cast<int>(m_frames.size()))
        return;

    // Sync alpha degree with global (day/night mode)
    if (m_alphaEffect && m_ambient_light_level != m_renderer->get_ambient_light_level())
    {
        m_ambient_light_level = m_renderer->get_ambient_light_level();
    }

    sf::RenderTexture* target = m_renderer->GetBackBuffer();
    if (!target)
        return;

    m_inUse = true;

    const PAKLib::sprite_rect& frameRect = m_frames[frame];

    // Source position within the sprite texture - start at frame origin and add shift
    int srcX = frameRect.x + shiftX;
    int srcY = frameRect.y + shiftY;
    int srcW = 128;  // Guide map viewport is always 128x128
    int srcH = 128;

    // Destination position with pivot offsets (matching DDraw implementation)
    int drawX = x + frameRect.pivotX;
    int drawY = y + frameRect.pivotY;

    // get render target size for clipping
    sf::Vector2u targetSize = target->getSize();
    int targetWidth = static_cast<int>(targetSize.x);
    int targetHeight = static_cast<int>(targetSize.y);

    // Clip to screen boundaries (matching DDraw's clipping logic)
    if (drawX < 0)
    {
        int diff = -drawX;
        srcX += diff;
        srcW -= diff;
        if (srcW <= 0) { m_inUse = false; return; }
        drawX = 0;
    }
    else if (drawX + srcW > targetWidth)
    {
        srcW -= (drawX + srcW) - targetWidth;
        if (srcW <= 0) { m_inUse = false; return; }
    }

    if (drawY < 0)
    {
        int diff = -drawY;
        srcY += diff;
        srcH -= diff;
        if (srcH <= 0) { m_inUse = false; return; }
        drawY = 0;
    }
    else if (drawY + srcH > targetHeight)
    {
        srcH -= (drawY + srcH) - targetHeight;
        if (srcH <= 0) { m_inUse = false; return; }
    }

    // Update bounds
    m_boundRect.left = drawX;
    m_boundRect.top = drawY;
    m_boundRect.right = drawX + srcW;
    m_boundRect.bottom = drawY + srcH;

    // Create sprite with the shifted source rectangle
    sf::IntRect srcRect(
        {srcX, srcY},
        {srcW, srcH}
    );
    sf::Sprite sprite(m_texture, srcRect);

    // Apply alpha if specified
    if (params.m_alpha < 1.0f)
    {
        std::uint8_t alphaVal = static_cast<std::uint8_t>(params.m_alpha * 255.0f);
        sprite.setColor(sf::Color(255, 255, 255, alphaVal));
    }

    // Position at destination
    sprite.setPosition({static_cast<float>(drawX), static_cast<float>(drawY)});

    // draw to target
    sf::RenderStates states;
    states.blendMode = params.m_alpha < 1.0f ? sf::BlendAlpha : sf::BlendNone;
    target->draw(sprite, states);

    m_inUse = false;
}

int SFMLSprite::GetFrameCount() const
{
    return static_cast<int>(m_frames.size());
}

hb::shared::sprite::SpriteRect SFMLSprite::GetFrameRect(int frame) const
{
    if (frame < 0 || frame >= static_cast<int>(m_frames.size()))
    {
        return hb::shared::sprite::SpriteRect{0, 0, 0, 0, 0, 0};
    }

    const PAKLib::sprite_rect& src = m_frames[frame];
    hb::shared::sprite::SpriteRect rect;
    rect.x = src.x;
    rect.y = src.y;
    rect.width = src.width;
    rect.height = src.height;
    rect.pivotX = src.pivotX;
    rect.pivotY = src.pivotY;
    return rect;
}

void SFMLSprite::GetBoundingRect(int x, int y, int frame, int& left, int& top, int& right, int& bottom)
{
    if (frame < 0 || frame >= static_cast<int>(m_frames.size()))
    {
        left = top = right = bottom = 0;
        return;
    }

    const PAKLib::sprite_rect& frameRect = m_frames[frame];

    left = x + frameRect.pivotX;
    top = y + frameRect.pivotY;
    right = left + frameRect.width;
    bottom = top + frameRect.height;
}

void SFMLSprite::CalculateBounds(int x, int y, int frame)
{
    if (frame < 0 || frame >= static_cast<int>(m_frames.size()))
    {
        m_boundRect = hb::shared::sprite::BoundRect{};
        return;
    }

    const PAKLib::sprite_rect& frameRect = m_frames[frame];

    m_boundRect.left = x + frameRect.pivotX;
    m_boundRect.top = y + frameRect.pivotY;
    m_boundRect.right = m_boundRect.left + frameRect.width;
    m_boundRect.bottom = m_boundRect.top + frameRect.height;
}

bool SFMLSprite::GetLastDrawBounds(int& left, int& top, int& right, int& bottom) const
{
    if (!m_boundRect.IsValid())
        return false;

    left = m_boundRect.left;
    top = m_boundRect.top;
    right = m_boundRect.right;
    bottom = m_boundRect.bottom;
    return true;
}

hb::shared::sprite::BoundRect SFMLSprite::GetBoundRect() const
{
    return m_boundRect;
}

bool SFMLSprite::CheckCollision(int spriteX, int spriteY, int frame, int pointX, int pointY)
{
    if (frame < 0 || frame >= static_cast<int>(m_frames.size()))
        return false;

    const PAKLib::sprite_rect& frameRect = m_frames[frame];

    int left = spriteX + frameRect.pivotX;
    int top = spriteY + frameRect.pivotY;
    int right = left + frameRect.width;
    int bottom = top + frameRect.height;

    // Bounding box check
    if (pointX < left || pointX >= right || pointY < top || pointY >= bottom)
        return false;

    // Reload texture if evicted (needed for pixel-perfect collision image)
    if (!m_textureLoaded)
    {
        if (m_imageData.empty())
            reload_from_pak();
        else
            create_texture();
    }

    // Pixel-perfect check using cached image alpha channel
    if (m_collisionImage.getSize().x == 0)
        return true; // Fallback to bounding box if image not available

    int localX = pointX - left;
    int localY = pointY - top;
    sf::Color pixel = m_collisionImage.getPixel({
        static_cast<unsigned int>(frameRect.x + localX),
        static_cast<unsigned int>(frameRect.y + localY)
    });
    return pixel.a > 0;
}

void SFMLSprite::Preload()
{
    if (!m_textureLoaded)
    {
        if (m_imageData.empty())
            reload_from_pak();
        else
            create_texture();
    }
}

void SFMLSprite::Unload()
{
    if (!m_textureLoaded)
        return;

    // Release GPU texture — replace with default (empty) texture
    m_texture = sf::Texture();

    // Release collision image
    m_collisionImage = sf::Image();

    // Clear any lingering image data (should already be empty after create_texture)
    m_imageData.clear();
    m_imageData.shrink_to_fit();

    // Reset dimensions
    m_bitmapWidth = 0;
    m_bitmapHeight = 0;

    m_textureLoaded = false;

    // Keep: m_frames, m_pakFilePath, m_spriteIndex — needed for reload
}

bool SFMLSprite::IsLoaded() const
{
    return m_textureLoaded;
}

void SFMLSprite::Restore()
{
    // Recreate texture if needed (e.g. after device loss)
    if (!m_textureLoaded)
    {
        if (m_imageData.empty())
            reload_from_pak();
        else
            create_texture();
    }
}

bool SFMLSprite::IsInUse() const
{
    return m_inUse;
}

uint32_t SFMLSprite::GetLastAccessTime() const
{
    return m_lastAccessTime;
}

void SFMLSprite::set_ambient_light_level(char level)
{
    if (m_ambient_light_level != level && m_alphaEffect)
    {
        m_ambient_light_level = level;
        ApplyAmbientLightLevel();
    }
}

void SFMLSprite::ApplyAmbientLightLevel()
{
    // Alpha degree is now applied at draw time via sprite color
    // No need to modify the texture
}
