// sfml_sprite_factory.h: SFML implementation of ISpriteFactory interface
//
// Part of SFMLEngine static library
// Creates SFMLSprite instances from extracted sprite folders
//////////////////////////////////////////////////////////////////////

#pragma once

#include "sprite_factory.h"
#include <string>

// Forward declaration
class SFMLRenderer;

class SFMLSpriteFactory : public hb::shared::sprite::ISpriteFactory
{
public:
    SFMLSpriteFactory(SFMLRenderer* renderer);
    virtual ~SFMLSpriteFactory();

    //------------------------------------------------------------------
    // ISpriteFactory Implementation
    //------------------------------------------------------------------

    // Sprite creation
    hb::shared::sprite::ISprite* create_sprite(const std::string& asset_name, int spriteIndex, bool alphaEffect = true) override;
    hb::shared::sprite::ISprite* create_sprite_from_metadata(
        const std::vector<hb::shared::sprite::SpriteRect>& frames,
        const std::string& image_path,
        bool alphaEffect = true) override;

    // Sprite destruction
    void destroy_sprite(hb::shared::sprite::ISprite* sprite) override;

    // Global alpha degree
    void set_ambient_light_level(int level) override;
    int get_ambient_light_level() const override;

    // Asset information
    int get_sprite_count(const std::string& asset_name) const override;

    //------------------------------------------------------------------
    // Configuration
    //------------------------------------------------------------------

    // Set the sprite path prefix (default: "sprites")
    void SetSpritePath(const std::string& path) { m_spritePath = path; }
    std::string get_sprite_path() const override { return m_spritePath; }

private:
    SFMLRenderer* m_renderer;
    std::string m_spritePath;
    int m_ambient_light_level;
};
