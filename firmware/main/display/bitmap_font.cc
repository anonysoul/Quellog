#include "bitmap_font.h"

#include <esp_log.h>
#include <esp_partition.h>

#include <algorithm>
#include <cstring>
#include <string>

namespace {

constexpr char kTag[] = "BitmapFont";
constexpr char kMagic[] = "QGF1";
constexpr uint16_t kFormatVersion = 1;
constexpr char kPartitionLabel[] = "font";

struct FontHeader {
    char magic[4];
    uint16_t version;
    uint16_t reserved;
    uint32_t glyph_count;
    int16_t line_height;
    int16_t baseline;
    uint16_t font_size;
    uint16_t fallback_index;
    uint32_t glyph_table_offset;
    uint32_t bitmap_table_offset;
    uint32_t bitmap_data_size;
} __attribute__((packed));

struct GlyphRecord {
    uint32_t codepoint;
    uint32_t bitmap_offset;
    uint16_t width;
    uint16_t height;
    int16_t x_offset;
    int16_t y_offset;
    uint16_t advance;
    uint16_t bitmap_size;
} __attribute__((packed));

constexpr uint32_t kInvalidCodePoint = 0xFFFFFFFFUL;

uint32_t DecodeUtf8Byte(unsigned char byte) {
    return static_cast<uint32_t>(byte);
}

}  // namespace

BitmapFont& BitmapFont::Instance() {
    static BitmapFont instance;
    return instance;
}

bool BitmapFont::EnsureLoaded() {
    if (attempted_) {
        return loaded_;
    }
    attempted_ = true;
    loaded_ = Load();
    return loaded_;
}

bool BitmapFont::Load() {
    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        static_cast<esp_partition_subtype_t>(0x40),
        kPartitionLabel);
    if (partition == nullptr) {
        ESP_LOGE(kTag, "font partition '%s' not found", kPartitionLabel);
        return false;
    }

    const void* mapped = nullptr;
    esp_partition_mmap_handle_t mmap_handle = 0;
    esp_err_t err = esp_partition_mmap(partition, 0, partition->size, ESP_PARTITION_MMAP_DATA, &mapped, &mmap_handle);
    if (err != ESP_OK || mapped == nullptr) {
        ESP_LOGE(kTag, "failed to mmap font partition: %s", esp_err_to_name(err));
        return false;
    }

    mapped_data_ = static_cast<const uint8_t*>(mapped);
    mapped_size_ = partition->size;
    mmap_handle_ = mmap_handle;

    if (mapped_size_ < sizeof(FontHeader)) {
        ESP_LOGE(kTag, "font partition too small");
        return false;
    }

    const FontHeader* header = reinterpret_cast<const FontHeader*>(mapped_data_);
    if (std::memcmp(header->magic, kMagic, sizeof(header->magic)) != 0) {
        ESP_LOGE(kTag, "font partition magic mismatch");
        return false;
    }
    if (header->version != kFormatVersion) {
        ESP_LOGE(kTag, "unsupported font format version: %u", static_cast<unsigned>(header->version));
        return false;
    }
    if (header->glyph_table_offset >= mapped_size_ || header->bitmap_table_offset >= mapped_size_) {
        ESP_LOGE(kTag, "invalid font offsets");
        return false;
    }

    glyph_count_ = header->glyph_count;
    line_height_ = header->line_height;
    baseline_ = header->baseline;
    font_size_ = header->font_size;
    fallback_index_ = header->fallback_index;
    glyph_table_ = mapped_data_ + header->glyph_table_offset;
    bitmap_table_ = mapped_data_ + header->bitmap_table_offset;

    const size_t glyph_table_size = static_cast<size_t>(glyph_count_) * sizeof(GlyphRecord);
    if (header->glyph_table_offset + glyph_table_size > mapped_size_) {
        ESP_LOGE(kTag, "invalid glyph table size");
        return false;
    }
    if (header->bitmap_table_offset + header->bitmap_data_size > mapped_size_) {
        ESP_LOGE(kTag, "invalid bitmap table size");
        return false;
    }

    ESP_LOGI(kTag, "loaded font partition: glyphs=%lu line_height=%d font_size=%d",
             static_cast<unsigned long>(glyph_count_), line_height_, font_size_);
    return true;
}

