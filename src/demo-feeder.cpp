#define _USE_MATH_DEFINES
#include <Windows.h>

#include <cmath>

#include "YAVRK/ConsoleLoopCondition.h"
#include "YAVRK/SHM.h"

int main() {
  using Pixel = YAVRK::SHM::Pixel;
  Pixel colors[] = {
    {0xff, 0x00, 0x00, 0xff},// red
    {0x00, 0xff, 0x00, 0xff},// green
    {0x00, 0x00, 0xff, 0xff},// blue
    {0x00, 0x00, 0x00, 0xff},// black
  };

  YAVRK::SHM::Header config {
    /* Headlocked
    .Flags = YAVRK::Flags::HEADLOCKED | YAVRK::Flags::DISCARD_DEPTH_INFORMATION,
    .y = -0.15,
    .z = -0.5f,
    .VirtualWidth = 0.2f,
    .VirtualHeight = 0.3f,
    .ImageWidth = 400,
    .ImageHeight = 1200,
    */
    /* On knee */
    .Flags = YAVRK::Flags::DISCARD_DEPTH_INFORMATION,
    .y = 0.5f,
    .z = -0.25f,
    .rx = float(M_PI / 2),
    .VirtualWidth = 0.2f,
    .VirtualHeight = 0.3f,
    .ImageWidth = 400,
    .ImageHeight = 1200,
  };
  uint64_t frames = -1;
  printf("Feeding YAVRK - hit Ctrl-C to exit.\n");
  std::vector<Pixel> pixels(config.ImageWidth * config.ImageHeight);
  YAVRK::SHM::Writer shm;
  YAVRK::ConsoleLoopCondition cliLoop;
  do {
    frames++;
    Pixel pixel;
    for (uint16_t y = 0; y < config.ImageHeight; y++) {
      for (uint16_t x = 0; x < config.ImageWidth; x++) {
        if (y < config.ImageHeight / 4) {
          pixel = colors[frames % 4];
        } else if (y < config.ImageHeight / 2) {
          pixel = colors[(frames + 1) % 4];
        } else if (y < 3 * config.ImageHeight / 4) {
          pixel = colors[(frames + 2) % 4];
        } else {
          pixel = colors[(frames + 3) % 4];
        }

        void* dest = &pixels[((y * config.ImageWidth) + x)];
        memcpy(dest, (void*)&pixel, sizeof(Pixel));
      }
    }
    shm.Update(config, pixels);
  } while (cliLoop.sleep(std::chrono::seconds(1)));
  printf("Exit requested, cleaning up.\n");
  return 0;
}
