// sfml_texture.h: SFML implementation of hb::shared::render::ITexture interface
//
// Part of SFMLEngine static library
// Wraps sf::Texture and sf::RenderTexture for rendering operations
// Supports two modes:
//   - Render target mode (default): off-screen rendering via sf::RenderTexture
//   - Image mode: loaded from file with mipmaps for display-only use
//////////////////////////////////////////////////////////////////////

#pragma once

#include "texture.h"
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <cstdint>

namespace sf { class Image; }

class SFMLTexture : public hb::shared::render::ITexture
{
public:
    // Render-target mode: creates both texture and render texture
    SFMLTexture(uint16_t width, uint16_t height);

    // Image mode: loaded from pre-existing image (display-only, with mipmaps)
    explicit SFMLTexture(const sf::Image& image);

    virtual ~SFMLTexture();

    // ============== hb::shared::render::ITexture Implementation ==============
    uint16_t get_width() const override;
    uint16_t get_height() const override;

    // Render-target operations (no-ops in image mode)
    void begin_render() override;
    void end_render() override;
    void set_view(float origin_x, float origin_y, float width, float height) override;
    void reset_view() override;
    void* get_render_surface() override;
    void clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) override;
    void set_smooth(bool smooth) override;

    // ============== SFML-Specific Access ==============

    // Get the texture to use for display (works for both modes)
    const sf::Texture& get_display_texture() const;

    // Get the SFML render texture (for rendering operations — render-target mode only)
    sf::RenderTexture* GetRenderTexture() { return m_image_mode ? nullptr : &m_renderTexture; }

    bool is_image_mode() const { return m_image_mode; }

private:
    sf::Texture m_texture;
    sf::RenderTexture m_renderTexture;

    uint16_t m_width;
    uint16_t m_height;
    bool m_image_mode;
};
