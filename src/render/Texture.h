#pragma once
#include <glad/glad.h>
#include <string>
#include <vector>

// Load an 8-bit RGB or RGBA PNG from disk and upload it as an OpenGL texture.
// Returns the GL texture handle, or 0 on failure.
GLuint loadTexturePNG(const std::string& path);

