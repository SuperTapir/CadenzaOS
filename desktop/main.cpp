#include <SDL3/SDL.h>

#include <cstdio>

#include "cadenza/core/version.h"

int main(int, char**) {
  std::printf("Cadenza desktop host %s (SDL %d.%d.%d)\n",
              cadenza::coreVersion(), SDL_MAJOR_VERSION, SDL_MINOR_VERSION,
              SDL_MICRO_VERSION);
  return 0;
}
