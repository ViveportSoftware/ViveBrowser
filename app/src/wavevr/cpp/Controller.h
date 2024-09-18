#pragma once

#include "wvr/wvr_types.h"
#include "ElbowModel.h"
#include "vrb/Matrix.h"

namespace crow {
  static const vrb::Vector kAverageHeight(0.0f, 1.7f, 0.0f);
  static const int32_t kMaxControllerCount = 4;

  typedef struct {
    WVR_DeviceType type;
    ElbowModel::HandEnum hand;
    WVR_InteractionMode interactiveMode;
  } ControllerMetaInfo;

  const ControllerMetaInfo controllersInfo[kMaxControllerCount] = {
      {WVR_DeviceType_Controller_Left,   ElbowModel::HandEnum::Left,  WVR_InteractionMode_Controller},
      {WVR_DeviceType_Controller_Right,  ElbowModel::HandEnum::Right, WVR_InteractionMode_Controller},
      {WVR_DeviceType_NaturalHand_Left,  ElbowModel::HandEnum::Left,  WVR_InteractionMode_Hand},
      {WVR_DeviceType_NaturalHand_Right, ElbowModel::HandEnum::Right, WVR_InteractionMode_Hand},
  };

  struct Controller {
    int32_t index;
    WVR_DeviceType type;
    bool created;
    bool enabled;
    WVR_InteractionMode interactionMode;
    bool touched;
    bool is6DoF;
    int32_t gripPressedCount;
    vrb::Matrix transform;
    ElbowModel::HandEnum hand;
    uint64_t inputFrameID;
    float remainingVibrateTime;
    double lastHapticUpdateTimeStamp;

    Controller()
        : index(-1), type(WVR_DeviceType_Controller_Right), created(false), enabled(false),
          touched(false), is6DoF(false), gripPressedCount(0), transform(vrb::Matrix::Identity()),
          hand(ElbowModel::HandEnum::Right), inputFrameID(0), remainingVibrateTime(0.0f),
          lastHapticUpdateTimeStamp(0.0f), interactionMode(WVR_InteractionMode_SystemDefault) {}
  };
}