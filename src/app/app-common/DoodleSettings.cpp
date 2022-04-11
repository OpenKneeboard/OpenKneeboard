#include <OpenKneeboard/DoodleSettings.h>

namespace OpenKneeboard {

DoodleSettings::DoodleSettings() {
  DrawRadius = 15;
  EraseRadius = 50;
  FixedDraw = false;
  FixedErase = true;
}

int DoodleSettings::GetDrawRadius() {
  return DrawRadius;
}

int DoodleSettings::GetEraseRadius() {
  return EraseRadius;
}

bool DoodleSettings::GetFixedDraw() {
  return FixedDraw;
}

bool DoodleSettings::GetFixedErase() {
  return FixedErase;
}

}// namespace OpenKneeboard