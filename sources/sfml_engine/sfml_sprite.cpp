// sfml_sprite.cpp: SFML implementation of ISprite interface
//
// Part of SFMLEngine static library
//////////////////////////////////////////////////////////////////////

#include "sfml_sprite.h"
#include "sfml_renderer.h"
#include "common_types.h"
#include "sprite_folder.h"
#include "log.h"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <algorithm>
#include <cmath>

// Visible bounds of the target's current view, in logical coordinates. Using view center/size
// accounts for views with non-zero origins (e.g. detail LOD) and for the window's UI view,
// whose logical size differs from the window's pixel size.
static hb::shared::sprite::BoundRect get_view_bounds(const sf::RenderTarget& target)
{
    const sf::View& view = target.getView();
    sf::Vector2f center = view.getCenter();
    sf::Vector2f size = view.getSize();
    hb::shared::sprite::BoundRect bounds;
    bounds.left = static_cast<int>(center.x - size.x / 2.0f);
    bounds.top = static_cast<int>(center.y - size.y / 2.0f);
    bounds.right = static_cast<int>(center.x + size.x / 2.0f);
    bounds.bottom = static_cast<int>(center.y + size.y / 2.0f);
    return bounds;
}

// Smooths a sprite texture for one screen-space backdrop draw, then restores it. Only a draw whose
// destination (logical units) covers the whole UI area smooths: bilinear sampling reads half a texel
// outside the source rect, which bleeds neighbouring frames of packed sheets and seams sliced chrome,
// so every other sprite stays nearest. Also needs renderer->wants_ui_smoothing() (screen space,
// upscaleSmoothing on, UI scaled) and no m_nearest_filter (bitmap fonts).
class scoped_ui_smoothing
{
public:
    scoped_ui_smoothing(sf::Texture& texture, const SFMLRenderer* renderer, bool nearest_filter,
                        int dest_x, int dest_y, int dest_w, int dest_h)
        : m_texture(texture)
        , m_active(!nearest_filter && !texture.isSmooth() && renderer && renderer->wants_ui_smoothing()
                   && dest_x <= 0 && dest_y <= 0
                   && dest_x + dest_w >= renderer->get_width() && dest_y + dest_h >= renderer->get_height())
    {
        if (m_active)
            m_texture.setSmooth(true);
    }

    ~scoped_ui_smoothing()
    {
        if (m_active)
            m_texture.setSmooth(false);
    }

    scoped_ui_smoothing(const scoped_ui_smoothing&) = delete;
    scoped_ui_smoothing& operator=(const scoped_ui_smoothing&) = delete;

private:
    sf::Texture& m_texture;
    bool m_active;
};

// A high-resolution frame (image pixels) in logical pixels: sizes and pivots divided by the art scale
// and rounded, the position floored (only meaningful to the image, which the sprite reads in image pixels)
static hb::shared::sprite::SpriteRect to_logical(const hb::shared::sprite::SpriteRect& art, int scale)
{
    const auto divide = [scale](int value) { return static_cast<int>(std::lround(static_cast<double>(value) / scale)); };
    hb::shared::sprite::SpriteRect logical;
    logical.x = static_cast<uint16_t>(art.x / scale);
    logical.y = static_cast<uint16_t>(art.y / scale);
    logical.width = static_cast<uint16_t>(art.width > 0 ? std::max(1, divide(art.width)) : 0);
    logical.height = static_cast<uint16_t>(art.height > 0 ? std::max(1, divide(art.height)) : 0);
    logical.pivotX = static_cast<int16_t>(divide(art.pivotX));
    logical.pivotY = static_cast<int16_t>(divide(art.pivotY));
    return logical;
}

SFMLSprite::SFMLSprite(SFMLRenderer* renderer, const std::vector<hb::shared::sprite::SpriteRect>& frames,
                       const std::string& image_path, bool alphaEffect, int art_scale)
    : m_renderer(renderer)
    , m_image_path(image_path)
    , m_frames(frames)
    , m_art_scale(std::max(1, art_scale))
    , m_bitmapWidth(0)
    , m_bitmapHeight(0)
    , m_textureLoaded(false)
    , m_load_failed(false)
    , m_alphaEffect(alphaEffect)
    , m_ambient_light_level(1)
    , m_inUse(false)
    , m_lastAccessTime(0)
{
    if (m_art_scale > 1)
    {
        m_art_frames = frames;
        for (hb::shared::sprite::SpriteRect& frame : m_frames)
            frame = to_logical(frame, m_art_scale);
    }
}

SFMLSprite::~SFMLSprite()
{
    Unload();
}

