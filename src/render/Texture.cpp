#include "Texture.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <zlib.h>

// These standard GL constants are not included in this project's stripped GLAD.
#ifndef GL_NEAREST_MIPMAP_LINEAR
#  define GL_NEAREST_MIPMAP_LINEAR 0x2702
#endif

static uint32_t readU32BE(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
         | (uint32_t(p[2]) <<  8) |  uint32_t(p[3]);
}

static int paethPredictor(int a, int b, int c) {
    int p  = a + b - c;
    int pa = std::abs(p - a);
    int pb = std::abs(p - b);
    int pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc)             return b;
    return c;
}

// ── Shared PNG decode ─────────────────────────────────────────────────────────
// Decodes a PNG file into an RGBA pixel buffer.  Returns false on error.

static bool decodePNG(const std::string& path,
                      int& outW, int& outH,
                      std::vector<uint8_t>& outPixels) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "[Texture] Cannot open: " << path << "\n";
        return false;
    }
    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>()
    );

    static const uint8_t kSig[8] = { 0x89,'P','N','G','\r','\n',0x1a,'\n' };
    if (data.size() < 8 || std::memcmp(data.data(), kSig, 8) != 0) {
        std::cerr << "[Texture] Not a PNG: " << path << "\n";
        return false;
    }

    uint32_t w = 0, h = 0;
    uint8_t  bitDepth = 0, colorType = 0;
    std::vector<uint8_t> idatBuf;

    size_t pos = 8;
    while (pos + 12 <= data.size()) {
        uint32_t    len  = readU32BE(data.data() + pos);
        char        type[5] {};
        std::memcpy(type, data.data() + pos + 4, 4);
        const uint8_t* cdata = data.data() + pos + 8;

        if      (std::strcmp(type, "IHDR") == 0) {
            w         = readU32BE(cdata);
            h         = readU32BE(cdata + 4);
            bitDepth  = cdata[8];
            colorType = cdata[9];
        }
        else if (std::strcmp(type, "IDAT") == 0) {
            idatBuf.insert(idatBuf.end(), cdata, cdata + len);
        }
        else if (std::strcmp(type, "IEND") == 0) {
            break;
        }
        pos += 12 + len;
    }

    if (bitDepth != 8 || (colorType != 2 && colorType != 6)) {
        std::cerr << "[Texture] Unsupported PNG format (need 8-bit RGB/RGBA): "
                  << path << "\n";
        return false;
    }

    int      bpp    = (colorType == 6) ? 4 : 3;
    uint32_t stride = w * static_cast<uint32_t>(bpp);

    uLongf rawSize = (stride + 1) * h;
    std::vector<uint8_t> raw(rawSize);
    int zr = uncompress(raw.data(), &rawSize, idatBuf.data(), (uLong)idatBuf.size());
    if (zr != Z_OK) {
        std::cerr << "[Texture] zlib error " << zr << " for: " << path << "\n";
        return false;
    }

    outPixels.assign(w * h * 4, 255);
    std::vector<uint8_t> prev(stride, 0);
    size_t src = 0;
    for (uint32_t y = 0; y < h; ++y) {
        uint8_t filter = raw[src++];
        std::vector<uint8_t> row(raw.begin() + src, raw.begin() + src + stride);
        src += stride;
        for (uint32_t x = 0; x < stride; ++x) {
            uint8_t a = (x >= (uint32_t)bpp) ? row[x - bpp] : 0;
            uint8_t b = prev[x];
            uint8_t c = (x >= (uint32_t)bpp) ? prev[x - bpp] : 0;
            switch (filter) {
                case 1: row[x] = uint8_t(row[x] + a);                        break;
                case 2: row[x] = uint8_t(row[x] + b);                        break;
                case 3: row[x] = uint8_t(row[x] + (a + b) / 2);             break;
                case 4: row[x] = uint8_t(row[x] + paethPredictor(a, b, c)); break;
            }
        }
        for (uint32_t x = 0; x < w; ++x) {
            outPixels[(y * w + x) * 4 + 0] = row[x * bpp + 0];
            outPixels[(y * w + x) * 4 + 1] = row[x * bpp + 1];
            outPixels[(y * w + x) * 4 + 2] = row[x * bpp + 2];
            outPixels[(y * w + x) * 4 + 3] = (bpp == 4) ? row[x * bpp + 3] : 255u;
        }
        prev = row;
    }

    outW = static_cast<int>(w);
    outH = static_cast<int>(h);
    return true;
}

static GLuint uploadToGL(int w, int h, const std::vector<uint8_t>& pixels,
                          const std::string& path, int bpp) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    std::cout << "[Texture] Loaded " << path
              << " (" << w << "x" << h << " ch=" << bpp << ")\n";
    return tex;
}

GLuint loadTexturePNG(const std::string& path) {
    int w, h;
    std::vector<uint8_t> pixels;
    if (!decodePNG(path, w, h, pixels)) return 0;
    return uploadToGL(w, h, pixels, path, 4);
}

