#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "SDL.h"
#include "SDL_events.h"
#include "SDL_log.h"
#include "SDL_render.h"
#include "SDL_timer.h"
#include "SDL_video.h"

// SDL Container Object
typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
} sdl_t;

typedef enum {
  QUIT,
  RUNNING,
  PAUSED,
} game_state_t;

// Musashi Object
typedef struct {
  game_state_t state;
} musashi_t;

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
                                 640,                    // Width
                                 480,                    // Height
                                 SDL_WINDOW_SHOWN        // Flags
  );

  // Allow hardware acceleration
  sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);

  // Check if window creation failed
  if (!sdl->window) {
    SDL_Log("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return false;
  }

  return true;
}

// Update window with changes
void update_screen(sdl_t *sdl) {
  SDL_RenderClear(sdl->renderer);
  SDL_RenderPresent(sdl->renderer);
}

// Cleanup SDL2
void final_cleanup(const sdl_t sdl) {
  SDL_DestroyWindow(sdl.window);
  SDL_Quit();
}

void handle_input(musashi_t *musashi) {
  SDL_Event event;

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
}

bool init_musashi(musashi_t *musashi) {

  // Set game defaults
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

    // Draw to screen at ~60Hz
    SDL_Delay(16);

    // NOTE: Mac needs events to believe app not hung
    SDL_PumpEvents();
  }

  final_cleanup(sdl);
  exit(EXIT_SUCCESS);

  return 0;
}
