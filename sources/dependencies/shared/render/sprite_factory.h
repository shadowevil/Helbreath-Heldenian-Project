// sprite_factory.h: Abstract sprite factory interface for renderer abstraction
//
// Part of the shared interface layer between client and renderers
//////////////////////////////////////////////////////////////////////

#pragma once

#include "sprite.h"
#include "sprite_types.h"
#include <string>
#include <vector>

namespace hb::shared::sprite {

class ISpriteFactory {
public:
    virtual ~ISpriteFactory() = default;

    //------------------------------------------------------------------
    // Sprite Creation/Destruction
    //------------------------------------------------------------------

    // Create a sprite from an extracted sprite folder
    // asset_name: Asset name below the sprite path (e.g., "npcs/ogre")
    // spriteIndex: Index of the sprite within the asset
    // alphaEffect: Whether this sprite supports alpha degree changes
    virtual ISprite* create_sprite(const std::string& asset_name, int spriteIndex, bool alphaEffect = true) = 0;

    // Create a sprite from pre-loaded metadata (used by SpriteLoader for batch loading)
    // Only stores frame rects + image path — no image data is read.
    // Texture is loaded on demand from image_path when the sprite is first drawn;
    // an empty image_path is an empty sprite. frames are in image pixels; art_scale is image pixels per
    // logical pixel (folder_sprite::art_scale), and the sprite reports and draws frames at logical size.
    virtual ISprite* create_sprite_from_metadata(
        const std::vector<SpriteRect>& frames,
        const std::string& image_path,
        bool alphaEffect = true,
        int art_scale = 1) = 0;

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
    // Asset Information
    //------------------------------------------------------------------

    // get the number of sprites in an asset
    // Returns 0 if the asset is unknown or its manifest is invalid
    virtual int get_sprite_count(const std::string& asset_name) const = 0;

    //------------------------------------------------------------------
    // Path Configuration
    //------------------------------------------------------------------

    // get the base path for sprite asset folders
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
    static ISprite* create(const std::string& asset_name, int spriteIndex, bool alphaEffect = true);
    static void destroy(ISprite* sprite);

    // Global alpha degree
    static void set_ambient_light_level(int level);
    static int get_ambient_light_level();

    // Asset information
    static int get_sprite_count(const std::string& asset_name);

    // Path configuration
    static std::string get_sprite_path();

private:
    static ISpriteFactory* s_factory;
};

} // namespace hb::shared::sprite
