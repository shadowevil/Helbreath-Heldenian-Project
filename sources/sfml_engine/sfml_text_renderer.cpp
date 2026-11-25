// sfml_text_renderer.cpp: SFML implementation of ITextRenderer
//
// Part of SFMLEngine static library
//////////////////////////////////////////////////////////////////////

#include "sfml_text_renderer.h"
#include "sfml_renderer.h"
#include "sfml_texture.h"
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <array>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace hb::shared::text {

// Global accessor implementation
static ITextRenderer* s_pTextRenderer = nullptr;

ITextRenderer* GetTextRenderer()
{
    return s_pTextRenderer;
}

void SetTextRenderer(ITextRenderer* renderer)
{
    s_pTextRenderer = renderer;
}

SFMLTextRenderer::SFMLTextRenderer(SFMLRenderer* renderer)
    : m_renderer(renderer)
    , m_fontLoaded(false)
    , m_font_smoothing(false)
    , m_loaded_font_path()
    , m_font_size(12)  // Default size to match GDI rendering
{
    LoadDefaultFont();
}

bool SFMLTextRenderer::LoadDefaultFont()
{
    return LoadFontFromFile(bundled_font_path);
}

bool SFMLTextRenderer::LoadFontFromFile(const char* fontPath)
{
    if (!fontPath)
        return false;

    if (m_font.openFromFile(fontPath))
    {
        m_font.setSmooth(m_font_smoothing);
        m_fontLoaded = true;
        m_loaded_font_path = fontPath;
        return true;
    }

    return false;
}

void SFMLTextRenderer::SetFontSize(int size)
{
    // SFML/FreeType renders ~2 points larger than GDI, so compensate
    int adjustedSize = (size > 2) ? (size - 2) : size;
    m_font_size = static_cast<unsigned int>(adjustedSize);
}

void SFMLTextRenderer::set_font_smoothing(bool enabled)
{
    if (m_font_smoothing == enabled)
    {
        return;
    }

    m_font_smoothing = enabled;

    // Force a full font reload so all active TTF glyph pages are rebuilt with the new smoothing mode.
    // sf::Font caches glyph pages internally per character size; toggling smooth alone may not
    // retroactively recreate already-generated pages in all SFML versions.
    if (!m_loaded_font_path.empty())
    {
        sf::Font reloaded;
        if (reloaded.openFromFile(m_loaded_font_path))
        {
            reloaded.setSmooth(m_font_smoothing);
            m_font = std::move(reloaded);
            m_fontLoaded = true;
            return;
        }
    }

    // Fallback: preserve behavior even if reload path is unavailable.
    m_font.setSmooth(enabled);
}

bool SFMLTextRenderer::is_font_smoothing() const
{
    return m_font_smoothing;
}

unsigned int SFMLTextRenderer::resolve_font_size(int fontSize) const
{
    if (fontSize <= 0)
        return m_font_size;  // Use default

    // Apply GDI-2 adjustment for the requested size.
    // sf::Font lazily caches glyph pages per character size internally,
    // so requesting different sizes has no reload cost.
    return static_cast<unsigned int>((fontSize > 2) ? (fontSize - 2) : fontSize);
}

bool SFMLTextRenderer::IsFontLoaded() const
{
    return m_fontLoaded;
}

TextMetrics SFMLTextRenderer::measure_text(const char* text, int fontSize) const
{
    TextMetrics metrics = {0, 0};

    if (!text || !m_fontLoaded)
        return metrics;

    sf::Text sfText(m_font, text, resolve_font_size(fontSize));
    sf::FloatRect bounds = sfText.getLocalBounds();

    metrics.width = static_cast<int>(bounds.size.x);
    metrics.height = static_cast<int>(bounds.size.y);

    return metrics;
}

int SFMLTextRenderer::get_fitting_char_count(const char* text, int maxWidth, int fontSize) const
{
    if (!text || !m_fontLoaded)
        return 0;

    unsigned int resolvedSize = resolve_font_size(fontSize);
    int len = static_cast<int>(strlen(text));
    sf::Text sfText(m_font, "", resolvedSize);

    for (int i = len; i > 0; i--)
    {
        std::string substr(text, i);
        sfText.setString(substr);
        sf::FloatRect bounds = sfText.getLocalBounds();

        if (bounds.size.x <= static_cast<float>(maxWidth))
            return i;
    }

    return 0;
}

int SFMLTextRenderer::get_line_height(int fontSize) const
{
    if (!m_fontLoaded)
        return 0;

    return static_cast<int>(m_font.getLineSpacing(resolve_font_size(fontSize)));
}

void SFMLTextRenderer::draw_text(int x, int y, const char* text, const hb::shared::render::Color& color,
                                int fontSize, bool bold)
{
    draw_text_offset(x, y, 0, 0, text, color, fontSize, bold);
}

void SFMLTextRenderer::draw_text_offset(int x, int y, int dx, int dy, const char* text,
                                       const hb::shared::render::Color& color, int fontSize, bool bold)
{
    sf::RenderTarget* target = m_renderer ? m_renderer->get_current_target() : nullptr;
    if (!text || !m_fontLoaded || !target)
        return;

    unsigned int logical_size = resolve_font_size(fontSize);
    sf::Text sfText(m_font, text, logical_size);
    sfText.setFillColor(sf::Color(color.r, color.g, color.b));
    if (bold)
        sfText.setStyle(sf::Text::Bold);

    draw_at_output_resolution(*target, sfText, logical_size, static_cast<float>(x), static_cast<float>(y),
                              sf::RenderStates::Default, output_shift(dx, dy));
}

sf::Vector2f SFMLTextRenderer::output_shift(int dx, int dy) const
{
    // At 1:1 (and in the world pass) text is placed in logical px, so the shift is too
    const sf::Vector2f shift{static_cast<float>(dx), static_cast<float>(dy)};
    if (std::abs(m_renderer->text_scale() - 1.0f) < 0.001f)
        return shift;
    // Per axis, as snap_to_output_pixel: a stretched UI has different x and y scales
    const sf::Vector2f scale = m_renderer->output_pixels_per_unit();
    if (scale.x <= 0.0f || scale.y <= 0.0f)
        return shift;
    return {std::round(shift.x * scale.x) / scale.x, std::round(shift.y * scale.y) / scale.y};
}

void SFMLTextRenderer::draw_at_output_resolution(sf::RenderTarget& target, sf::Text& text, unsigned int logical_size,
                                                 float x, float y, const sf::RenderStates& states,
                                                 sf::Vector2f subpixel) const
{
    // 1 in the world pass (world text stays at logical size until Phase 3) and at 800x600
    float scale = m_renderer->text_scale();
    if (std::abs(scale - 1.0f) < 0.001f)
    {
        text.setPosition(sf::Vector2f{std::round(x), std::round(y)} + subpixel);
        target.draw(text, states);
        return;
    }

    // Rasterise at window resolution: s = ui scale, character size = floor(logical size * s),
    // drawn at 1/s so the UI view maps it back to logical size. Layout and measurement stay logical.
    // floor, not round: hinted text then tends narrower than measured, so it never overhangs.
    long pixel_size = static_cast<long>(std::floor(static_cast<float>(logical_size) * scale + 0.001f));
    text.setCharacterSize(static_cast<unsigned int>((std::max)(pixel_size, 1L)));
    text.setScale({1.0f / scale, 1.0f / scale});

    // Snap the origin once to a whole window pixel; sf::Text keeps every glyph of a line on one shared
    // baseline at an integer offset from it, so glyph y is never snapped on its own
    text.setPosition(m_renderer->snap_to_output_pixel({x, y}) + subpixel);
    target.draw(text, states);
}

int SFMLTextRenderer::load_face(const char* path, bool smooth)
{
    if (!path)
        return -1;
    for (std::size_t i = 0; i < m_faces.size(); i++)
    {
        if (m_faces[i].path == path && m_faces[i].smooth == smooth)
            return static_cast<int>(i) + 1;
    }
    auto font = std::make_unique<sf::Font>();
    if (!font->openFromFile(path))
        return -1;
    // Per face, whatever the Antialias Font setting (which stays with the bundled font): a recipe's
    // +0.5 faux-bold copy only blends with a smooth glyph texture
    font->setSmooth(smooth);
    m_faces.push_back({path, smooth, std::move(font)});
    return static_cast<int>(m_faces.size());
}

const sf::Font* SFMLTextRenderer::face_font(int face_id) const
{
    if (face_id == 0)
        return m_fontLoaded ? &m_font : nullptr;
    if (face_id < 1 || face_id > static_cast<int>(m_faces.size()))
        return nullptr;
    return m_faces[static_cast<std::size_t>(face_id - 1)].font.get();
}

sf::Text SFMLTextRenderer::make_run(const sf::Font& font, const sf::String& text, const text_run& run) const
{
    const unsigned int size = static_cast<unsigned int>((std::max)(run.pixel_size, 1));
    sf::Text sf_text(font, text, size);
    // SFML spaces letters by (space advance / 3) * (factor - 1) px; solve for the tracking. The factor is
    // relative to the space advance, so it holds when the run is rasterised at another size.
    const float space_advance = font.getGlyph(U' ', size, false).advance;
    if (run.tracking != 0.0f && space_advance > 0.0f)
        sf_text.setLetterSpacing(1.0f + 3.0f * run.tracking / space_advance);
    return sf_text;
}

ink_box SFMLTextRenderer::measure_run(const char* text, const text_run& run) const
{
    const sf::Font* font = face_font(run.face);
    if (!text || !font)
        return {};
    const sf::FloatRect bounds = make_run(*font, text, run).getLocalBounds();
    return {bounds.position.x, bounds.position.y, bounds.position.x + bounds.size.x, bounds.position.y + bounds.size.y};
}

void SFMLTextRenderer::draw_run(float x, float y, const char* text, const text_run& run,
                                const hb::shared::render::Color& color, hb::shared::render::BlendMode blend)
{
    sf::RenderTarget* target = m_renderer ? m_renderer->get_current_target() : nullptr;
    const sf::Font* font = face_font(run.face);
    if (!text || !font || !target)
        return;

    sf::Text sf_text = make_run(*font, text, run);
    sf_text.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
    const sf::RenderStates states(blend == hb::shared::render::BlendMode::Additive ? sf::BlendAdd : sf::BlendAlpha);
    // The whole-pixel part is snapped; the fractional part stays, so a faux-bold copy at +0.5 falls
    // between pixels and blends (the face's glyph texture is smooth), as in text_fx.py
    const float base_x = std::floor(x);
    const float base_y = std::floor(y);
    draw_at_output_resolution(*target, sf_text, sf_text.getCharacterSize(), base_x, base_y, states,
                              {x - base_x, y - base_y});
}

namespace
{

// The glyph coverage of a UTF-8 text as the alpha of an image: the ink box plus pad pixels on every side,
// white glyphs onto transparent black at whole pixels (the union of the glyphs' coverage). False when the
// text has no ink or the canvas cannot be made.
bool rasterize_coverage(sf::Text& sf_text, int pad, sf::Image& image)
{
    sf_text.setFillColor(sf::Color::White);
    const sf::FloatRect ink = sf_text.getLocalBounds();
    pad = (std::max)(pad, 0);
    const sf::Vector2u size(static_cast<unsigned int>(std::ceil(ink.size.x)) + 2u * static_cast<unsigned int>(pad),
                            static_cast<unsigned int>(std::ceil(ink.size.y)) + 2u * static_cast<unsigned int>(pad));
    sf::RenderTexture canvas;
    if (ink.size.x <= 0.0f || ink.size.y <= 0.0f || !canvas.resize(size))
        return false;

    canvas.clear(sf::Color::Transparent);
    sf_text.setPosition({std::round(static_cast<float>(pad) - ink.position.x),
                         std::round(static_cast<float>(pad) - ink.position.y)});
    canvas.draw(sf_text, sf::RenderStates(sf::BlendAlpha));
    canvas.display();
    image = canvas.getTexture().copyToImage();
    return true;
}

sf::String utf8_string(const char* text)
{
    return sf::String::fromUtf8(text, text + std::strlen(text));
}

// One separable pass over a width x height plane of alpha: each sample becomes the maximum (dilate) or the
// weighted sum (blur) of its neighbours along one axis; horizontal or vertical
template <typename Combine>
void filter_plane(std::vector<float>& plane, unsigned int width, unsigned int height, bool horizontal, int radius, Combine combine)
{
    std::vector<float> out(plane.size());
    for (unsigned int y = 0; y < height; y++)
    {
        for (unsigned int x = 0; x < width; x++)
        {
            float result = 0.0f;
            for (int k = -radius; k <= radius; k++)
            {
                const int sx = static_cast<int>(x) + (horizontal ? k : 0);
                const int sy = static_cast<int>(y) + (horizontal ? 0 : k);
                const bool inside = sx >= 0 && sy >= 0 && sx < static_cast<int>(width) && sy < static_cast<int>(height);
                result = combine(result, inside ? plane[static_cast<std::size_t>(sy) * width + static_cast<std::size_t>(sx)] : 0.0f, k);
            }
            out[static_cast<std::size_t>(y) * width + x] = result;
        }
    }
    plane.swap(out);
}

} // namespace

hb::shared::render::ITexture* SFMLTextRenderer::bake_run(const char* text, const text_run& run,
                                                         const hb::shared::render::Color& color, int pad)
{
    const sf::Font* font = face_font(run.face);
    if (!text || *text == '\0' || !font)
        return nullptr;

    sf::Text sf_text = make_run(*font, utf8_string(text), run);
    sf::Image image;
    if (!rasterize_coverage(sf_text, pad, image))
        return nullptr;

    // Every pixel's colour is set to the run's colour (straight alpha), the coverage kept as its alpha
    const sf::Vector2u size = image.getSize();
    for (unsigned int y = 0; y < size.y; y++)
    {
        for (unsigned int x = 0; x < size.x; x++)
        {
            const std::uint8_t coverage = image.getPixel({x, y}).a;
            image.setPixel({x, y}, sf::Color(color.r, color.g, color.b, static_cast<std::uint8_t>(coverage * color.a / 255)));
        }
    }
    return new SFMLTexture(image, true);
}

hb::shared::render::ITexture* SFMLTextRenderer::bake_glow(const char* text, const text_run& run,
                                                          const hb::shared::render::Color& color, int dilate, float blur)
{
    const sf::Font* font = face_font(run.face);
    if (!text || *text == '\0' || !font)
        return nullptr;

    sf::Text sf_text = make_run(*font, utf8_string(text), run);
    sf::Image image;
    if (!rasterize_coverage(sf_text, glow_pad(dilate, blur), image))
        return nullptr;

    const sf::Vector2u size = image.getSize();
    std::vector<float> plane(static_cast<std::size_t>(size.x) * size.y);
    for (unsigned int y = 0; y < size.y; y++)
    {
        for (unsigned int x = 0; x < size.x; x++)
            plane[static_cast<std::size_t>(y) * size.x + x] = static_cast<float>(image.getPixel({x, y}).a) / 255.0f;
    }

    // Dilate by a square window, then blur with a Gaussian; the pad leaves room for both
    const auto maximum = [](float best, float sample, int) { return (std::max)(best, sample); };
    dilate = (std::max)(dilate, 0);
    filter_plane(plane, size.x, size.y, true, dilate, maximum);
    filter_plane(plane, size.x, size.y, false, dilate, maximum);
    if (blur > 0.0f)
    {
        const int radius = static_cast<int>(std::ceil(blur * 3.0f));
        std::vector<float> weights(static_cast<std::size_t>(2 * radius + 1));
        float sum = 0.0f;
        for (int k = -radius; k <= radius; k++)
        {
            const float w = std::exp(-static_cast<float>(k * k) / (2.0f * blur * blur));
            weights[static_cast<std::size_t>(k + radius)] = w;
            sum += w;
        }
        const auto gaussian = [&weights, radius, sum](float total, float sample, int k)
        {
            return total + sample * weights[static_cast<std::size_t>(k + radius)] / sum;
        };
        filter_plane(plane, size.x, size.y, true, radius, gaussian);
        filter_plane(plane, size.x, size.y, false, radius, gaussian);
    }

    for (unsigned int y = 0; y < size.y; y++)
    {
        for (unsigned int x = 0; x < size.x; x++)
        {
            const float coverage = (std::min)(plane[static_cast<std::size_t>(y) * size.x + x], 1.0f);
            image.setPixel({x, y}, sf::Color(color.r, color.g, color.b, static_cast<std::uint8_t>(coverage * static_cast<float>(color.a))));
        }
    }
    return new SFMLTexture(image, true);
}

namespace
{

// The 1:1 masks are point-sampled from the glyphs rasterised at this many times the face's pixel size:
// odd, so every logical pixel's block has a centre texel, and large, so the auto-hinter SFML forces
// (FT_LOAD_FORCE_AUTOHINT) moves an edge by well under a logical pixel
constexpr int pixel_oversample = 21;
// Masks are packed in rows no wider than this
constexpr int pixel_atlas_row = 512;

struct pixel_mask
{
    sf::Vector2i offset;            // top-left from the pen position on the baseline, logical px
    sf::Vector2i size;
    std::vector<std::uint8_t> on;   // size.x * size.y, row major, 1 where the pixel is ink
};

// One glyph's 1:1 mask: each logical pixel takes the coverage at the centre of its oversample block,
// on a grid anchored at the pen position and baseline (not at the glyph's box), at half coverage or more
pixel_mask sample_mask(const sf::Glyph& glyph, const sf::Image& page)
{
    pixel_mask mask;
    const sf::Vector2i box_pos(glyph.bounds.position);
    const sf::Vector2i box_size(glyph.bounds.size);
    if (box_size.x <= 0 || box_size.y <= 0)
        return mask;

    const auto floor_div = [](int value) { return value >= 0 ? value / pixel_oversample : -((-value + pixel_oversample - 1) / pixel_oversample); };
    const sf::Vector2i first(floor_div(box_pos.x), floor_div(box_pos.y));
    const sf::Vector2i last(floor_div(box_pos.x + box_size.x - 1), floor_div(box_pos.y + box_size.y - 1));
    mask.offset = first;
    mask.size = last - first + sf::Vector2i(1, 1);
    mask.on.assign(static_cast<std::size_t>(mask.size.x) * static_cast<std::size_t>(mask.size.y), 0);

    for (int py = 0; py < mask.size.y; py++)
    {
        const int sy = (first.y + py) * pixel_oversample + pixel_oversample / 2 - box_pos.y;
        if (sy < 0 || sy >= box_size.y)
            continue;
        for (int px = 0; px < mask.size.x; px++)
        {
            const int sx = (first.x + px) * pixel_oversample + pixel_oversample / 2 - box_pos.x;
            if (sx < 0 || sx >= box_size.x)
                continue;
            const sf::Vector2u texel(static_cast<unsigned int>(glyph.textureRect.position.x + sx),
                                     static_cast<unsigned int>(glyph.textureRect.position.y + sy));
            if (page.getPixel(texel).a >= 128)
                mask.on[static_cast<std::size_t>(py) * static_cast<std::size_t>(mask.size.x) + static_cast<std::size_t>(px)] = 1;
        }
    }
    return mask;
}

// Two triangles of a textured quad: dest in target units, src in texels
void append_quad(sf::VertexArray& quads, sf::FloatRect dest, sf::FloatRect src, sf::Color color)
{
    const sf::Vector2f d0 = dest.position;
    const sf::Vector2f d1 = dest.position + dest.size;
    const sf::Vector2f s0 = src.position;
    const sf::Vector2f s1 = src.position + src.size;
    const sf::Vertex corners[6] = {
        {{d0.x, d0.y}, color, {s0.x, s0.y}}, {{d1.x, d0.y}, color, {s1.x, s0.y}}, {{d0.x, d1.y}, color, {s0.x, s1.y}},
        {{d0.x, d1.y}, color, {s0.x, s1.y}}, {{d1.x, d0.y}, color, {s1.x, s0.y}}, {{d1.x, d1.y}, color, {s1.x, s1.y}},
    };
    for (const sf::Vertex& v : corners)
        quads.append(v);
}

} // namespace

int SFMLTextRenderer::load_pixel_face(const char* path, int pixel_size)
{
    if (!path || pixel_size <= 0)
        return -1;
    for (std::size_t i = 0; i < m_pixel_faces.size(); i++)
    {
        if (m_pixel_faces[i]->path == path && m_pixel_faces[i]->pixel_size == pixel_size)
            return static_cast<int>(i) + 1;
    }

    auto pf = std::make_unique<pixel_face>();
    pf->path = path;
    pf->pixel_size = pixel_size;
    pf->font = std::make_unique<sf::Font>();
    if (!pf->font->openFromFile(path))
        return -1;
    // Scaled glyphs are drawn texel for output pixel on a snapped origin, so nearest loses nothing
    pf->font->setSmooth(false);

    // The large raster comes from a font of its own, so its glyph page is freed once the masks are taken
    sf::Font large;
    if (!large.openFromFile(path))
        return -1;
    const unsigned int large_size = static_cast<unsigned int>(pixel_size * pixel_oversample);
    std::array<sf::Glyph, pixel_glyph_count> large_glyphs;
    for (std::size_t i = 0; i < pixel_glyph_count; i++)
    {
        const char32_t code = pixel_first_char + static_cast<char32_t>(i);
        pixel_glyph& g = pf->glyphs[i];
        g.present = code != U' ' && pf->font->hasGlyph(code);
        if (!g.present)
            continue;
        large_glyphs[i] = large.getGlyph(code, large_size, false);
        // The design advances are whole pixels at pixel_size; the hinted large advance rounds back to them
        g.advance = static_cast<int>(std::lround(large_glyphs[i].advance / static_cast<float>(pixel_oversample)));
    }
    // One read of the page, after every glyph is on it (a page that grows keeps earlier glyphs in place)
    const sf::Image page = large.getTexture(large_size).copyToImage();

    std::array<pixel_mask, pixel_glyph_count> masks;
    sf::Vector2i cursor(0, 0);
    int row_height = 0;
    int atlas_width = 1;
    for (std::size_t i = 0; i < pixel_glyph_count; i++)
    {
        if (!pf->glyphs[i].present)
            continue;
        masks[i] = sample_mask(large_glyphs[i], page);
        const sf::Vector2i size = masks[i].size;
        if (size.x <= 0 || size.y <= 0)
            continue;
        if (cursor.x > 0 && cursor.x + size.x > pixel_atlas_row)
        {
            cursor = {0, cursor.y + row_height + 1};
            row_height = 0;
        }
        pf->glyphs[i].mask = sf::IntRect(cursor, size);
        pf->glyphs[i].offset = masks[i].offset;
        cursor.x += size.x + 1;
        row_height = (std::max)(row_height, size.y);
        atlas_width = (std::max)(atlas_width, cursor.x);
    }

    sf::Image atlas(sf::Vector2u(static_cast<unsigned int>(atlas_width), static_cast<unsigned int>(cursor.y + row_height + 1)),
                    sf::Color::Transparent);
    for (std::size_t i = 0; i < pixel_glyph_count; i++)
    {
        const sf::IntRect rect = pf->glyphs[i].mask;
        for (int y = 0; y < rect.size.y; y++)
        {
            for (int x = 0; x < rect.size.x; x++)
            {
                if (masks[i].on[static_cast<std::size_t>(y) * static_cast<std::size_t>(rect.size.x) + static_cast<std::size_t>(x)])
                    atlas.setPixel({static_cast<unsigned int>(rect.position.x + x), static_cast<unsigned int>(rect.position.y + y)},
                                   sf::Color::White);
            }
        }
    }
    if (!pf->atlas.loadFromImage(atlas))
        return -1;
    pf->atlas.setSmooth(false);

    m_pixel_faces.push_back(std::move(pf));
    return static_cast<int>(m_pixel_faces.size());
}

const SFMLTextRenderer::pixel_face* SFMLTextRenderer::pixel_face_at(int face_id) const
{
    if (face_id < 1 || face_id > static_cast<int>(m_pixel_faces.size()))
        return nullptr;
    return m_pixel_faces[static_cast<std::size_t>(face_id - 1)].get();
}

const SFMLTextRenderer::pixel_glyph* SFMLTextRenderer::pixel_glyph_for(const pixel_face& face, char c)
{
    const auto code = static_cast<unsigned char>(c);
    if (code < pixel_first_char || code >= pixel_first_char + pixel_glyph_count)
        return nullptr;
    const pixel_glyph& g = face.glyphs[code - pixel_first_char];
    return g.present ? &g : nullptr;
}

template <typename Visit>
int SFMLTextRenderer::for_each_pixel_glyph(const pixel_face& face, const char* text, const pixel_run& run, Visit visit)
{
    int pen = 0;
    for (const char* p = text; *p; p++)
    {
        const pixel_glyph* g = pixel_glyph_for(face, *p);
        if (!g)
        {
            pen += run.space_advance;
            continue;
        }
        visit(*p, *g, pen);
        pen += g->advance;
    }
    return pen;
}

int SFMLTextRenderer::measure_pixel_run(const char* text, const pixel_run& run) const
{
    const pixel_face* face = pixel_face_at(run.face);
    if (!text || !face)
        return 0;
    return for_each_pixel_glyph(*face, text, run, [](char, const pixel_glyph&, int) {});
}

void SFMLTextRenderer::draw_pixel_run(int x, int y, int dx, int dy, const char* text, const pixel_run& run,
                                      const hb::shared::render::Color& color, hb::shared::render::BlendMode blend)
{
    sf::RenderTarget* target = m_renderer ? m_renderer->get_current_target() : nullptr;
    const pixel_face* face = pixel_face_at(run.face);
    if (!text || !face || !target)
        return;

    const sf::Color fill(color.r, color.g, color.b, color.a);
    sf::RenderStates states(blend == hb::shared::render::BlendMode::Additive ? sf::BlendAdd : sf::BlendAlpha);
    // One array for every call (up to four per shadowed string a frame): clear() keeps its capacity
    sf::VertexArray& quads = m_pixel_quads;
    quads.clear();
    const float scale = m_renderer->text_scale();

    if (std::abs(scale - 1.0f) < 0.001f)
    {
        // 1:1 (and the world pass): the masks on whole logical pixels, as the bitmap glyphs drew
        for_each_pixel_glyph(*face, text, run, [&](char, const pixel_glyph& g, int pen)
        {
            if (g.mask.size.x <= 0)
                return;
            const sf::Vector2f pos(static_cast<float>(x + dx + pen + g.offset.x),
                                   static_cast<float>(y + dy + run.baseline + g.offset.y));
            append_quad(quads, sf::FloatRect(pos, sf::Vector2f(g.mask.size)), sf::FloatRect(g.mask), fill);
        });
        states.texture = &face->atlas;
    }
    else
    {
        // Scaled: each glyph rasterised at the output resolution and drawn at 1/scale, its pen position
        // (whole logical px) snapped to a whole output pixel; the baseline is the same for every glyph
        // sf::Font keeps a glyph page per character size and never frees one; this stays small because only
        // the few UI scales a session uses (one per window size) ever reach here
        const auto size = static_cast<unsigned int>((std::max)(
            static_cast<long>(std::floor(static_cast<float>(face->pixel_size) * scale + 0.001f)), 1L));
        const sf::Vector2f shift = output_shift(dx, dy);
        for_each_pixel_glyph(*face, text, run, [&](char c, const pixel_glyph&, int pen)
        {
            const sf::Glyph& glyph = face->font->getGlyph(static_cast<char32_t>(static_cast<unsigned char>(c)), size, false);
            if (glyph.bounds.size.x <= 0.0f || glyph.bounds.size.y <= 0.0f)
                return;
            const sf::Vector2f origin = m_renderer->snap_to_output_pixel(
                {static_cast<float>(x + pen), static_cast<float>(y + run.baseline)}) + shift;
            append_quad(quads, sf::FloatRect(origin + glyph.bounds.position / scale, glyph.bounds.size / scale),
                        sf::FloatRect(glyph.textureRect), fill);
        });
        // After every getGlyph: a new glyph can grow the page
        states.texture = &face->font->getTexture(size);
    }

    if (quads.getVertexCount() > 0)
        target->draw(quads, states);
}

void SFMLTextRenderer::draw_text_aligned(int x, int y, int width, int height, const char* text, const hb::shared::render::Color& color,
                                        Align alignment, int fontSize, bool bold)
{
    draw_text_aligned_offset(x, y, width, height, 0, 0, text, color, alignment, fontSize, bold);
}

void SFMLTextRenderer::draw_text_aligned_offset(int x, int y, int width, int height, int dx, int dy, const char* text,
                                               const hb::shared::render::Color& color, Align alignment, int fontSize,
                                               bool bold)
{
    sf::RenderTarget* target = m_renderer ? m_renderer->get_current_target() : nullptr;
    if (!text || !m_fontLoaded || !target)
        return;

    // Placement uses the logical-size metrics, so aligned text lands exactly where it did at 800x600
    unsigned int logical_size = resolve_font_size(fontSize);
    sf::Text sfText(m_font, text, logical_size);
    if (bold)
        sfText.setStyle(sf::Text::Bold);
    sf::FloatRect bounds = sfText.getLocalBounds();

    // Extract alignment components
    uint8_t hAlign = alignment & Align::HMask;
    uint8_t vAlign = alignment & Align::VMask;

    // Calculate X position based on horizontal alignment
    float drawX = static_cast<float>(x) - bounds.position.x;
    if (hAlign == Align::HCenter)
        drawX = static_cast<float>(x) + (static_cast<float>(width) - bounds.size.x) / 2.0f - bounds.position.x;
    else if (hAlign == Align::Right)
        drawX = static_cast<float>(x + width) - bounds.size.x - bounds.position.x;

    // Calculate Y position based on vertical alignment
    float drawY = static_cast<float>(y) - bounds.position.y;
    if (vAlign == Align::VCenter)
        drawY = static_cast<float>(y) + (static_cast<float>(height) - bounds.size.y) / 2.0f - bounds.position.y;
    else if (vAlign == Align::Bottom)
        drawY = static_cast<float>(y + height) - bounds.size.y - bounds.position.y;

    // Round to nearest pixel for pixel-perfect rendering
    int pixelX = static_cast<int>(drawX + 0.5f);
    int pixelY = static_cast<int>(drawY + 0.5f);

    sfText.setFillColor(sf::Color(color.r, color.g, color.b));

    draw_at_output_resolution(*target, sfText, logical_size, static_cast<float>(pixelX), static_cast<float>(pixelY),
                              sf::RenderStates::Default, output_shift(dx, dy));
}

void SFMLTextRenderer::begin_batch()
{
    // No-op for SFML - we don't need DC acquisition
}

void SFMLTextRenderer::end_batch()
{
    // No-op for SFML
}

} // namespace hb::shared::text
