// sprite_factory.h: Abstract sprite factory interface for renderer abstraction
//
// Part of the shared interface layer between client and renderers
//////////////////////////////////////////////////////////////////////

#pragma once

#include "sprite.h"
#include "PAK.h"
#include <string>

namespace hb::shared::sprite {

class ISpriteFactory {
public:
    virtual ~ISpriteFactory() = default;

    //------------------------------------------------------------------
    // Sprite Creation/Destruction
    //------------------------------------------------------------------

    // Create a sprite from a PAK file
    // pakName: Name of the PAK file (without path/extension, e.g., "Gmtile")
    // spriteIndex: Index of the sprite within the PAK
    // alphaEffect: Whether this sprite supports alpha degree changes
    virtual ISprite* create_sprite(const std::string& pakName, int spriteIndex, bool alphaEffect = true) = 0;

    // Create a sprite from pre-loaded metadata (used by SpriteLoader for batch loading)
    // Only stores frame rects + PAK path/index — no image data is copied.
    // Texture is loaded on demand from disk when the sprite is first drawn.
    virtual ISprite* create_sprite_from_metadata(
        const std::vector<PAKLib::sprite_rect>& frames,
        const std::string& pakFilePath,
        int spriteIndex,
        bool alphaEffect = true) = 0;

    // Destroy a sprite and free its resources
    virtual void destroy_sprite(ISprite* sprite) = 0;

    //------------------------------------------------------------------
    // Global Alpha Degree (legacy feature)
    //------------------------------------------------------------------

    // Set global alpha degree affecting all sprites with alphaEffect enabled
    // Degree 1 = normal, Degree 2 = night/dark mode
    virtual void set_ambient_light_level(int level) = 0;
    virtual int get_ambient_light_level() const = 0;

    //------------------------------------------------------------------
    // PAK File Information
    //------------------------------------------------------------------

    // get the number of sprites in a PAK file
    // Returns 0 if the file cannot be opened or is invalid
    virtual int get_sprite_count(const std::string& pakName) const = 0;

    //------------------------------------------------------------------
    // Path Configuration
    //------------------------------------------------------------------

    // get the base path for sprite PAK files
    virtual std::string get_sprite_path() const = 0;
};

//------------------------------------------------------------------
// Global Sprite Factory Access
//------------------------------------------------------------------

class Sprites {
public:
    // Set the active sprite factory (called by renderer during init)
    static void set_factory(ISpriteFactory* factory);

    // get the active sprite factory
    static ISpriteFactory* get_factory();

    // Convenience methods that delegate to the active factory
    static ISprite* create(const std::string& pakName, int spriteIndex, bool alphaEffect = true);
    static void destroy(ISprite* sprite);

    // Global alpha degree
    static void set_ambient_light_level(int level);
    static int get_ambient_light_level();

    // PAK file information
    static int get_sprite_count(const std::string& pakName);

    // Path configuration
    static std::string get_sprite_path();

private:
    static ISpriteFactory* s_factory;
};

} // namespace hb::shared::sprite
