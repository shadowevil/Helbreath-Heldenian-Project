// sfml_sprite_factory.cpp: SFML implementation of ISpriteFactory interface
//
// Part of SFMLEngine static library
//////////////////////////////////////////////////////////////////////

#include "sfml_sprite_factory.h"
#include "sfml_sprite.h"
#include "sfml_renderer.h"

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

hb::shared::sprite::ISprite* SFMLSpriteFactory::create_sprite(const std::string& pakName, int spriteIndex, bool alphaEffect)
{
    std::string fullPath = BuildPakPath(pakName);

    SFMLSprite* sprite = new SFMLSprite(m_renderer, fullPath, spriteIndex, alphaEffect);

    // Apply global alpha degree
    if (alphaEffect && m_ambient_light_level != 1)
    {
        sprite->set_ambient_light_level(static_cast<char>(m_ambient_light_level));
    }

    return sprite;
}

hb::shared::sprite::ISprite* SFMLSpriteFactory::create_sprite_from_metadata(
    const std::vector<PAKLib::sprite_rect>& frames,
    const std::string& pakFilePath,
    int spriteIndex,
    bool alphaEffect)
{
    SFMLSprite* sprite = new SFMLSprite(m_renderer, frames, pakFilePath, spriteIndex, alphaEffect);

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

int SFMLSpriteFactory::get_sprite_count(const std::string& pakName) const
{
    std::string fullPath = BuildPakPath(pakName);

    try
    {
        PAKLib::pak pakFile = PAKLib::loadpak_metadata_fast(fullPath);
        return static_cast<int>(pakFile.sprite_count);
    }
    catch (...)
    {
        return 0;
    }
}

std::string SFMLSpriteFactory::BuildPakPath(const std::string& pakName) const
{
    // If pakName already has a path or extension, use as-is
    if (pakName.find('/') != std::string::npos ||
        pakName.find('\\') != std::string::npos ||
        pakName.find('.') != std::string::npos)
    {
        return pakName;
    }

    // Build path: spritePath/pakName.pak
    std::string path = m_spritePath;
    if (!path.empty() && path.back() != '/' && path.back() != '\\')
    {
        path += '/';
    }
    path += pakName;
    path += ".pak";

    return path;
}
