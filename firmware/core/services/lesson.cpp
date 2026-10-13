#include "services/lesson.h"

#include <cJSON.h>
#include <cstring>

namespace cubey {

constexpr size_t kMostPictures = 3;

static std::vector<uint8_t> picture(const cJSON *item) {
    const char *text = cJSON_GetStringValue(cJSON_GetObjectItem(item, "picture"));
    std::vector<uint8_t> png = base64_decode(text ? text : "");
    uint32_t w, h;
    if (!png_size(png, w, h)) png.clear();
    return png;
}

static bool is(const cJSON *msg, const char *type) {
    const char *t = cJSON_GetStringValue(cJSON_GetObjectItem(msg, "type"));
    return t && strcmp(t, type) == 0;
}

std::optional<std::vector<uint8_t>> lesson_show(const std::string &json) {
    cJSON *msg = cJSON_Parse(json.c_str());
    std::optional<std::vector<uint8_t>> out;
    if (is(msg, "lesson.show")) out = picture(msg);
    cJSON_Delete(msg);
    return out;
}

std::optional<std::vector<LessonPicture>> lesson_choose(const std::string &json) {
    cJSON *msg = cJSON_Parse(json.c_str());
    std::optional<std::vector<LessonPicture>> out;
    if (is(msg, "lesson.choose")) {
        out.emplace();
        const cJSON *item = nullptr;
        cJSON_ArrayForEach(item, cJSON_GetObjectItem(msg, "pictures")) {
            const char *id = cJSON_GetStringValue(cJSON_GetObjectItem(item, "id"));
            std::vector<uint8_t> png = picture(item);
            if (id && *id && !png.empty() && out->size() < kMostPictures) out->push_back({id, std::move(png)});
        }
    }
    cJSON_Delete(msg);
    return out;
}

bool png_size(const std::vector<uint8_t> &png, uint32_t &w, uint32_t &h) {
    static const uint8_t signature[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (png.size() < 24 || memcmp(png.data(), signature, sizeof signature) != 0 || memcmp(&png[12], "IHDR", 4) != 0)
        return false;
    const auto be32 = [&](size_t at) { return uint32_t(png[at]) << 24 | uint32_t(png[at + 1]) << 16 | uint32_t(png[at + 2]) << 8 | png[at + 3]; };
    w = be32(16);
    h = be32(20);
    return w && h;
}

} // namespace cubey
