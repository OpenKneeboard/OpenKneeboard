#pragma once

namespace OpenKneeboard {
class DoodleSettings {
 public:
  DoodleSettings();
  int GetDrawRadius();
  int GetEraseRadius();
  bool GetFixedDraw();
  bool GetFixedErase();

 private:
  int DrawRadius;
  int EraseRadius;
  bool FixedDraw;
  bool FixedErase;
  // TODO: stroke colour?
};
}// namespace OpenKneeboard