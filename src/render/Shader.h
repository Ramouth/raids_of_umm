#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

/*
 * Shader — GLSL program wrapper.
 *
 * Loads vertex + fragment shader source from file paths, compiles, links.
 * Provides uniform helpers. Caches uniform locations.
 *
 * Usage:
 *   Shader s("assets/shaders/model.vert", "assets/shaders/model.frag");
 *   s.bind();
 *   s.setMat4("u_MVP", mvp);
 */
class Shader {
public:
    Shader() = default;
    Shader(const std::string& vertPath, const std::string& fragPath);
    ~Shader();

    // Non-copyable, movable
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& o) noexcept;
    Shader& operator=(Shader&& o) noexcept;

    void bind()   const;
    void unbind() const;

    bool valid() const { return m_program != 0; }
    GLuint id()  const { return m_program; }

    // Uniform setters
    void setInt  (const std::string& name, int v)                  const;
    void setFloat(const std::string& name, float v)                const;
    void setVec2 (const std::string& name, const glm::vec2& v)     const;
    void setVec3 (const std::string& name, const glm::vec3& v)     const;
    void setVec4 (const std::string& name, const glm::vec4& v)     const;
    void setMat3 (const std::string& name, const glm::mat3& m)     const;
    void setMat4 (const std::string& name, const glm::mat4& m)     const;

private:
    GLuint m_program = 0;
    mutable std::unordered_map<std::string, GLint> m_uniformCache;

    GLint uniformLocation(const std::string& name) const;

    static GLuint compileShader(GLenum type, const std::string& src, const std::string& debugName);
    static std::string loadFile(const std::string& path);
};
