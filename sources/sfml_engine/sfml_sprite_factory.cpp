// sfml_sprite_factory.cpp: SFML implementation of ISpriteFactory interface
//
// Part of SFMLEngine static library
//////////////////////////////////////////////////////////////////////

#include "sfml_sprite_factory.h"
#include "sfml_sprite.h"
#include "sfml_renderer.h"
#include "sprite_folder.h"
#include <stdexcept>

SFMLSpriteFactory::SFMLSpriteFactory(SFMLRenderer* renderer)
    : m_renderer(renderer)
    , m_spritePath("sprites")
    , m_ambient_light_level(1)
{
}

SFMLSpriteFactory::~SFMLSpriteFactory()
{
    // Note: Factory does not own created sprites
    // Caller is responsible for destroying sprites via destroy_sprite
}

hb::shared::sprite::ISprite* SFMLSpriteFactory::create_sprite(const std::string& asset_name, int spriteIndex, bool alphaEffect)
{
    std::vector<hb::shared::sprite::folder_sprite> sprites = hb::shared::sprite::load_sprite_folder(m_spritePath, asset_name);
    if (spriteIndex < 0 || static_cast<size_t>(spriteIndex) >= sprites.size())
    {
        throw std::out_of_range("Sprite index " + std::to_string(spriteIndex) + " out of range in " + asset_name);
    }

    const hb::shared::sprite::folder_sprite& entry = sprites[static_cast<size_t>(spriteIndex)];
    return create_sprite_from_metadata(entry.frames, entry.image_path, alphaEffect, entry.art_scale);
}

hb::shared::sprite::ISprite* SFMLSpriteFactory::create_sprite_from_metadata(
    const std::vector<hb::shared::sprite::SpriteRect>& frames,
    const std::string& image_path,
    bool alphaEffect,
    int art_scale)
{
    SFMLSprite* sprite = new SFMLSprite(m_renderer, frames, image_path, alphaEffect, art_scale);

    // Apply global alpha degree
    if (alphaEffect && m_ambient_light_level != 1)
    {
        sprite->set_ambient_light_level(static_cast<char>(m_ambient_light_level));
    }

    return sprite;
}

void SFMLSpriteFactory::destroy_sprite(hb::shared::sprite::ISprite* sprite)
{
    delete sprite;
}

void SFMLSpriteFactory::set_ambient_light_level(int level)
{
    m_ambient_light_level = level;

    // Update renderer's sprite alpha degree
    if (m_renderer)
    {
        m_renderer->set_ambient_light_level(static_cast<char>(level));
    }
}

int SFMLSpriteFactory::get_ambient_light_level() const
{
    return m_ambient_light_level;
}

int SFMLSpriteFactory::get_sprite_count(const std::string& asset_name) const
{
    return hb::shared::sprite::sprite_folder_count(m_spritePath, asset_name);
}
