module;

#include <cocos/renderer/CCTexture2D.h>

export module ctext.texture_filter;

import ctext.config;
import std;

namespace {
    std::vector<cocos2d::Texture2D*> textures;

    void Apply(cocos2d::Texture2D* texture, bool nearest) {
        if (!texture) return;
        // Use Cocos' own helpers so its antialias flag and volatile-texture
        // cache are updated as well as the live GL sampler state.
        if (nearest) texture->setAliasTexParameters();
        else texture->setAntiAliasTexParameters();
    }
}

export namespace ctext::texture_filter {
    void Register(cocos2d::Texture2D* texture) {
        if (!texture) return;
        if (std::find(textures.begin(), textures.end(), texture) == textures.end()) {
            // Keep the object alive while it is tracked.  The resource manager
            // may release its reference before a later live toggle; retaining
            // prevents the toggle pass from touching dangling Texture2D pointers.
            texture->retain();
            textures.push_back(texture);
        }
        Apply(texture, ctext::Config::Get().GraphicsForceNearestFilter);
    }

    void SetNearest(bool enabled) {
        ctext::Config::Get().SetGraphicsForceNearestFilter(enabled);
        for (auto* texture : textures) Apply(texture, enabled);
    }
}
