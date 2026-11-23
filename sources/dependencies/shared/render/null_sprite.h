// null_sprite.h: Null object pattern implementation for ISprite
//
// Returns safe no-op behavior when accessing non-existent sprites.
// Singleton pattern ensures only one instance exists.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "sprite.h"
#include <cstdio>

namespace hb::shared::sprite {

class NullSprite : public ISprite {
public:
    // Singleton access
    static NullSprite& Instance() {
        static NullSprite instance;
        return instance;
    }

    // Non-copyable, non-movable
    NullSprite(const NullSprite&) = delete;
    NullSprite& operator=(const NullSprite&) = delete;
    NullSprite(NullSprite&&) = delete;
    NullSprite& operator=(NullSprite&&) = delete;

    //------------------------------------------------------------------
    // ISprite Implementation - All no-ops
    //------------------------------------------------------------------

    void draw(int, int, int, const DrawParams&) override {
        static int s_nullDrawCount = 0;
        s_nullDrawCount++;
        if (s_nullDrawCount <= 5 || (s_nullDrawCount % 1000) == 0) {
            printf("[NullSprite::draw] call #%d (sprite missing!)\n", s_nullDrawCount);
        }
    }
    void DrawToSurface(void*, int, int, int, const DrawParams&) override {}
    void DrawWidth(int, int, int, int, bool) override {}
    void DrawShifted(int, int, int, int, int, const DrawParams&) override {}

    int GetFrameCount() const override { return 0; }

    SpriteRect GetFrameRect(int) const override {
        return SpriteRect{ 0, 0, 0, 0, 0, 0 };
    }

    void GetBoundingRect(int, int, int, int& left, int& top, int& right, int& bottom) override {
        left = top = right = bottom = 0;
    }

    void CalculateBounds(int, int, int) override {}

    bool GetLastDrawBounds(int& left, int& top, int& right, int& bottom) const override {
        left = right = bottom = 0;
        top = -1;
        return false;
    }

    BoundRect GetBoundRect() const override {
        return BoundRect{};
    }

    bool CheckCollision(int, int, int, int, int) override { return false; }

    void Preload() override {}
    void Unload() override {}
    bool IsLoaded() const override { return false; }
    void Restore() override {}
    bool IsInUse() const override { return false; }
    uint32_t GetLastAccessTime() const override { return 0; }

private:
    NullSprite() = default;
    ~NullSprite() = default;
};

// Convenience function to get NullSprite pointer
inline ISprite* GetNullSprite() {
    return &NullSprite::Instance();
}

} // namespace hb::shared::sprite
