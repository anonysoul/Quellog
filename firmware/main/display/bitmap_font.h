#ifndef QUELLOG_BITMAP_FONT_H_
#define QUELLOG_BITMAP_FONT_H_

#include <cstddef>
#include <cstdint>
#include <string>

struct GlyphBitmap {
    uint32_t codepoint = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    int16_t x_offset = 0;
    int16_t y_offset = 0;
    uint16_t advance = 0;
    const uint8_t* bitmap = nullptr;
    size_t bitmap_size = 0;
    bool fallback = false;
};

class BitmapFont {
public:
    static BitmapFont& Instance();

    bool EnsureLoaded();
    bool IsLoaded() const { return loaded_; }
    int line_height() const { return line_height_; }
    int baseline() const { return baseline_; }

    bool GetGlyph(uint32_t codepoint, GlyphBitmap& glyph) const;
    int MeasureText(const char* text) const;
    std::string FitText(const std::string& text, int max_width, const char* ellipsis = "...") const;

private:
    BitmapFont() = default;
    BitmapFont(const BitmapFont&) = delete;
    BitmapFont& operator=(const BitmapFont&) = delete;

    bool Load();
    int FindGlyphIndex(uint32_t codepoint) const;
    bool BuildGlyph(int glyph_index, bool fallback, GlyphBitmap& glyph) const;

    bool attempted_ = false;
    bool loaded_ = false;
    int line_height_ = 16;
    int baseline_ = 12;
    int font_size_ = 16;
    int fallback_index_ = -1;
    const uint8_t* mapped_data_ = nullptr;
    size_t mapped_size_ = 0;
    uint32_t mmap_handle_ = 0;
    const uint8_t* glyph_table_ = nullptr;
    const uint8_t* bitmap_table_ = nullptr;
    uint32_t glyph_count_ = 0;
};

bool NextUtf8CodePoint(const std::string& text, size_t& offset, uint32_t& codepoint);

#endif  // QUELLOG_BITMAP_FONT_H_
