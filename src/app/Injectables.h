#pragma once
namespace OpenKneeboard {
struct Injectable {
  const char* Name;
  const char* BuildPath;
};
}// namespace OpenKneeboard

namespace OpenKneeboard::Injectables {

extern const Injectable Oculus_D3D11;

}
