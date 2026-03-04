#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include "SpriteRenderer.h"

/*
 * AnimatedSprite — wraps SpriteRenderer to drive horizontal-strip sprite animation.
 *
 * Spritesheet contract:
 *   - Horizontal strip: N frames each 64×64 px → image width = N×64, height = 64
 *   - UV per frame: u ∈ [frame/N, (frame+1)/N]  (handled by SpriteRenderer::setFrame)
 *
 * Usage:
 *   AnimatedSprite sprite;
 *   sprite.init();
 *   sprite.loadSprite("assets/textures/units/armoured_warrior_idle.png", 4);
 *   sprite.addClip("idle",   {0, 4, 6.0f,  true});
 *   sprite.addClip("attack", {0, 4, 12.0f, false});
 *   sprite.play("idle");
 *
 *   // per frame:
 *   sprite.update(dt);
 *   sprite.draw(worldPos, size, view, proj);
 */
struct AnimClip {
    int   firstFrame = 0;
    int   numFrames  = 1;
    float fps        = 6.0f;
    bool  loop       = true;
};

class AnimatedSprite {
public:
    AnimatedSprite()  = default;
    ~AnimatedSprite() = default;

    AnimatedSprite(const AnimatedSprite&)            = delete;
    AnimatedSprite& operator=(const AnimatedSprite&) = delete;

    // Must be called once before use (initialises the underlying SpriteRenderer).
    void init();

    // Load a spritesheet PNG.  totalFrames = total columns in the strip.
    void loadSprite(const std::string& pngPath, int totalFrames = 1);

    // Register a named animation clip.
    void addClip(const std::string& name, const AnimClip& clip);

    // Start playing a clip by name.  Resets elapsed time and frame index.
    // If the clip is already playing nothing changes (no restart).
    void play(const std::string& name);

    // Advance animation by dt seconds.  Call every game/render tick.
    void update(float dt);

    // Passthrough draw — calls SpriteRenderer::draw after setting the correct frame.
    void draw(const glm::vec3& worldPos, float size,
              const glm::mat4& view, const glm::mat4& proj);
    void draw(const glm::vec3& worldPos, float worldWidth, float worldHeight,
              const glm::mat4& view, const glm::mat4& proj);

    // True while a non-looping clip is still playing (false when it has ended).
    bool isPlaying() const { return m_playing; }

    // Name of the currently active clip (empty string if none).
    const std::string& currentClip() const { return m_currentClip; }

private:
    SpriteRenderer m_renderer;
    std::unordered_map<std::string, AnimClip> m_clips;

    std::string m_currentClip;
    int   m_totalFrames = 1;   // total columns in the loaded spritesheet
    int   m_localFrame  = 0;   // frame index within the current clip
    float m_elapsed     = 0.0f;
    bool  m_playing     = false;
};
