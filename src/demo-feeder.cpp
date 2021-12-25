#define _USE_MATH_DEFINES
#include <Windows.h>

#include <cmath>

#include "YAVRK/SHM.h"

int main() {
  using Pixel = YAVRK::SHM::Pixel;
  Pixel colors[] = {
    { 0xff, 0x00, 0x00, 0xff }, // red
    { 0x00, 0xff, 0x00, 0xff }, // green
    { 0x00, 0x00, 0xff, 0xff }, // blue
    { 0x00, 0x00, 0x00, 0xff }, // black
  };

  YAVRK::SHMHeader config {
    .Version = YAVRK::IPC_VERSION,
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
  auto shm = YAVRK::SHM::GetOrCreate(config);
  int64_t frames = -1;
  printf("Acquired SHM, feeding YAVRK - hit Ctrl-C to exit.\n");
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

        void* dest = &shm.ImageData()[((y * config.ImageWidth) + x)];
        memcpy(dest, (void*)&pixel, sizeof(Pixel));
      }
    }
    Sleep(1000);
  } while (true);
  return 0;
}