int BitmapFont::FindGlyphIndex(uint32_t codepoint) const {
    int low = 0;
    int high = static_cast<int>(glyph_count_) - 1;
    while (low <= high) {
        const int mid = low + ((high - low) / 2);
        const GlyphRecord* record = reinterpret_cast<const GlyphRecord*>(
            glyph_table_ + (static_cast<size_t>(mid) * sizeof(GlyphRecord)));
        if (record->codepoint == codepoint) {
            return mid;
        }
        if (record->codepoint < codepoint) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return -1;
}

bool BitmapFont::BuildGlyph(int glyph_index, bool fallback, GlyphBitmap& glyph) const {
    if (glyph_index < 0 || glyph_index >= static_cast<int>(glyph_count_)) {
        return false;
    }
    const GlyphRecord* record = reinterpret_cast<const GlyphRecord*>(
        glyph_table_ + (static_cast<size_t>(glyph_index) * sizeof(GlyphRecord)));
    const size_t bitmap_region_offset = static_cast<size_t>(bitmap_table_ - mapped_data_);
    if (bitmap_region_offset + record->bitmap_offset + record->bitmap_size > mapped_size_) {
        return false;
    }

    glyph.codepoint = record->codepoint;
    glyph.width = record->width;
    glyph.height = record->height;
    glyph.x_offset = record->x_offset;
    glyph.y_offset = record->y_offset;
    glyph.advance = record->advance;
    glyph.bitmap = bitmap_table_ + record->bitmap_offset;
    glyph.bitmap_size = record->bitmap_size;
    glyph.fallback = fallback;
    return true;
}

bool BitmapFont::GetGlyph(uint32_t codepoint, GlyphBitmap& glyph) const {
    if (!loaded_) {
        return false;
    }
    const int index = FindGlyphIndex(codepoint);
    if (index >= 0 && BuildGlyph(index, false, glyph)) {
        return true;
    }

    static uint32_t last_missing = kInvalidCodePoint;
    if (last_missing != codepoint) {
        ESP_LOGW(kTag, "missing glyph U+%04lX", static_cast<unsigned long>(codepoint));
        last_missing = codepoint;
    }
    return BuildGlyph(fallback_index_, true, glyph);
}

int BitmapFont::MeasureText(const char* text) const {
    if (text == nullptr || *text == '\0') {
        return 0;
    }
    if (!loaded_) {
        return static_cast<int>(std::strlen(text)) * (font_size_ / 2);
    }

    const std::string value(text);
    size_t offset = 0;
    int width = 0;
    while (offset < value.size()) {
        uint32_t codepoint = 0;
        if (!NextUtf8CodePoint(value, offset, codepoint)) {
            break;
        }
        GlyphBitmap glyph;
        if (GetGlyph(codepoint, glyph)) {
            width += glyph.advance;
        }
    }
    return width;
}

std::string BitmapFont::FitText(const std::string& text, int max_width, const char* ellipsis) const {
    if (max_width <= 0 || text.empty()) {
        return "";
    }
    if (!loaded_ || MeasureText(text.c_str()) <= max_width) {
        return text;
    }

    const std::string suffix = ellipsis == nullptr ? "" : std::string(ellipsis);
    const int suffix_width = MeasureText(suffix.c_str());
    std::string result;
    size_t offset = 0;
    int width = 0;
    while (offset < text.size()) {
        const size_t original_offset = offset;
        uint32_t codepoint = 0;
        if (!NextUtf8CodePoint(text, offset, codepoint)) {
            break;
        }
        GlyphBitmap glyph;
        if (!GetGlyph(codepoint, glyph)) {
            break;
        }
        if (width + glyph.advance + suffix_width > max_width) {
            break;
        }
        result.append(text.substr(original_offset, offset - original_offset));
        width += glyph.advance;
    }
    if (result.empty()) {
        return suffix_width <= max_width ? suffix : "";
    }
    return result + suffix;
}

bool NextUtf8CodePoint(const std::string& text, size_t& offset, uint32_t& codepoint) {
    if (offset >= text.size()) {
        return false;
    }

    const unsigned char lead = static_cast<unsigned char>(text[offset]);
    if (lead < 0x80) {
        codepoint = DecodeUtf8Byte(lead);
        ++offset;
        return true;
    }

    int length = 0;
    uint32_t value = 0;
    if ((lead & 0xE0U) == 0xC0U) {
        length = 2;
        value = lead & 0x1FU;
    } else if ((lead & 0xF0U) == 0xE0U) {
        length = 3;
        value = lead & 0x0FU;
    } else if ((lead & 0xF8U) == 0xF0U) {
        length = 4;
        value = lead & 0x07U;
    } else {
        codepoint = '?';
        ++offset;
        return true;
    }

    if (offset + static_cast<size_t>(length) > text.size()) {
        codepoint = '?';
        offset = text.size();
        return true;
    }

    for (int i = 1; i < length; ++i) {
        const unsigned char next = static_cast<unsigned char>(text[offset + static_cast<size_t>(i)]);
        if ((next & 0xC0U) != 0x80U) {
            codepoint = '?';
            ++offset;
            return true;
        }
        value = (value << 6U) | (next & 0x3FU);
    }

    offset += static_cast<size_t>(length);
    codepoint = value;
    return true;
}
