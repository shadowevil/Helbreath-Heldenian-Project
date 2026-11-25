// sprite_folder.h: Reads extracted sprite folders (manifest.json plus one image file per sprite)
//
// Each asset ("npcs/ogre") is a folder under the sprite root holding manifest.json and the image
// files it names. <root>/index.json maps every asset name to its folder and is read once per root.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "sprite_types.h"
#include <cstdint>
#include <string>
#include <vector>

namespace hb::shared::sprite {

// One sprite of an asset folder. An empty sprite has no frames and no image path; it keeps its
// position because sprite indices are positional.
//
// art_scale is the sprite record's optional "art_scale": image pixels per logical pixel (a high-resolution
// sheet drawn at its logical size), 1 when absent. frames are as stored, in image pixels; the sprite
// divides them by art_scale, so callers see logical sizes and pivots.
struct folder_sprite
{
	std::vector<SpriteRect> frames;
	std::string image_path;
	int art_scale = 1;
};

// Frame metadata of every sprite in the asset, in index order; no image bytes are read.
// Throws std::runtime_error when the name is not a safe relative path or the manifest is missing or
// malformed. An image file name that would leave the asset folder is ignored (the sprite draws empty).
std::vector<folder_sprite> load_sprite_folder(const std::string& sprite_root, const std::string& name);

// The asset's sprite count from index.json (or its manifest when the index lacks it); 0 when unknown.
int sprite_folder_count(const std::string& sprite_root, const std::string& name);

// Whether <sprite_root>/index.json was read and parsed (reads it on first call). When false every lookup
// falls back to <root>/<name>/manifest.json; shared code cannot log, so the client reports it.
bool sprite_index_loaded(const std::string& sprite_root);

// The whole file as bytes; empty when it cannot be read.
std::vector<uint8_t> read_file_bytes(const std::string& path);

} // namespace hb::shared::sprite
