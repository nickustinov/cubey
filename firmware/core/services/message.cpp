#include "services/message.h"

#include <cJSON.h>

namespace cubey {

ServerMessage parse_message(const std::string &json) {
    cJSON *msg = cJSON_Parse(json.c_str());
    const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(msg, "type"));
    const char *kind = cJSON_GetStringValue(cJSON_GetObjectItem(msg, "for"));
    const char *letter = cJSON_GetStringValue(cJSON_GetObjectItem(msg, "letter"));
    const char *word = cJSON_GetStringValue(cJSON_GetObjectItem(msg, "word"));
    const char *hers = cJSON_GetStringValue(cJSON_GetObjectItem(msg, "hers"));
    const auto positive = [msg](const char *name) {
        const cJSON *item = cJSON_GetObjectItem(msg, name);
        return cJSON_IsNumber(item) && cJSON_GetNumberValue(item) > 0 ? static_cast<int>(cJSON_GetNumberValue(item)) : 0;
    };
    ServerMessage out{type ? type : "", cJSON_IsTrue(cJSON_GetObjectItem(msg, "last")) != 0, positive("minutes"), kind ? kind : "", letter ? letter : "", word ? word : "",
                      hers ? hers : "", cJSON_IsTrue(cJSON_GetObjectItem(msg, "on")) != 0, positive("n"), {}, "", "", {}, cJSON_IsTrue(cJSON_GetObjectItem(msg, "new")) != 0,
                      positive("brightness"), positive("volume"), ""};
    const auto whole = [msg](const char *name) {
        const cJSON *item = cJSON_GetObjectItem(msg, name);
        return cJSON_IsNumber(item) && cJSON_GetNumberValue(item) >= 0 ? static_cast<int>(cJSON_GetNumberValue(item)) : -1;
    };
    out.i = whole("i");
    out.ms = whole("ms");
    if (const char *text = cJSON_GetStringValue(cJSON_GetObjectItem(msg, "text"))) out.text = text;
    if (const char *language = cJSON_GetStringValue(cJSON_GetObjectItem(msg, "language"))) out.language = language;
    if (const char *theme = cJSON_GetStringValue(cJSON_GetObjectItem(msg, "theme"))) out.theme = theme;
    const cJSON *item = nullptr;
    cJSON_ArrayForEach(item, cJSON_GetObjectItem(msg, "rows")) if (cJSON_IsString(item)) out.rows.emplace_back(item->valuestring);
    cJSON_ArrayForEach(item, cJSON_GetObjectItem(msg, "languages")) if (cJSON_IsString(item)) out.languages.emplace_back(item->valuestring);
    cJSON_Delete(msg);
    return out;
}

std::string tapped_message(const std::string &id) {
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", "tapped");
    cJSON_AddStringToObject(msg, "id", id.c_str());
    char *text = cJSON_PrintUnformatted(msg);
    std::string out = text;
    cJSON_free(text);
    cJSON_Delete(msg);
    return out;
}

} // namespace cubey