bool SFMLSprite::load_texture()
{
    if (m_textureLoaded)
        return true;

    // An empty sprite has no image; a failed one is not retried on every draw
    if (m_image_path.empty() || m_load_failed)
        return false;

    // SFML detects the image format (PNG, BMP, JPEG) from the bytes
    const std::vector<uint8_t> imageData = hb::shared::sprite::read_file_bytes(m_image_path);
    if (imageData.empty() || !m_collisionImage.loadFromMemory(imageData.data(), imageData.size())
        || !m_texture.loadFromImage(m_collisionImage))
    {
        m_load_failed = true;
        m_collisionImage = sf::Image();
        hb::logger::warn("Sprite image failed to load, not retried until unloaded: {}", m_image_path);
        return false;
    }

    // get dimensions from loaded image
    sf::Vector2u size = m_collisionImage.getSize();
    m_bitmapWidth = static_cast<uint16_t>(size.x);
    m_bitmapHeight = static_cast<uint16_t>(size.y);

    // Pixel art stays nearest (crisp, no edge bleed). A high-resolution sheet is always drawn scaled,
    // so it samples smooth with mipmaps; its frames are packed with gaps for that (HH-0083).
    // Mipmaps are a quality improvement only; without them the sheet still draws.
    m_texture.setSmooth(m_art_scale > 1);
    if (m_art_scale > 1)
    {
        [[maybe_unused]] const bool mipmapped = m_texture.generateMipmap();
    }

    m_textureLoaded = true;
    return true;
}

void SFMLSprite::draw(int x, int y, int frame, const hb::shared::sprite::DrawParams& params)
{
    // Lazy load texture on first draw, or reload after Unload()
    if (!m_textureLoaded)
        load_texture();

    if (!m_textureLoaded || !m_renderer || frame < 0 || frame >= static_cast<int>(m_frames.size()))
        return;

    // Sync alpha degree with global (day/night mode)
    if (m_alphaEffect && m_ambient_light_level != m_renderer->get_ambient_light_level())
    {
        m_ambient_light_level = m_renderer->get_ambient_light_level();
    }

    m_inUse = true;
    m_lastAccessTime = GameClock::get_time_ms();

    DrawInternal(m_renderer->get_current_target(), x, y, frame, params);

    m_inUse = false;
}

