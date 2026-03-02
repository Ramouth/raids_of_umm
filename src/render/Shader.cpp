#include "Shader.h"
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <utility>

Shader::Shader(const std::string& vertPath, const std::string& fragPath) {
    std::string vertSrc = loadFile(vertPath);
    std::string fragSrc = loadFile(fragPath);

    GLuint vert = compileShader(GL_VERTEX_SHADER,   vertSrc, vertPath);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc, fragPath);

    m_program = glCreateProgram();
    glAttachShader(m_program, vert);
    glAttachShader(m_program, frag);
    glLinkProgram(m_program);

    GLint ok = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLsizei len = 0;
        char log[1024];
        glGetProgramInfoLog(m_program, sizeof(log), &len, log);
        glDeleteProgram(m_program);
        m_program = 0;
        throw std::runtime_error(
            "[Shader] Link error (" + vertPath + " + " + fragPath + "):\n" + log);
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
}

Shader::~Shader() {
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

Shader::Shader(Shader&& o) noexcept
    : m_program(o.m_program), m_uniformCache(std::move(o.m_uniformCache)) {
    o.m_program = 0;
}

Shader& Shader::operator=(Shader&& o) noexcept {
    if (this != &o) {
        if (m_program) glDeleteProgram(m_program);
        m_program      = o.m_program;
        m_uniformCache = std::move(o.m_uniformCache);
        o.m_program    = 0;
    }
    return *this;
}

void Shader::bind()   const { glUseProgram(m_program); }
void Shader::unbind() const { glUseProgram(0); }

GLint Shader::uniformLocation(const std::string& name) const {
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end()) return it->second;
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    m_uniformCache[name] = loc;
    return loc;
}

void Shader::setInt  (const std::string& n, int v)            const { glUniform1i(uniformLocation(n), v); }
void Shader::setFloat(const std::string& n, float v)          const { glUniform1f(uniformLocation(n), v); }
void Shader::setVec2 (const std::string& n, const glm::vec2& v) const { glUniform2f(uniformLocation(n), v.x, v.y); }
void Shader::setVec3 (const std::string& n, const glm::vec3& v) const { glUniform3f(uniformLocation(n), v.x, v.y, v.z); }
void Shader::setVec4 (const std::string& n, const glm::vec4& v) const { glUniform4f(uniformLocation(n), v.x, v.y, v.z, v.w); }
void Shader::setMat3 (const std::string& n, const glm::mat3& m) const {
    glUniformMatrix3fv(uniformLocation(n), 1, GL_FALSE, glm::value_ptr(m));
}
void Shader::setMat4 (const std::string& n, const glm::mat4& m) const {
    glUniformMatrix4fv(uniformLocation(n), 1, GL_FALSE, glm::value_ptr(m));
}

// ── Private helpers ───────────────────────────────────────────────────────────

std::string Shader::loadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("[Shader] Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint Shader::compileShader(GLenum type, const std::string& src, const std::string& debugName) {
    GLuint shader = glCreateShader(type);
    const char* cstr = src.c_str();
    glShaderSource(shader, 1, &cstr, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLsizei len = 0;
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), &len, log);
        glDeleteShader(shader);
        throw std::runtime_error("[Shader] Compile error (" + debugName + "):\n" + log);
    }
    return shader;
}
