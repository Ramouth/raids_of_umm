#pragma once
#include <glad/glad.h>
#include <string>
#include <vector>

// Load an 8-bit RGB or RGBA PNG from disk and upload it as an OpenGL texture.
// Returns the GL texture handle, or 0 on failure.
GLuint loadTexturePNG(const std::string& path);

// PNG with raw pixel access — useful for in-engine cropping / export.
struct PNGData {
    GLuint               tex    = 0;  // GL texture (0 = not loaded)
    int                  w      = 0;
    int                  h      = 0;
    std::vector<uint8_t> pixels;      // RGBA, w*h*4 bytes, row-0-at-top
};

// Loads a PNG, populates all PNGData fields.  Returns false on failure.
bool loadPNG(const std::string& path, PNGData& out);

// Writes an RGBA buffer as a PNG file.  Returns false on failure.
bool savePNG(const std::string& path, const uint8_t* rgba, int w, int h);
