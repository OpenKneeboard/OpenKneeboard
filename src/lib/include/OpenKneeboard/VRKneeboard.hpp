// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/SHM.hpp>
#include <OpenKneeboard/VRSettings.hpp>

#include <directxtk/SimpleMath.h>

#include <chrono>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace OpenKneeboard {

class VRKneeboard {
 public:
  using Matrix = DirectX::SimpleMath::Matrix;
  using Quaternion = DirectX::SimpleMath::Quaternion;
  using Vector2 = DirectX::SimpleMath::Vector2;
  using Vector3 = DirectX::SimpleMath::Vector3;

  struct Pose {
    Vector3 mPosition {};
    Quaternion mOrientation {};
  };

  struct ControllerInput {
    bool mValid {false};
    uint8_t mHandIndex {1};
    Pose mAimPose {};
    bool mGripHeld {false};
    bool mTriggerHeld {false};
    float mThumbstickY {0.0f};
  };

  enum class EditCorner : uint8_t {
    None = 0,
    NW = 1,
    NE = 2,
    SW = 3,
    SE = 4,
  };

  struct ManipulationVisuals {
    bool mEditModeActive {false};
    bool mContentPointerActive {false};
    bool mDragging {false};
    bool mHasRay {false};
    Vector3 mRayStartWorld {};
    Vector3 mRayEndWorld {};
    bool mHasHitOnBoard {false};
    uint64_t mTargetLayerID {0};
    float mHitLocalU {0.5f};
    float mHitLocalV {0.5f};
    EditCorner mHoveredCorner {EditCorner::None};
  };

  struct RenderParameters {
    Pose mKneeboardPose;
    Vector2 mKneeboardSize;
    float mKneeboardOpacity;
    bool mIsLookingAtKneeboard;
  };

  struct Layer {
    const SHM::LayerConfig* mLayerConfig {nullptr};
    RenderParameters mRenderParameters;
  };

 protected:
  void UpdateControllerManipulation(
    const SHM::Config& config,
    const std::span<const SHM::LayerConfig>& layers,
    const Pose& hmdPose,
    const std::optional<Pose>& handMountPose,
    const ControllerInput& controller,
    float dtSeconds);

  void UpdateControllerManipulation(
    const SHM::Config& config,
    const std::span<const SHM::LayerConfig>& layers,
    const Pose& hmdPose,
    const std::optional<Pose>& handMountPose,
    std::span<const ControllerInput> controllers,
    float dtSeconds);

  [[nodiscard]]
  VRRenderSettings GetEffectiveVRSettings(
    const VRRenderSettings& shmVR) const noexcept;

  [[nodiscard]]
  bool IsEditModeActive() const noexcept {
    return mManipulation.mEditModeActive;
  }

  [[nodiscard]]
  bool IsManipulationDragging() const noexcept {
    return mManipulation.mDragMode != ManipulationState::DragMode::None;
  }

  [[nodiscard]]
  EditCorner GetHoveredCorner() const noexcept {
    return mManipulation.mHoveredCorner;
  }

  [[nodiscard]]
  const ManipulationVisuals& GetManipulationVisuals() const noexcept {
    return mManipulation.mVisuals;
  }

  std::vector<Layer> GetLayers(
    const SHM::Config&,
    const std::span<const SHM::LayerConfig>&,
    const Pose& hmdPose,
    float dtSeconds = 0.016f,
    const std::optional<Pose>& handMountPose = std::nullopt);

 private:
  RenderParameters GetRenderParameters(
    const SHM::Config&,
    const SHM::LayerConfig&,
    const Pose& hmdPose,
    float dtSeconds,
    const std::optional<Pose>& handMountPose);

  struct Sizes {
    Vector2 mNormalSize;
    Vector2 mZoomedSize;
  };

