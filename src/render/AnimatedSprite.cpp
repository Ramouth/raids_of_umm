#include "AnimatedSprite.h"

void AnimatedSprite::init() {
    m_renderer.init();
}

void AnimatedSprite::loadSprite(const std::string& pngPath, int totalFrames) {
    m_renderer.loadSprite(pngPath);
    m_totalFrames = (totalFrames < 1) ? 1 : totalFrames;
}

void AnimatedSprite::addClip(const std::string& name, const AnimClip& clip) {
    m_clips[name] = clip;
}

void AnimatedSprite::play(const std::string& name) {
    if (name == m_currentClip && m_playing) return;
    auto it = m_clips.find(name);
    if (it == m_clips.end()) return;
    m_currentClip = name;
    m_localFrame  = 0;
    m_elapsed     = 0.0f;
    m_playing     = true;
}

void AnimatedSprite::update(float dt) {
    if (!m_playing || m_currentClip.empty()) return;
    auto it = m_clips.find(m_currentClip);
    if (it == m_clips.end()) return;

    const AnimClip& clip = it->second;
    if (clip.fps <= 0.0f || clip.numFrames <= 0) return;

    m_elapsed += dt;
    const float frameDuration = 1.0f / clip.fps;

    while (m_elapsed >= frameDuration) {
        m_elapsed -= frameDuration;
        m_localFrame++;
        if (m_localFrame >= clip.numFrames) {
            if (clip.loop) {
                m_localFrame = 0;
            } else {
                m_localFrame = clip.numFrames - 1;
                m_playing    = false;
                break;
            }
        }
    }
}

void AnimatedSprite::draw(const glm::vec3& worldPos, float size,
                           const glm::mat4& view, const glm::mat4& proj) {
    draw(worldPos, size, size, view, proj);
}

void AnimatedSprite::draw(const glm::vec3& worldPos, float worldWidth, float worldHeight,
                           const glm::mat4& view, const glm::mat4& proj) {
    int absoluteFrame = 0;
    if (!m_currentClip.empty()) {
        auto it = m_clips.find(m_currentClip);
        if (it != m_clips.end()) {
            absoluteFrame = it->second.firstFrame + m_localFrame;
        }
    }
    m_renderer.setFrame(absoluteFrame, m_totalFrames);
    m_renderer.draw(worldPos, worldWidth, worldHeight, view, proj);
}