void SFMLSprite::DrawToSurface(void* destSurface, int x, int y, int frame, const hb::shared::sprite::DrawParams& params)
{
    // Lazy load texture on first draw, or reload after Unload()
    if (!m_textureLoaded)
        load_texture();

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

void SFMLSprite::DrawInternal(sf::RenderTarget* target, int x, int y, int frame, const hb::shared::sprite::DrawParams& params)
{
    if (!target)
        return;

    const hb::shared::sprite::SpriteRect& frameRect = m_frames[frame];
    const hb::shared::sprite::SpriteRect& art = art_rect(frame);
    const float inv_scale = 1.0f / static_cast<float>(m_art_scale);
    const sf::IntRect art_src(
        {static_cast<int>(art.x), static_cast<int>(art.y)},
        {static_cast<int>(art.width), static_cast<int>(art.height)}
    );

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

        // Create sprite for this frame (image pixels, scaled to logical size)
        sf::Sprite sprite(m_texture, art_src);
        sprite.setScale({inv_scale, inv_scale});

        // Semi-transparent black for shadow
        // At night (m_renderer->get_ambient_light_level() == 2), reduce shadow opacity by 50%
        uint8_t shadowAlpha = (m_renderer->get_ambient_light_level() == 2) ? 70 : 140;
        sprite.setColor(sf::Color(0, 0, 0, shadowAlpha));

        // Set origin to bottom-left of sprite so transforms anchor there (at feet); local, image pixels
        sprite.setOrigin({0.0f, static_cast<float>(art.height)});

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

    // Clip to render target's visible view bounds (logical coordinates)
    hb::shared::sprite::BoundRect view = get_view_bounds(*target);

    sf::Sprite sprite(m_texture);
    if (m_art_scale > 1)
    {
        // High-resolution sheet: the whole frame at its exact logical size and pivot (sub-pixel), clipped
        // by the target; only a frame wholly outside the view is skipped
        if (m_boundRect.right <= view.left || m_boundRect.left >= view.right
            || m_boundRect.bottom <= view.top || m_boundRect.top >= view.bottom)
            return;
        const float fx = params.m_ignore_pivot ? static_cast<float>(x) : x + art.pivotX * inv_scale;
        const float fy = params.m_ignore_pivot ? static_cast<float>(y) : y + art.pivotY * inv_scale;
        sprite.setTextureRect(art_src);
        sprite.setScale({inv_scale, inv_scale});
        sprite.setPosition({fx, fy});
    }
    else
    {
        int srcX = frameRect.x;
        int srcY = frameRect.y;
        int srcW = frameRect.width;
        int srcH = frameRect.height;

        // Clip left edge
        if (drawX < view.left)
        {
            int diff = view.left - drawX;
            srcX += diff;
            srcW -= diff;
            drawX = view.left;
        }
        // Clip top edge
        if (drawY < view.top)
        {
            int diff = view.top - drawY;
            srcY += diff;
            srcH -= diff;
            drawY = view.top;
        }
        // Clip right edge
        if (drawX + srcW > view.right)
        {
            srcW = view.right - drawX;
        }
        // Clip bottom edge
        if (drawY + srcH > view.bottom)
        {
            srcH = view.bottom - drawY;
        }

        // Skip if completely clipped
        if (srcW <= 0 || srcH <= 0)
            return;

        sprite.setTextureRect(sf::IntRect({srcX, srcY}, {srcW, srcH}));
        sprite.setPosition({static_cast<float>(drawX), static_cast<float>(drawY)});
    }

    // Backdrop smoothing; set before the blend-mode code reads isSmooth(), so it neither re-toggles nor
    // restores early. A high-resolution sheet is already smooth, so this leaves it alone.
    const sf::FloatRect dest = sprite.getGlobalBounds();
    scoped_ui_smoothing ui_smoothing(m_texture, m_renderer, params.m_nearest_filter,
                                     static_cast<int>(dest.position.x), static_cast<int>(dest.position.y),
                                     static_cast<int>(dest.size.x), static_cast<int>(dest.size.y));

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
    // Lazy load texture on first draw, or reload after Unload()
    if (!m_textureLoaded)
        load_texture();

    if (!m_textureLoaded || !m_renderer || frame < 0 || frame >= static_cast<int>(m_frames.size()))
        return;

    // Sync alpha degree with global (day/night mode)
    if (m_alphaEffect && m_ambient_light_level != m_renderer->get_ambient_light_level())
    {
        m_ambient_light_level = m_renderer->get_ambient_light_level();
    }

    m_inUse = true;
    m_lastAccessTime = GameClock::get_time_ms();

    const hb::shared::sprite::SpriteRect& frameRect = m_frames[frame];
    const hb::shared::sprite::SpriteRect& art = art_rect(frame);

    int drawX = x + frameRect.pivotX;
    int drawY = y + frameRect.pivotY;

    // Clamp width (logical pixels)
    int maxSize = vertical ? frameRect.height : frameRect.width;
    if (width > maxSize) width = maxSize;
    if (width < 0) width = 0;

    // Create partial source rect, in image pixels (art scale 1: the logical rect)
    const int art_width = std::min(width * m_art_scale, static_cast<int>(vertical ? art.height : art.width));
    sf::IntRect srcRect;
    if (vertical)
    {
        srcRect = sf::IntRect(
            {static_cast<int>(art.x), static_cast<int>(art.y)},
            {static_cast<int>(art.width), art_width}
        );
    }
    else
    {
        srcRect = sf::IntRect(
            {static_cast<int>(art.x), static_cast<int>(art.y)},
            {art_width, static_cast<int>(art.height)}
        );
    }

    const float inv_scale = 1.0f / static_cast<float>(m_art_scale);
    sf::Sprite sprite(m_texture, srcRect);
    sprite.setScale({inv_scale, inv_scale});
    sprite.setPosition({x + art.pivotX * inv_scale, y + art.pivotY * inv_scale});

    // Use explicit alpha blending to respect PNG transparency
    if (sf::RenderTarget* target = m_renderer->get_current_target())
    {
        const sf::FloatRect dest = sprite.getGlobalBounds();
        scoped_ui_smoothing ui_smoothing(m_texture, m_renderer, false,
                                         drawX, drawY, static_cast<int>(dest.size.x), static_cast<int>(dest.size.y));
        target->draw(sprite, sf::RenderStates(sf::BlendAlpha));
    }

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

    // Lazy load texture on first draw, or reload after Unload()
    if (!m_textureLoaded)
        load_texture();

    if (!m_textureLoaded || !m_renderer || frame < 0 || frame >= static_cast<int>(m_frames.size()))
        return;

    // Sync alpha degree with global (day/night mode)
    if (m_alphaEffect && m_ambient_light_level != m_renderer->get_ambient_light_level())
    {
        m_ambient_light_level = m_renderer->get_ambient_light_level();
    }

    sf::RenderTarget* target = m_renderer->get_current_target();
    if (!target)
        return;

    m_inUse = true;

    const hb::shared::sprite::SpriteRect& frameRect = m_frames[frame];

    // Source offset within the frame and its size, in logical pixels; the source rect below is in image pixels
    int srcX = shiftX;
    int srcY = shiftY;
    int srcW = 128;  // Guide map viewport is always 128x128
    int srcH = 128;

    // Destination position with pivot offsets (matching DDraw implementation)
    int drawX = x + frameRect.pivotX;
    int drawY = y + frameRect.pivotY;

    // Clip to the target's logical view bounds (matching DDraw's clipping logic); the window's
    // pixel size is not the logical size, so never clip against target->getSize()
    hb::shared::sprite::BoundRect view = get_view_bounds(*target);

    if (drawX < view.left)
    {
        int diff = view.left - drawX;
        srcX += diff;
        srcW -= diff;
        if (srcW <= 0) { m_inUse = false; return; }
        drawX = view.left;
    }
    else if (drawX + srcW > view.right)
    {
        srcW -= (drawX + srcW) - view.right;
        if (srcW <= 0) { m_inUse = false; return; }
    }

    if (drawY < view.top)
    {
        int diff = view.top - drawY;
        srcY += diff;
        srcH -= diff;
        if (srcH <= 0) { m_inUse = false; return; }
        drawY = view.top;
    }
    else if (drawY + srcH > view.bottom)
    {
        srcH -= (drawY + srcH) - view.bottom;
        if (srcH <= 0) { m_inUse = false; return; }
    }

    // Update bounds
    m_boundRect.left = drawX;
    m_boundRect.top = drawY;
    m_boundRect.right = drawX + srcW;
    m_boundRect.bottom = drawY + srcH;

    // Create sprite with the shifted source rectangle, in image pixels, drawn at its logical size
    const hb::shared::sprite::SpriteRect& art = art_rect(frame);
    const float inv_scale = 1.0f / static_cast<float>(m_art_scale);
    sf::IntRect srcRect(
        {art.x + srcX * m_art_scale, art.y + srcY * m_art_scale},
        {srcW * m_art_scale, srcH * m_art_scale}
    );
    sf::Sprite sprite(m_texture, srcRect);
    sprite.setScale({inv_scale, inv_scale});

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
    scoped_ui_smoothing ui_smoothing(m_texture, m_renderer, params.m_nearest_filter, drawX, drawY, srcW, srcH);
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

    return m_frames[frame];
}

void SFMLSprite::GetBoundingRect(int x, int y, int frame, int& left, int& top, int& right, int& bottom)
{
    if (frame < 0 || frame >= static_cast<int>(m_frames.size()))
    {
        left = top = right = bottom = 0;
        return;
    }

    const hb::shared::sprite::SpriteRect& frameRect = m_frames[frame];

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

    const hb::shared::sprite::SpriteRect& frameRect = m_frames[frame];

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

    const hb::shared::sprite::SpriteRect& frameRect = m_frames[frame];

    int left = spriteX + frameRect.pivotX;
    int top = spriteY + frameRect.pivotY;
    int right = left + frameRect.width;
    int bottom = top + frameRect.height;

    // Bounding box check
    if (pointX < left || pointX >= right || pointY < top || pointY >= bottom)
        return false;

    // Reload texture if evicted (needed for pixel-perfect collision image)
    if (!m_textureLoaded)
        load_texture();

    // Pixel-perfect check using cached image alpha channel
    if (m_collisionImage.getSize().x == 0)
        return true; // Fallback to bounding box if image not available

    // Logical offset into the frame, read from the image at the art scale
    const hb::shared::sprite::SpriteRect& art = art_rect(frame);
    const int artX = std::min((pointX - left) * m_art_scale, art.width - 1);
    const int artY = std::min((pointY - top) * m_art_scale, art.height - 1);
    sf::Color pixel = m_collisionImage.getPixel({
        static_cast<unsigned int>(art.x + artX),
        static_cast<unsigned int>(art.y + artY)
    });
    return pixel.a > 0;
}

void SFMLSprite::Preload()
{
    if (!m_textureLoaded)
        load_texture();
}

void SFMLSprite::Unload()
{
    m_load_failed = false;  // a later draw retries the image

    if (!m_textureLoaded)
        return;

    // Release GPU texture — replace with default (empty) texture
    m_texture = sf::Texture();

    // Release collision image
    m_collisionImage = sf::Image();

    // Reset dimensions
    m_bitmapWidth = 0;
    m_bitmapHeight = 0;

    m_textureLoaded = false;

    // Keep: m_frames, m_image_path — needed for reload
}

bool SFMLSprite::IsLoaded() const
{
    return m_textureLoaded;
}

void SFMLSprite::Restore()
{
    // Recreate texture if needed (e.g. after device loss); retries a failed image
    m_load_failed = false;
    if (!m_textureLoaded)
        load_texture();
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