  struct FollowState {
    bool mInitialized {false};
    bool mEngaged {false};
    Vector3 mAnchorHmdPosition {};
    Pose mCurrentPose {};
    VRFollowMode mLastEffectiveMode {VRFollowMode::HeadFollowLazy};
    float mLastHeadOffsetX {0.0f};
    float mLastHeadOffsetY {0.0f};
    float mLastHeadOffsetZ {0.0f};
    float mLastHeadPitchDeg {0.0f};
    float mLastHeadYawDeg {0.0f};
    float mLastHandOffsetX {0.0f};
    float mLastHandOffsetY {0.0f};
    float mLastHandOffsetZ {0.0f};
    float mLastHandPitchDeg {0.0f};
    float mLastHandForwardX {0.0f};
    float mLastHandForwardY {0.0f};
    float mLastHandForwardZ {1.0f};
  };

  struct ManipulationState {
    enum class DragMode : uint8_t {
      None = 0,
      Move = 1,
      ResizeNW = 2,
      ResizeNE = 3,
      ResizeSW = 4,
      ResizeSE = 5,
    };

    bool mEditModeActive {false};
    EditCorner mHoveredCorner {EditCorner::None};
    bool mHoveredSurface {false};
    DragMode mDragMode {DragMode::None};
    uint8_t mActiveHandIndex {1};
    uint64_t mTargetLayerID {0};
    uint8_t mTargetKneeboardIndex {0};

    float mGrabDistanceM {0.5f};
    float mGrabHandRadiusM {0.18f};
    float mGrabLocalU {0.5f};
    float mGrabLocalV {0.5f};
    Vector3 mGrabPositionOffset {};
    Quaternion mGrabHandOrbitOffset {};
    Quaternion mGrabControllerInv {};
    Quaternion mGrabBoardStartOrient {};
    Vector3 mGrabAnchorCornerWorld {};
    Vector2 mGrabStartSize {};
    float mGrabZoomFactor {1.0f};
    bool mWasTriggerDown {false};
    bool mWasContentTriggerDown {false};
    bool mHasLastContentPointer {false};
    float mLastContentU {0.5f};
    float mLastContentV {0.5f};
    std::chrono::steady_clock::time_point mLastContentSendTime {};
    std::chrono::steady_clock::time_point mLastContentWheelTime {};

    bool mHasOptimisticVR {false};
    VRHeadFollowSettings mOptimisticHeadFollow {};
    VRHeadFollowSettings mLastSeenSHMHeadFollow {};
    bool mHasOptimisticPose {false};
    VRPose mOptimisticWorldPose {};
    Vector2 mOptimisticSize {};
    bool mHasOptimisticSize {false};
    std::chrono::steady_clock::time_point mLastChangeTime {};
    std::chrono::steady_clock::time_point mLastPersistTime {};
    bool mDirtyPendingPersist {false};

    ManipulationVisuals mVisuals {};
  };

  uint64_t mRecenterCount = 0;
  Matrix mRecenter = Matrix::Identity;
  std::unordered_map<uint64_t, bool> mIsLookingAtKneeboard;
  std::unordered_map<uint64_t, FollowState> mFollowStates;
  std::optional<float> mEyeHeight;
  ManipulationState mManipulation;

  Pose GetKneeboardPose(
    const VRRenderSettings& vr,
    const SHM::LayerConfig&,
    const Pose& hmdPose,
    float dtSeconds,
    const std::optional<Pose>& handMountPose);

  Vector2 GetKneeboardSize(
    const SHM::Config& config,
    const SHM::LayerConfig&,
    bool isLookingAtKneeboard);

  bool IsLookingAtKneeboard(
    const SHM::Config&,
    const SHM::LayerConfig&,
    const Pose& hmdPose,
    const Pose& kneeboardPose);

  Sizes GetSizes(const VRRenderSettings&, const SHM::LayerConfig&) const;

  void MaybeRecenter(const VRRenderSettings& vr, const Pose& hmdPose);
  void Recenter(const VRRenderSettings& vr, const Pose& hmdPose);
  void SolvePoseIntoSettings(
    VRHeadFollowSettings& hf,
    VRPose& worldPose,
    const Pose& targetPose,
    const Pose& hmdPose,
    const std::optional<Pose>& handMountPose) const;
  void SendManipulatedTransformEvent(bool force);
};

}// namespace OpenKneeboard
