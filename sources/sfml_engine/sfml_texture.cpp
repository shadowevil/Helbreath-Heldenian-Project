// sfml_texture.cpp: SFML implementation of hb::shared::render::ITexture interface
//
// Part of SFMLEngine static library
//////////////////////////////////////////////////////////////////////

#include "sfml_texture.h"
#include <SFML/Graphics/Image.hpp>

// Render-target mode constructor
SFMLTexture::SFMLTexture(uint16_t width, uint16_t height)
    : m_width(width)
    , m_height(height)
    , m_image_mode(false)
{
    // Create the texture with the specified size
    if (!m_texture.resize({width, height}))
    {
        // Texture creation failed - this shouldn't happen with a valid size
    }

    // Create render texture for blit operations
    if (!m_renderTexture.resize({width, height}))
    {
        // RenderTexture creation failed
    }

    // Initialize render texture to black
    m_renderTexture.clear(sf::Color::Black);
    m_renderTexture.display();
}

// Image mode constructor — loaded from pre-existing image; smooth adds bilinear filtering and mipmaps
SFMLTexture::SFMLTexture(const sf::Image& image, bool smooth)
    : m_width(static_cast<uint16_t>(image.getSize().x))
    , m_height(static_cast<uint16_t>(image.getSize().y))
    , m_image_mode(true)
{
    m_texture = sf::Texture(image);
    m_texture.setSmooth(smooth);
    // Mipmaps are a quality improvement only; without them the texture still draws.
    // A nearest texture has none, so minified draws never blend atlas neighbours in.
    if (smooth)
    {
        [[maybe_unused]] const bool mipmapped = m_texture.generateMipmap();
    }
}

SFMLTexture::~SFMLTexture()
{
    // SFML handles cleanup automatically
}

uint16_t SFMLTexture::get_width() const
{
    return m_width;
}

uint16_t SFMLTexture::get_height() const
{
    return m_height;
}

const sf::Texture& SFMLTexture::get_display_texture() const
{
    if (m_image_mode)
    {
        return m_texture;
    }
    return m_renderTexture.getTexture();
}

void SFMLTexture::begin_render()
{
    // Nothing to do: SFML render textures need no explicit begin
}

void SFMLTexture::end_render()
{
    if (m_image_mode)
    {
        return;
    }
    m_renderTexture.display();
}

void SFMLTexture::set_view(float origin_x, float origin_y, float width, float height)
{
    if (m_image_mode)
    {
        return;
    }
    sf::View view(sf::FloatRect({origin_x, origin_y}, {width, height}));
    m_renderTexture.setView(view);
}

void SFMLTexture::reset_view()
{
    if (m_image_mode)
    {
        return;
    }
    m_renderTexture.setView(m_renderTexture.getDefaultView());
}

void* SFMLTexture::get_render_surface()
{
    if (m_image_mode)
    {
        return nullptr;
    }
    return &m_renderTexture;
}

void SFMLTexture::clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (m_image_mode)
    {
        return;
    }
    m_renderTexture.clear(sf::Color(r, g, b, a));
}

void SFMLTexture::set_smooth(bool smooth)
{
    if (m_image_mode)
    {
        m_texture.setSmooth(smooth);
    }
    else
    {
        m_renderTexture.setSmooth(smooth);
    }
}
