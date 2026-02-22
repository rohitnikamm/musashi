#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "SDL.h"
#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_log.h"
#include "SDL_pixels.h"
#include "SDL_render.h"
#include "SDL_scancode.h"
#include "SDL_video.h"

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

typedef struct v2_s {
  float x, y;
} v2;

typedef struct v2i_s {
  int32_t x, y;
} v2i;

// SDL Container Object
typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  int32_t pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
} sdl_t;

typedef enum {
  QUIT,
  RUNNING,
  PAUSED,
} game_state_t;

// Musashi Object
typedef struct {
  game_state_t state;
  v2 pos, dir, plane;
} musashi_t;

#define ASSERT(_e, ...)                                                        \
  if (!(_e)) {                                                                 \
    fprintf(stderr, __VA_ARGS__);                                              \
    exit(1);                                                                   \
  }

// Get dot product of two vectors
#define dot(v0, v1)                                                            \
  ({                                                                           \
    const v2 _v0 = (v0), _v1 = (v1);                                           \
    (_v0.x * _v1.x) + (_v0.y * _v1.y);                                         \
  })

#define length(v)                                                              \
  ({                                                                           \
    const v2 _v = (v);                                                         \
    sqrtf(dot(_v, _v));                                                        \
  })

// Convert vector into length
#define normalize(u)                                                           \
  ({                                                                           \
    const v2 _u = (u);                                                         \
    const float l = length(_u);                                                \
    (v2){_u.x / l, _u.y / l};                                                  \
  })

#define min(a, b)                                                              \
  ({                                                                           \
    __typeof__(a) _a = (a), _b = (b);                                          \
    _a < _b ? _a : _b;                                                         \
  })
#define max(a, b)                                                              \
  ({                                                                           \
    __typeof__(a) _a = (a), _b = (b);                                          \
    _a > _b ? _a : _b;                                                         \
  })

// Get sign of number
#define sign(a)                                                                \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    (__typeof__(a))(_a < 0 ? -1 : (_a > 0 ? 1 : 0));                           \
  })

#define MAP_SIZE 8
static uint8_t MAPDATA[MAP_SIZE * MAP_SIZE] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 3,
    0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 2, 0, 4, 4, 0, 1, 1, 0, 0, 0,
    4, 0, 0, 1, 1, 0, 3, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

