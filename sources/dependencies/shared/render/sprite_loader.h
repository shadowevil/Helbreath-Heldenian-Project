// sprite_loader.h: Efficient batch sprite loading from PAK files
//
// Opens a PAK file once and allows loading multiple sprites from it
// without repeated file I/O operations.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "sprite.h"
#include "sprite_factory.h"
#include "PAK.h"
#include <string>
#include <functional>
#include <stdexcept>
#include <memory>

namespace hb::shared::sprite {

class SpriteLoader {
public:
    // Open a PAK file and execute a callback with the loader.
    // Only reads metadata (frame rects) — no image data is loaded.
    // The PAK file is soft-locked to prevent external modification.
    template<typename Func>
    static void open_pak(const std::string& pakName, Func&& use) {
        SpriteLoader loader;
        loader.m_pakName = pakName;

        ISpriteFactory* factory = Sprites::get_factory();
        if (!factory) {
            throw std::runtime_error("No sprite factory set");
        }

        // Build full path using factory's configured sprite path
        std::string pakPath = factory->get_sprite_path() + "/" + pakName + ".pak";

        try {
            loader.m_pak = PAKLib::loadpak_metadata_fast(pakPath);
            loader.m_isPakOpen = true;
            PAKLib::pak_lock_manager::lock(pakPath);
            use(loader);
        }
        catch (...) {
            loader.m_isPakOpen = false;
            throw;
        }
        loader.m_isPakOpen = false;
    }

    // get a sprite from the loaded PAK file.
    // Returns a sprite with only metadata — texture loaded on demand from disk.
    ISprite* get_sprite(size_t index, bool alphaEffect = true) {
        if (!m_isPakOpen) {
            throw std::runtime_error("No PAK file is currently open");
        }
        if (index >= m_pak.sprites.size()) {
            throw std::out_of_range("Sprite index out of range: " + std::to_string(index) +
                                    " >= " + std::to_string(m_pak.sprites.size()));
        }

        ISpriteFactory* factory = Sprites::get_factory();
        if (!factory) {
            return nullptr;
        }

        return factory->create_sprite_from_metadata(
            m_pak.sprites[index].sprite_rectangles,
            m_pak.pak_file_path,
            static_cast<int>(index),
            alphaEffect);
    }

    // get the number of sprites in the PAK
    size_t get_sprite_count() const {
        if (!m_isPakOpen) {
            return 0;
        }
        return m_pak.sprites.size();
    }

    // get PAK name
    const std::string& get_pak_name() const { return m_pakName; }

    // Check if PAK is open
    bool is_open() const { return m_isPakOpen; }

private:
    SpriteLoader() = default;

    bool m_isPakOpen = false;
    std::string m_pakName;
    PAKLib::pak m_pak;
};

} // namespace hb::shared::sprite
