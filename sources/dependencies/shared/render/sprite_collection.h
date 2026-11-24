// sprite_collection.h: RAII container for ISprite instances
//
// Provides array-like access with automatic memory management.
// Part of the shared interface layer between client and renderers.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "sprite.h"
#include "sprite_factory.h"
#include "null_sprite.h"
#include <unordered_map>
#include <memory>
#include <functional>

namespace hb::shared::sprite {

// Forward declaration
class SpriteCollection;

//------------------------------------------------------------------
// SpriteProxy - Provides operator-> access and assignment
//------------------------------------------------------------------

class SpriteProxy {
public:
    SpriteProxy(SpriteCollection& collection, size_t index);

    // Arrow operator for method access
    ISprite* operator->();
    const ISprite* operator->() const;

    // Dereference operator
    ISprite& operator*();
    const ISprite& operator*() const;

    // Assignment from factory-created sprite
    SpriteProxy& operator=(ISprite* sprite);

    // Boolean conversion (check if sprite exists)
    explicit operator bool() const;

    // Comparison operators for null checks (defined after SpriteCollection)
    bool operator==(std::nullptr_t) const;
    bool operator!=(std::nullptr_t) const;
    bool operator==(int zero) const;
    bool operator!=(int zero) const;

    // get raw pointer
    ISprite* get();
    const ISprite* get() const;

private:
    SpriteCollection& m_collection;
    size_t m_index;
};

//------------------------------------------------------------------
// SpriteCollection - RAII container for sprites
//------------------------------------------------------------------

class SpriteCollection {
public:
    // Custom deleter that uses the sprite factory
    struct SpriteDeleter {
        void operator()(ISprite* sprite) const {
            if (sprite) {
                Sprites::destroy(sprite);
            }
        }
    };

    using SpritePtr = std::unique_ptr<ISprite, SpriteDeleter>;

    SpriteCollection() = default;
    ~SpriteCollection() = default;

    // Non-copyable
    SpriteCollection(const SpriteCollection&) = delete;
    SpriteCollection& operator=(const SpriteCollection&) = delete;

    // Movable
    SpriteCollection(SpriteCollection&&) = default;
    SpriteCollection& operator=(SpriteCollection&&) = default;

    //------------------------------------------------------------------
    // Array-style Access
    //------------------------------------------------------------------

    // Returns a proxy for array-style access: collection[index]->draw(...)
    SpriteProxy operator[](size_t index);

    // Const access returns raw pointer (no proxy assignment)
    const ISprite* operator[](size_t index) const;

    //------------------------------------------------------------------
    // Direct Access
    //------------------------------------------------------------------

    // get sprite at index (nullptr if not present)
    ISprite* get(size_t index);
    const ISprite* get(size_t index) const;

    // Set sprite at index (takes ownership)
    void set(size_t index, ISprite* sprite);

    // Create and set sprite using factory
    ISprite* create(size_t index, const std::string& asset_name, int spriteIndex, bool alphaEffect = true);

    // Remove sprite at index
    void remove(size_t index);

    // Clear all sprites
    void clear();

    //------------------------------------------------------------------
    // Query
    //------------------------------------------------------------------

    // Number of sprites in collection
    size_t size() const { return m_sprites.size(); }

    // Check if index has a sprite
    bool contains(size_t index) const;

    // Check if collection is empty
    bool empty() const { return m_sprites.empty(); }

    // Swap sprites at two indices within this collection
    void swap_indices(size_t index_a, size_t index_b);

    //------------------------------------------------------------------
    // Iteration
    //------------------------------------------------------------------

    auto begin() { return m_sprites.begin(); }
    auto end() { return m_sprites.end(); }
    auto begin() const { return m_sprites.begin(); }
    auto end() const { return m_sprites.end(); }
    auto cbegin() const { return m_sprites.cbegin(); }
    auto cend() const { return m_sprites.cend(); }

    //------------------------------------------------------------------
    // Bulk Operations
    //------------------------------------------------------------------

    // Preload all sprites in collection
    void preloadAll();

    // Unload all sprites (release surfaces but keep metadata)
    void unloadAll();

