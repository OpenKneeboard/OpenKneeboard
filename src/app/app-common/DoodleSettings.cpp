#include <OpenKneeboard/DoodleSettings.h>

namespace OpenKneeboard {

void from_json(const nlohmann::json& j, DoodleSettings& ds) {
  ds.DrawRadius = j.at("drawRadius");
  ds.EraseRadius = j.at("eraseRadius");
  ds.FixedDraw = j.at("fixedDraw");
  ds.FixedErase = j.at("fixedErase");
}
void to_json(nlohmann::json& j, const DoodleSettings& ds) {
  j = {
    {"drawRadius", ds.DrawRadius},
    {"eraseRadius", ds.EraseRadius},
    {"fixedDraw", ds.FixedDraw},
    {"fixedErase", ds.FixedErase}};
}

}// namespace OpenKneeboard