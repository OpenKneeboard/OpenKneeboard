#include "YAVRK/SHM.h"

#include <Windows.h>

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
    .Width = 400,
    .Height = 1200,
  };
  auto shm = YAVRK::SHM::GetOrCreate(config);
  int64_t frames = -1;
  do {
    frames++;
    uint32_t color;
    for (auto y = 0; y < config.Height; y++) {
      for (auto x = 0; x < config.Width; x++) {
        if (y < config.Height/ 4) {
          color = colors[frames % 4];
        } else if (y < config.Height/ 2) {
          color = colors[(frames + 1) % 4];
        } else if (y < 3 * config.Height / 4) {
          color = colors[(frames + 2) % 4];
        } else {
          color = colors[(frames + 3) % 4];
        }

        void* dest = &shm.ImageData()[4 * ((y * config.Width) + x)];
        memcpy(dest, (void*)&color, sizeof(color));
      }
    }

    Sleep(1000);
  } while (true);
  return 0;
}