    // Restore all sprites (after device loss)
    void restoreAll();

private:
    std::unordered_map<size_t, SpritePtr> m_sprites;
};

//------------------------------------------------------------------
// SpriteProxy Implementation
//------------------------------------------------------------------

inline SpriteProxy::SpriteProxy(SpriteCollection& collection, size_t index)
    : m_collection(collection)
    , m_index(index)
{
}

inline ISprite* SpriteProxy::operator->()
{
    ISprite* sprite = m_collection.get(m_index);
    return sprite ? sprite : GetNullSprite();
}

inline const ISprite* SpriteProxy::operator->() const
{
    const ISprite* sprite = m_collection.get(m_index);
    return sprite ? sprite : GetNullSprite();
}

inline ISprite& SpriteProxy::operator*()
{
    ISprite* sprite = m_collection.get(m_index);
    return sprite ? *sprite : NullSprite::Instance();
}

inline const ISprite& SpriteProxy::operator*() const
{
    const ISprite* sprite = m_collection.get(m_index);
    return sprite ? *sprite : NullSprite::Instance();
}

inline SpriteProxy& SpriteProxy::operator=(ISprite* sprite)
{
    m_collection.set(m_index, sprite);
    return *this;
}

inline SpriteProxy::operator bool() const
{
    return m_collection.get(m_index) != nullptr;
}

inline bool SpriteProxy::operator==(std::nullptr_t) const
{
    return m_collection.get(m_index) == nullptr;
}

inline bool SpriteProxy::operator!=(std::nullptr_t) const
{
    return m_collection.get(m_index) != nullptr;
}

inline bool SpriteProxy::operator==(int zero) const
{
    return zero == 0 && m_collection.get(m_index) == nullptr;
}

inline bool SpriteProxy::operator!=(int zero) const
{
    return zero == 0 && m_collection.get(m_index) != nullptr;
}

inline ISprite* SpriteProxy::get()
{
    return m_collection.get(m_index);
}

inline const ISprite* SpriteProxy::get() const
{
    return m_collection.get(m_index);
}

//------------------------------------------------------------------
// SpriteCollection Implementation
//------------------------------------------------------------------

inline SpriteProxy SpriteCollection::operator[](size_t index)
{
    return SpriteProxy(*this, index);
}

inline const ISprite* SpriteCollection::operator[](size_t index) const
{
    return get(index);
}

inline ISprite* SpriteCollection::get(size_t index)
{
    auto it = m_sprites.find(index);
    return (it != m_sprites.end()) ? it->second.get() : nullptr;
}

inline const ISprite* SpriteCollection::get(size_t index) const
{
    auto it = m_sprites.find(index);
    return (it != m_sprites.end()) ? it->second.get() : nullptr;
}

inline void SpriteCollection::set(size_t index, ISprite* sprite)
{
    if (sprite) {
        m_sprites[index] = SpritePtr(sprite);
    }
    else {
        m_sprites.erase(index);
    }
}

inline ISprite* SpriteCollection::create(size_t index, const std::string& asset_name, int spriteIndex, bool alphaEffect)
{
    ISprite* sprite = Sprites::create(asset_name, spriteIndex, alphaEffect);
    if (sprite) {
        m_sprites[index] = SpritePtr(sprite);
    }
    return sprite;
}

inline void SpriteCollection::remove(size_t index)
{
    m_sprites.erase(index);
}

inline void SpriteCollection::clear()
{
    m_sprites.clear();
}

inline bool SpriteCollection::contains(size_t index) const
{
    return m_sprites.find(index) != m_sprites.end();
}

inline void SpriteCollection::preloadAll()
{
    for (auto& pair : m_sprites) {
        if (pair.second) {
            pair.second->Preload();
        }
    }
}

inline void SpriteCollection::unloadAll()
{
    for (auto& pair : m_sprites) {
        if (pair.second) {
            pair.second->Unload();
        }
    }
}

inline void SpriteCollection::restoreAll()
{
    for (auto& pair : m_sprites) {
        if (pair.second) {
            pair.second->Restore();
        }
    }
}

inline void SpriteCollection::swap_indices(size_t index_a, size_t index_b)
{
    auto it_a = m_sprites.find(index_a);
    auto it_b = m_sprites.find(index_b);
    bool has_a = it_a != m_sprites.end();
    bool has_b = it_b != m_sprites.end();

    if (has_a && has_b)
    {
        std::swap(it_a->second, it_b->second);
    }
    else if (has_a)
    {
        m_sprites[index_b] = std::move(it_a->second);
        m_sprites.erase(it_a);
    }
    else if (has_b)
    {
        m_sprites[index_a] = std::move(it_b->second);
        m_sprites.erase(it_b);
    }
}

} // namespace hb::shared::sprite
