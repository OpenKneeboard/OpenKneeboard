#define _USE_MATH_DEFINES
#include <Windows.h>

#include <cmath>

#include "YAVRK/SHM.h"

int main() {
  // - the data format is RGBA
  // - x64 is little-endian
  // ... so our literals need to be in ABGR form.
  std::uint32_t colors[] = {
    0xff0000ff,// red
    0xff00ff00,// green
    0xffff0000,// blue
    0xff000000,// black
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
    .y = 0.5,
    .z = -0.25f,
    .rx = M_PI / 2,
    .VirtualWidth = 0.2f,
    .VirtualHeight = 0.3f,
    .ImageWidth = 400,
    .ImageHeight = 1200,
  };
  auto shm = YAVRK::SHM::GetOrCreate(config);
  int64_t frames = -1;
  do {
    frames++;
    uint32_t color;
    for (auto y = 0; y < config.ImageHeight; y++) {
      for (auto x = 0; x < config.ImageWidth; x++) {
        if (y < config.ImageHeight / 4) {
          color = colors[frames % 4];
        } else if (y < config.ImageHeight / 2) {
          color = colors[(frames + 1) % 4];
        } else if (y < 3 * config.ImageHeight / 4) {
          color = colors[(frames + 2) % 4];
        } else {
          color = colors[(frames + 3) % 4];
        }

        void* dest = &shm.ImageData()[4 * ((y * config.ImageWidth) + x)];
        memcpy(dest, (void*)&color, sizeof(color));
      }
    }

    Sleep(1000);
  } while (true);
  return 0;
}