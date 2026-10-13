#include "ui/cards.h"

#include <memory>

#include "ui/dots.h"
// LVGL's image cache API (lv_image_cache_drop), which 9.6 leaves out of include/: a picture from the server is freed
// when replaced, and its decoded copy must go with it (the cache knows images by address).
#include "src/misc/cache/instance/lv_image_cache.h"

namespace cubey::cards {

namespace {

constexpr int32_t kScreen = 480, kMiddle = kScreen / 2;
constexpr int32_t kBig = 352;   // the one card: room for its 256 px picture
constexpr int32_t kSmall = 216; // the cards to tap: room for their 176 px pictures
constexpr int32_t kApart = kSmall / 2 + 8; // from the middle to a small card's middle: 16 px between them
constexpr int32_t kRadius = 32;
constexpr uint32_t kCream = 0xFFF8EE;

// A picture from the server: its PNG bytes and the image over them, kept while a card shows it.
struct Picture {
    std::vector<uint8_t> png;
    lv_image_dsc_t dsc{};
};

lv_obj_t *root = nullptr; // the cards' black screen
std::vector<std::unique_ptr<Picture>> pictures;
std::vector<lv_obj_t *> shown; // the cards, in the order placed
std::function<bool(const std::string &)> tap_cb;
bool tapped = false; // one answer per question
int turn_ = 0;

lv_obj_t *card(int32_t side) {
    lv_obj_t *o = lv_obj_create(root);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, side, side);
    lv_obj_set_style_radius(o, kRadius, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(kCream), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(o, false);
    lv_obj_set_clickable(o, false);
    shown.push_back(o);
    return o;
}

// An image over the PNG bytes, which the cards keep until the pictures change.
void image(lv_obj_t *parent, std::vector<uint8_t> png) {
    auto p = std::make_unique<Picture>();
    p->png = std::move(png);
    uint32_t w = 0, h = 0;
    png_size(p->png, w, h);
    p->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    p->dsc.header.cf = LV_COLOR_FORMAT_RAW_ALPHA;
    p->dsc.header.w = w;
    p->dsc.header.h = h;
    p->dsc.data = p->png.data();
    p->dsc.data_size = p->png.size();
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, &p->dsc);
    lv_obj_set_clickable(img, false);
    lv_obj_center(img);
    pictures.push_back(std::move(p));
}

// Each card in its place, its picture upright.
void lay_out() {
    for (size_t i = 0; i < shown.size(); i++) {
        const Place p = place(static_cast<int>(shown.size()), static_cast<int>(i), turn_);
        lv_obj_set_pos(shown[i], p.x - p.side / 2, p.y - p.side / 2);
        if (lv_obj_t *img = lv_obj_get_child(shown[i], 0)) lv_image_set_rotation(img, turn_ * 900);
    }
}

// Pops in: from small to a little too big, and back.
void pop(lv_obj_t *obj, int32_t from) {
    lv_obj_set_style_transform_pivot_x(obj, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(obj, LV_PCT(50), 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, from, LV_SCALE_NONE);
    lv_anim_set_duration(&a, 360);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_set_exec_cb(&a, [](void *o, int32_t scale) { lv_obj_set_style_transform_scale(static_cast<lv_obj_t *>(o), scale, 0); });
    lv_anim_start(&a);
}

void on_tapped(lv_event_t *e) {
    auto *picked = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
    if (tapped || !tap_cb || !tap_cb(*static_cast<std::string *>(lv_event_get_user_data(e)))) return;
    tapped = true;
    for (lv_obj_t *other : shown)
        if (other != picked) lv_obj_set_style_opa(other, LV_OPA_30, 0);
    pop(picked, 300);
}

} // namespace

Place place(int count, int index, int quarters) {
    int32_t dx = 0, dy = 0, side = kBig;
    if (count == 2) dx = index ? kApart : -kApart, side = kSmall;
    if (count == 3) dx = index == 2 ? 0 : index ? kApart : -kApart, dy = index == 2 ? kApart : -kApart, side = kSmall;
    for (int k = 0; k < (quarters & 3); k++) {
        const int32_t was = dx; // a quarter clockwise
        dx = -dy;
        dy = was;
    }
    return {kMiddle + dx, kMiddle + dy, side};
}

lv_obj_t *create(lv_obj_t *parent, std::function<bool(const std::string &)> on_tap) {
    root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(root, lv_color_hex(dots::ground()), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(root, false);
    lv_obj_set_clickable(root, false);
    lv_obj_set_hidden(root, true);
    tap_cb = std::move(on_tap);
    turn_ = dots::turn();
    lv_obj_add_event_cb(
        root,
        [](lv_event_t *) {
            root = nullptr;
            shown.clear();
            for (auto &p : pictures) lv_image_cache_drop(&p->dsc);
            pictures.clear();
            tap_cb = nullptr;
        },
        LV_EVENT_DELETE, nullptr);
    return root;
}

// The cards come off first, then their pictures go (from LVGL's cache of decoded images too).
void clear() {
    if (!root) return;
    lv_obj_clean(root);
    shown.clear();
    for (auto &p : pictures) lv_image_cache_drop(&p->dsc);
    pictures.clear();
    lv_obj_set_hidden(root, true);
}

bool showing() { return root && !lv_obj_is_hidden(root); }

void show(std::vector<uint8_t> png) {
    clear();
    if (!root || png.empty()) return;
    lv_obj_t *c = card(kBig);
    image(c, std::move(png));
    lay_out();
    lv_obj_set_hidden(root, false);
    pop(c, 96);
}

void choose(std::vector<LessonPicture> choose) {
    clear();
    tapped = false;
    if (!root || choose.size() < 2) return; // nothing to choose between
    for (auto &picture : choose) {
        lv_obj_t *option = card(kSmall);
        lv_obj_set_clickable(option, true);
        image(option, std::move(picture.png));
        // Its id rides along in the event's user data: freed with the card.
        auto *id = new std::string(std::move(picture.id));
        lv_obj_add_event_cb(option, on_tapped, LV_EVENT_CLICKED, id);
        lv_obj_add_event_cb(option, [](lv_event_t *e) { delete static_cast<std::string *>(lv_event_get_user_data(e)); }, LV_EVENT_DELETE, id);
    }
    lay_out();
    lv_obj_set_hidden(root, false);
    for (lv_obj_t *option : shown) pop(option, 160);
}

void set_turn(int quarters) {
    turn_ = quarters & 3;
    if (root) lay_out();
}

} // namespace cubey::cards
