#include <OpenKneeboard/DoodleSettings.h>

#include <nlohmann/json.hpp>

namespace OpenKneeboard {

void from_json(const nlohmann::json& j, DoodleSettings& ds) {
  ds.drawRadius = j.at("drawRadius");
  ds.eraseRadius = j.at("eraseRadius");
  ds.fixedDraw = j.at("fixedDraw");
  ds.fixedErase = j.at("fixedErase");
}
void to_json(nlohmann::json& j, const DoodleSettings& ds) {
  j = {
    {"drawRadius", ds.drawRadius},
    {"eraseRadius", ds.eraseRadius},
    {"fixedDraw", ds.fixedDraw},
    {"fixedErase", ds.fixedErase}};
}

}// namespace OpenKneeboard