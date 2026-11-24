// sprite_factory.cpp: Static member implementations for Sprites class
//
//////////////////////////////////////////////////////////////////////

#include "sprite_factory.h"
#include <cstdio>

namespace hb::shared::sprite {

// Static member initialization
ISpriteFactory* Sprites::s_factory = nullptr;

void Sprites::set_factory(ISpriteFactory* factory) {
    s_factory = factory;
}

ISpriteFactory* Sprites::get_factory() {
    return s_factory;
}

ISprite* Sprites::create(const std::string& asset_name, int spriteIndex, bool alphaEffect) {
    if (s_factory) {
        return s_factory->create_sprite(asset_name, spriteIndex, alphaEffect);
    }
    printf("[Sprites::create] ERROR: No factory set! Cannot create sprite %s[%d]\n", asset_name.c_str(), spriteIndex);
    return nullptr;
}

void Sprites::destroy(ISprite* sprite) {
    if (s_factory && sprite) {
        s_factory->destroy_sprite(sprite);
    }
}

void Sprites::set_ambient_light_level(int level) {
    if (s_factory) {
        s_factory->set_ambient_light_level(level);
    }
}

int Sprites::get_ambient_light_level() {
    if (s_factory) {
        return s_factory->get_ambient_light_level();
    }
    return 1;
}

int Sprites::get_sprite_count(const std::string& asset_name) {
    if (s_factory) {
        return s_factory->get_sprite_count(asset_name);
    }
    return 0;
}

std::string Sprites::get_sprite_path() {
    if (s_factory) {
        return s_factory->get_sprite_path();
    }
    return "sprites";  // Default fallback
}

} // namespace hb::shared::sprite
