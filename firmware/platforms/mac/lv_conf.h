/* LVGL settings for the Mac simulator and host tests. Anything not set here takes LVGL's default. */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565 /* as Puce's board's panel */
#define LV_DEF_REFR_PERIOD 33

#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#define LV_USE_SNAPSHOT 1 /* --screenshot */
#define LV_USE_LODEPNG 1  /* her word game's pictures; its encoder writes the screenshot */
#define LV_CACHE_DEF_SIZE (1024 * 1024) /* decoded pictures, as on the board */

#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
#define LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_DIRECT
#define LV_SDL_DIRECT_EXIT 1

#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_DEMOS 0

#endif /* LV_CONF_H */