// Initialize SDL2
bool init_sdl(sdl_t *sdl) {
  // Initialize SDL2 video subsystem
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    SDL_Log("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return false;
  }

  // Create a window
  sdl->window = SDL_CreateWindow("game window",          // Title
                                 SDL_WINDOWPOS_CENTERED, // X position
                                 SDL_WINDOWPOS_CENTERED, // Y position
                                 SCREEN_WIDTH,           // Width
                                 SCREEN_HEIGHT,          // Height
                                 SDL_WINDOW_SHOWN        // Flags
  );
  if (!sdl->window) {
    SDL_Log("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return false;
  }

  // Allow hardware acceleration
  sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
  if (!sdl->renderer) {
    SDL_Log("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
    return false;
  }

  // Create Texture
  sdl->texture = SDL_CreateTexture(sdl->renderer, SDL_PIXELFORMAT_ABGR8888,
                                   SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH,
                                   SCREEN_HEIGHT);
  if (!sdl->texture) {
    SDL_Log("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
    return false;
  }

  return true;
}

// Update window with changes
void update_screen(sdl_t *sdl) {
  SDL_UpdateTexture(sdl->texture, NULL, sdl->pixels, SCREEN_WIDTH * 4);
  SDL_RenderCopyEx(sdl->renderer, sdl->texture, NULL, NULL, 0.0, NULL,
                   SDL_FLIP_VERTICAL);
  SDL_RenderPresent(sdl->renderer);
}

// Cleanup SDL2
void final_cleanup(const sdl_t sdl) {
  SDL_DestroyTexture(sdl.texture);
  SDL_DestroyRenderer(sdl.renderer);
  SDL_DestroyWindow(sdl.window);
  SDL_Quit();
}

// Draw map in screen column
static void verline(int x, int y0, int y1, uint32_t color, sdl_t *sdl) {
  for (int y = y0; y <= y1; y++) {
    sdl->pixels[(y * SCREEN_WIDTH) + x] = color;
  }
}

static void render(musashi_t *musashi, sdl_t *sdl) {
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    // x coords in space from [-1, 1]
    const float xcam = (2 * (x / (float)(SCREEN_WIDTH))) - 1;

    // compute ray direction through curr column
    const v2 dir = {
        musashi->dir.x + musashi->plane.x * xcam,
        musashi->dir.y + musashi->plane.y * xcam,
    };

    v2 pos = musashi->pos;
    v2i ipos = {(int)pos.x, (int)pos.y};

    // distance ray must travel from one x/y side to the next
    const v2 deltadist = {
        fabsf(dir.x) < 1e-20 ? 1e30 : fabsf(1.0f / dir.x),
        fabsf(dir.y) < 1e-20 ? 1e30 : fabsf(1.0f / dir.y),
    };

    // distance from start position to first x/y side
    v2 sidedist = {
        deltadist.x * (dir.x < 0 ? (pos.x - ipos.x) : (ipos.x + 1 - pos.x)),
        deltadist.y * (dir.y < 0 ? (pos.y - ipos.y) : (ipos.y + 1 - pos.y)),
    };

    // integer step direction for x/y , calculated from overall diff
    const v2i step = {(int)sign(dir.x), (int)sign(dir.y)};

    // DDA hit
    struct {
      int val, side;
      v2 pos;
    } hit = {0, 0, {0.0f, 0.0f}};

    // keep stepping until we hit a wall
    while (!hit.val) {
      if (sidedist.x < sidedist.y) {
        sidedist.x += deltadist.x;
        ipos.x += step.x;
        hit.side = 0;
      } else {
        sidedist.y += deltadist.y;
        ipos.y += step.y;
        hit.side = 1;
      }

      ASSERT(ipos.x >= 0 && ipos.x < MAP_SIZE && ipos.y >= 0 &&
                 ipos.y < MAP_SIZE,
             "DDA out of bounds");

      hit.val = MAPDATA[ipos.y * MAP_SIZE + ipos.x];
    }

    uint32_t color;
    switch (hit.val) {
    case 1:
      color = 0xFF0000FF;
      break;
    case 2:
      color = 0xFF00FF00;
      break;
    case 3:
      color = 0xFFFF0000;
      break;
    case 4:
      color = 0xFFFF00FF;
      break;
    }

    // darken colors on y-sides
    if (hit.side == 1) {
      const uint32_t br = ((color & 0xFF00FF) * 0xC0) >> 8,
                     g = ((color & 0x00FF00) * 0xC0) >> 8;
      color = 0xFF000000 | (br & 0xFF00FF) | (g & 0x00FF00);
    }

    hit.pos = (v2){pos.x + sidedist.x, pos.y + sidedist.y};

    // distance to hit
    const float dperp =
        hit.side == 0 ? (sidedist.x - deltadist.x) : (sidedist.y - deltadist.y);

    // perform perspective division, calculate line height relative to screen
    // center
    const int h = (int)(SCREEN_HEIGHT / dperp),
              y0 = max((SCREEN_HEIGHT / 2) - (h / 2), 0),
              y1 = min((SCREEN_HEIGHT / 2) + (h / 2), SCREEN_HEIGHT - 1);

    verline(x, 0, y0, 0xFF202020, sdl);
    verline(x, y0, y1, color, sdl);
    verline(x, y1, SCREEN_HEIGHT - 1, 0xFF505050, sdl);
  }
}

static void rotate(float rot, musashi_t *musashi) {
  const v2 d = musashi->dir, p = musashi->plane;
  musashi->dir.x = d.x * cos(rot) - d.y * sin(rot);
  musashi->dir.y = d.x * sin(rot) + d.y * cos(rot);
  musashi->plane.x = p.x * cos(rot) - p.y * sin(rot);
  musashi->plane.y = p.x * sin(rot) + p.y * cos(rot);
}

void handle_input(musashi_t *musashi) {
  SDL_Event event;

  const float rotspeed = 1.0f * 0.016f;
  const float movespeed = 1.0f * 0.016f;
  const uint8_t *keystate = SDL_GetKeyboardState(NULL);

  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      // Exit window: end program
      musashi->state = QUIT;
      return;
    default:
      break;
    }
  }

  // TODO: use switch
  if (keystate[SDL_SCANCODE_LEFT]) {
    rotate(+rotspeed, musashi);
  }
  if (keystate[SDL_SCANCODE_RIGHT]) {
    rotate(-rotspeed, musashi);
  }
  if (keystate[SDL_SCANCODE_UP]) {
    musashi->pos.x += musashi->dir.x * movespeed;
    musashi->pos.y += musashi->dir.y * movespeed;
  }
  if (keystate[SDL_SCANCODE_DOWN]) {
    musashi->pos.x -= musashi->dir.x * movespeed;
    musashi->pos.y -= musashi->dir.y * movespeed;
  }
}

// Set game defaults
bool init_musashi(musashi_t *musashi) {
  musashi->pos = (v2){2, 2};
  musashi->dir = normalize(((v2){-1.0f, 0.1f}));
  musashi->plane = (v2){0.0f, 0.66f};
  musashi->state = RUNNING;
  return true;
}

int main() {
  // Initialize SDL
  sdl_t sdl = {0};
  if (!init_sdl(&sdl))
    exit(EXIT_FAILURE);

  // Initialize Game
  musashi_t musashi = {0};
  if (!init_musashi(&musashi))
    exit(EXIT_FAILURE);

  // Main emulator loop
  while (musashi.state != QUIT) {
    handle_input(&musashi);

    // Clear pixel buffer every frame
    memset(sdl.pixels, 0, sizeof(sdl.pixels));

    render(&musashi, &sdl);
    update_screen(&sdl);

    // NOTE: Mac needs events to believe app not hung
    SDL_PumpEvents();
  }

  final_cleanup(sdl);
  exit(EXIT_SUCCESS);

  return 0;
}
