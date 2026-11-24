// output_rects.h: where the world target and the UI space land in the window
//
// Part of SFMLEngine static library. One computation shared by the renderer (composite, UI view)
// and the window (mouse transform, cursor clip), so the two can never disagree.
//   Letterbox (fullscreen, no stretch): world and UI share the largest centred rect of the logical
//     aspect, uniform scale.
//   Stretch (windowed or fullscreenStretch): the world fills the whole window, scaled per axis; the
//     UI keeps a uniform scale in the largest centred rect of the logical aspect. When the window
//     already has the logical aspect the UI uses the world mapping itself, so nothing changes there.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <cmath>

// One logical space mapped onto the window: rect in window pixels, scale in pixels per logical unit
struct output_space
{
    sf::FloatRect rect;
    sf::Vector2f scale;

    // Window pixels -> logical units of this space
    sf::Vector2f from_output(sf::Vector2f pixel) const
    {
        return {(pixel.x - rect.position.x) / scale.x, (pixel.y - rect.position.y) / scale.y};
    }

    // Logical units of this space -> window pixels
    sf::Vector2f to_output(sf::Vector2f logical) const
    {
        return {rect.position.x + logical.x * scale.x, rect.position.y + logical.y * scale.y};
    }

    // The unit a window pixel falls in, as mouse input floors it (from_output, then floor)
    sf::Vector2i unit_of_pixel(sf::Vector2i pixel) const
    {
        const sf::Vector2f logical = from_output({static_cast<float>(pixel.x), static_cast<float>(pixel.y)});
        return {static_cast<int>(std::floor(logical.x)), static_cast<int>(std::floor(logical.y))};
    }

    // The first window pixel whose unit is `unit` (or, when the scale is below 1 and the unit has no pixel,
    // the first one past it): the exact inverse of unit_of_pixel, at any fractional scale
    sf::Vector2i first_pixel_of_unit(sf::Vector2i unit) const
    {
        const sf::Vector2f edge = to_output({static_cast<float>(unit.x), static_cast<float>(unit.y)});
        sf::Vector2i pixel(static_cast<int>(std::ceil(edge.x)), static_cast<int>(std::ceil(edge.y)));
        // the float edge can land a pixel either side of the floor; settle it with the forward mapping
        while (unit_of_pixel({pixel.x - 1, pixel.y}).x >= unit.x) --pixel.x;
        while (unit_of_pixel(pixel).x < unit.x) ++pixel.x;
        while (unit_of_pixel({pixel.x, pixel.y - 1}).y >= unit.y) --pixel.y;
        while (unit_of_pixel(pixel).y < unit.y) ++pixel.y;
        return pixel;
    }
};

struct output_rects
{
    output_space world;
    output_space ui;
};

inline output_rects compute_output_rects(sf::Vector2f output, sf::Vector2f logical, bool letterbox)
{
    const sf::Vector2f stretch_scale(output.x / logical.x, output.y / logical.y);
    const output_space stretch{sf::FloatRect({0.0f, 0.0f}, output), stretch_scale};
    if (!letterbox && stretch_scale.x == stretch_scale.y)
        return {stretch, stretch};

    const float scale = (stretch_scale.y < stretch_scale.x) ? stretch_scale.y : stretch_scale.x;
    const sf::Vector2f dest(logical.x * scale, logical.y * scale);
    const output_space uniform{
        sf::FloatRect({(output.x - dest.x) / 2.0f, (output.y - dest.y) / 2.0f}, dest), {scale, scale}};
    return {letterbox ? uniform : stretch, uniform};
}
