// sprite_loader.h: Efficient batch sprite loading from an extracted sprite folder
//
// Reads an asset's manifest once and allows creating multiple sprites from it
// without repeated file I/O operations.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "sprite.h"
#include "sprite_factory.h"
#include "sprite_folder.h"
#include <string>
#include <stdexcept>
#include <vector>

namespace hb::shared::sprite {

class SpriteLoader {
public:
    // Read an asset's manifest (e.g. "npcs/ogre") and execute a callback with the loader.
    // Only frame metadata is read; each texture is loaded from its image file on first draw.
    template<typename Func>
    static void open(const std::string& asset_name, Func&& use) {
        ISpriteFactory* factory = Sprites::get_factory();
        if (!factory) {
            throw std::runtime_error("No sprite factory set");
        }

        SpriteLoader loader;
        loader.m_asset_name = asset_name;
        loader.m_sprites = load_sprite_folder(factory->get_sprite_path(), asset_name);
        use(loader);
    }

    // get a sprite from the loaded asset.
    // Returns a sprite with only metadata — texture loaded on demand from disk.
    ISprite* get_sprite(size_t index, bool alphaEffect = true) {
        if (index >= m_sprites.size()) {
            throw std::out_of_range("Sprite index out of range: " + std::to_string(index) +
                                    " >= " + std::to_string(m_sprites.size()));
        }

        ISpriteFactory* factory = Sprites::get_factory();
        if (!factory) {
            return nullptr;
        }

        return factory->create_sprite_from_metadata(
            m_sprites[index].frames,
            m_sprites[index].image_path,
            alphaEffect,
            m_sprites[index].art_scale);
    }

    // get the number of sprites in the asset
    size_t get_sprite_count() const { return m_sprites.size(); }

    // get asset name
    const std::string& get_asset_name() const { return m_asset_name; }

private:
    SpriteLoader() = default;

    std::string m_asset_name;
    std::vector<folder_sprite> m_sprites;
};

} // namespace hb::shared::sprite
