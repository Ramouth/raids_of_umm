#include "Texture.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <zlib.h>
#include <filesystem>

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

bool loadPNG(const std::string& path, PNGData& out) {
    if (!decodePNG(path, out.w, out.h, out.pixels)) return false;
    out.tex = uploadToGL(out.w, out.h, out.pixels, path, 4);
    return true;
}

// ── PNG writer ────────────────────────────────────────────────────────────────

static void writeU32BE(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back((v >> 24) & 0xFF);
    buf.push_back((v >> 16) & 0xFF);
    buf.push_back((v >>  8) & 0xFF);
    buf.push_back((v >>  0) & 0xFF);
}

static void writePNGChunk(std::vector<uint8_t>& out, const char* type,
                           const uint8_t* data, uint32_t len) {
    writeU32BE(out, len);
    const auto* t = reinterpret_cast<const uint8_t*>(type);
    out.insert(out.end(), t, t + 4);
    if (data && len > 0) out.insert(out.end(), data, data + len);
    uLong crc = crc32(0, t, 4);
    if (data && len > 0) crc = crc32(crc, data, len);
    writeU32BE(out, static_cast<uint32_t>(crc));
}

bool savePNG(const std::string& path, const uint8_t* rgba, int w, int h) {
    // Build raw data: filter byte 0 (None) per row + RGBA pixels
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>((w * 4 + 1) * h));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba + y * w * 4, rgba + y * w * 4 + w * 4);
    }

    uLongf compSize = compressBound(static_cast<uLong>(raw.size()));
    std::vector<uint8_t> comp(compSize);
    if (compress(comp.data(), &compSize, raw.data(), static_cast<uLong>(raw.size())) != Z_OK) {
        std::cerr << "[Texture] savePNG: compress failed\n";
        return false;
    }
    comp.resize(compSize);

    uint8_t ihdr[13] = {};
    auto put32 = [](uint8_t* p, uint32_t v) {
        p[0]=(v>>24)&0xFF; p[1]=(v>>16)&0xFF; p[2]=(v>>8)&0xFF; p[3]=v&0xFF;
    };
    put32(ihdr + 0, static_cast<uint32_t>(w));
    put32(ihdr + 4, static_cast<uint32_t>(h));
    ihdr[8] = 8;  // bit depth
    ihdr[9] = 6;  // RGBA

    std::vector<uint8_t> out;
    static const uint8_t sig[8] = { 0x89,'P','N','G','\r','\n',0x1a,'\n' };
    out.insert(out.end(), sig, sig + 8);
    writePNGChunk(out, "IHDR", ihdr, 13);
    writePNGChunk(out, "IDAT", comp.data(), static_cast<uint32_t>(comp.size()));
    writePNGChunk(out, "IEND", nullptr, 0);

    namespace fs = std::filesystem;
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "[Texture] savePNG: cannot write " << path << "\n";
        return false;
    }
    f.write(reinterpret_cast<const char*>(out.data()),
            static_cast<std::streamsize>(out.size()));
    std::cout << "[Texture] Saved " << path << " (" << w << "x" << h << ")\n";
    return true;
}
