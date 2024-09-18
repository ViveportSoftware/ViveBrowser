/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeviceDelegateWaveVR.h"
#include "DeviceUtils.h"
#include "ElbowModel.h"
#include "GestureDelegate.h"

#include "vrb/CameraEye.h"
#include "vrb/Color.h"
#include "vrb/ConcreteClass.h"
#include "vrb/CreationContext.h"
#include "vrb/FBO.h"
#include "vrb/GLError.h"
#include "vrb/Matrix.h"
#include "vrb/RenderContext.h"
#include "vrb/Vector.h"
#include "vrb/RenderState.h"
#include "DeviceDelegate.h"
#include "HandConstant.h"

#include "HandManager.h"
#include "ControllerManager.h"

#include <array>

#include <wvr/wvr.h>
#include <wvr/wvr_render.h>
#include <wvr/wvr_device.h>
#include <wvr/wvr_projection.h>
#include <wvr/wvr_overlay.h>
#include <wvr/wvr_system.h>
#include <wvr/wvr_events.h>
#include <wvr/wvr_arena.h>
#include <wvr/wvr_ctrller_render_model.h>

#include <wvr/wvr_hand.h>
#include <wvr/wvr_hand_render_model.h>

namespace crow {

    #define DEFAULT_EYE_BUFFER_WIDTH 1440
    #define DEFAULT_EYE_BUFFER_HEIGHT 1440


// #define VRB_WAVE_EVENT_LOG_ENABLED 1
#if defined(VRB_WAVE_EVENT_LOG_ENABLED)
#  define VRB_WAVE_EVENT_LOG(x) VRB_DEBUG(x)
#else
#  define VRB_WAVE_EVENT_LOG(x)
#endif // VRB_WAVE_EVENT_DEBUG

#define SCALE_FACTOR_VIEWPORT 1.0f
#define SCALE_FACTOR_IMMERSIVE 1.0f
#define SCALE_FACTOR_TEXTURE 1.0f
#define SCALE_FACTOR_NATIVE 1.0f

struct DeviceDelegateWaveVR::State {
  vrb::RenderContextWeak context;
  bool isRunning;
  vrb::Color clearColor;
  float near;
  float far;
  float foveatedFov;
  void* leftTextureQueue;
  void* rightTextureQueue;
  device::RenderMode renderMode;
  int32_t leftFBOIndex;
  int32_t rightFBOIndex;
  vrb::FBOPtr currentFBO;
  std::vector<vrb::FBOPtr> leftFBOQueue;
  std::vector<vrb::FBOPtr> rightFBOQueue;
  vrb::CameraEyePtr cameras[2];
  uint32_t renderWidth;
  uint32_t renderHeight;

  WVR_DevicePosePair_t devicePairs[1 + kMaxControllerCount]; // HMD, 2 controllers, 2 hands
  ControllerDelegatePtr delegate;
  GestureDelegatePtr gestures;
  std::array<Controller, kMaxControllerCount> controllers;
  ImmersiveDisplayPtr immersiveDisplay;
  device::DeviceType deviceType;
  bool lastSubmitDiscarded;
  bool recentered;
  vrb::Matrix reorientMatrix;
  bool ignoreNextRecenter;
  int32_t sixDoFControllerCount;

  HandManager *handManager;
  ControllerManager *controllerManager;

  State()
      : isRunning(true)
      , near(0.1f)
      , far(100.f)
      , foveatedFov(0.0f)
      , renderMode(device::RenderMode::StandAlone)
      , leftFBOIndex(0)
      , rightFBOIndex(0)
      , leftTextureQueue(nullptr)
      , rightTextureQueue(nullptr)
      , renderWidth(0)
      , renderHeight(0)
      , devicePairs {}
      , controllers {}
      , deviceType(device::UnknownType)
      , lastSubmitDiscarded(false)
      , recentered(false)
      , ignoreNextRecenter(false)
      , sixDoFControllerCount(0)
      , handManager(nullptr)
  {
    memset((void*)devicePairs, 0, sizeof(WVR_DevicePosePair_t) * (kMaxControllerCount+1));
    gestures = GestureDelegate::Create();
    for (int32_t index = 0; index < kMaxControllerCount; index++) {
      controllers[index].index = index;
      controllers[index].type = controllersInfo[index].type;
      controllers[index].hand = controllersInfo[index].hand;
      controllers[index].interactionMode = controllersInfo[index].interactiveMode;

      controllers[index].is6DoF = WVR_GetDegreeOfFreedom(controllers[index].type) == WVR_NumDoF_6DoF;
      if (controllers[index].is6DoF) {
        sixDoFControllerCount++;
      }
    }
    if (sixDoFControllerCount) {
      deviceType = device::ViveFocusPlus;
    } else {
      deviceType = device::ViveFocus;
    }
    reorientMatrix = vrb::Matrix::Identity();
  }

  void FillFBOQueue(void* aTextureQueue, std::vector<vrb::FBOPtr>& aFBOQueue) {
    vrb::FBO::Attributes attributes;
    attributes.samples = 4;
    vrb::RenderContextPtr render = context.lock();
    for (int ix = 0; ix < WVR_GetTextureQueueLength(aTextureQueue); ix++) {
      vrb::FBOPtr fbo = vrb::FBO::Create(render);
      uintptr_t handle = (uintptr_t)WVR_GetTexture(aTextureQueue, ix).id;
      fbo->SetTextureHandle((GLuint)handle, renderWidth, renderHeight, attributes);
      if (fbo->IsValid()) {
        aFBOQueue.push_back(fbo);
      } else {
        VRB_ERROR("FAILED to make valid FBO");
      }
    }
  }

  void InitializeCameras() {
    for (WVR_Eye eye : {WVR_Eye_Left, WVR_Eye_Right}) {
      const device::Eye deviceEye = eye == WVR_Eye_Left ? device::Eye::Left : device::Eye::Right;
      vrb::Matrix eyeOffset = vrb::Matrix::FromRowMajor(WVR_GetTransformFromEyeToHead(eye, WVR_NumDoF_6DoF).m);
      cameras[device::EyeIndex(deviceEye)]->SetEyeTransform(eyeOffset);

      float left, right, top, bottom;
      WVR_GetClippingPlaneBoundary(eye, &left, &right, &top, &bottom);
      const float fovLeft = -atan(left);
      const float fovRight = atan(right);
      const float fovTop = atan(top);
      const float fovBottom = -atan(bottom);

      // We wanna use 1/3 fovX degree as the foveated fov.
      foveatedFov = (fovLeft + fovRight) * 180.0f / (float)M_PI / 3.0f;
      vrb::Matrix projection = vrb::Matrix::PerspectiveMatrix(fovLeft, fovRight, fovTop, fovBottom, near, far);
      cameras[device::EyeIndex(deviceEye)]->SetPerspective(projection);

      if (immersiveDisplay) {
        vrb::Vector translation = eyeOffset.GetTranslation();
        immersiveDisplay->SetEyeOffset(deviceEye, translation.x(), translation.y(), translation.z());
        const float toDegrees = 180.0f / (float)M_PI;
        immersiveDisplay->SetFieldOfView(deviceEye, fovLeft * toDegrees, fovRight * toDegrees, fovTop * toDegrees, fovBottom * toDegrees);
      }
    }
  }

  void Initialize() {
    vrb::RenderContextPtr localContext = context.lock();
    if (!localContext) {
      return;
    }
    vrb::CreationContextPtr create = localContext->GetRenderThreadCreationContext();
    cameras[device::EyeIndex(device::Eye::Left)] = vrb::CameraEye::Create(create);
    cameras[device::EyeIndex(device::Eye::Right)] = vrb::CameraEye::Create(create);
    InitializeCameras();

    // Set the input keys that we are using.
    WVR_InputAttribute inputIdAndTypes[] = {
            {WVR_InputId_Alias1_Menu, WVR_InputType_Button, WVR_AnalogType_None},
            {WVR_InputId_Alias1_Touchpad, WVR_InputType_Button | WVR_InputType_Touch | WVR_InputType_Analog, WVR_AnalogType_2D},
            {WVR_InputId_Alias1_Trigger, WVR_InputType_Button , WVR_AnalogType_None},
            {WVR_InputId_Alias1_Bumper, WVR_InputType_Button , WVR_AnalogType_None},
            {WVR_InputId_Alias1_Grip, WVR_InputType_Button , WVR_AnalogType_None},
            {WVR_InputId_Alias1_A, WVR_InputType_Button , WVR_AnalogType_None},
            {WVR_InputId_Alias1_B, WVR_InputType_Button , WVR_AnalogType_None},
            {WVR_InputId_Alias1_X, WVR_InputType_Button , WVR_AnalogType_None},
            {WVR_InputId_Alias1_Y, WVR_InputType_Button , WVR_AnalogType_None},
            // Remove thumbstick buttonID, we still could receive button/touch event from WVR_InputId_Alias1_Touchpad.
            // Therefore, we can use single buttonID for Focu+ (Touchpad) and Focu3 controllers (Thumbstick)
            //{WVR_InputId_Alias1_Thumbstick, WVR_InputType_Button | WVR_InputType_Touch | WVR_InputType_Analog, WVR_AnalogType_2D},
    };
    WVR_SetInputRequest(WVR_DeviceType_HMD, inputIdAndTypes, sizeof(inputIdAndTypes) / sizeof(*inputIdAndTypes));
    WVR_SetInputRequest(WVR_DeviceType_Controller_Right, inputIdAndTypes, sizeof(inputIdAndTypes) / sizeof(*inputIdAndTypes));
    WVR_SetInputRequest(WVR_DeviceType_Controller_Left, inputIdAndTypes, sizeof(inputIdAndTypes) / sizeof(*inputIdAndTypes));

    uint64_t supported = WVR_GetSupportedFeatures();
    if(supported & WVR_SupportedFeature_HandTracking) {
      handManager = new HandManager(WVR_HandTrackerType_Natural, delegate);
      handManager->onCreate();
    }
    controllerManager = new ControllerManager(delegate, context);
    controllerManager->onCreate();
  }

  void InitializeRender() {
    WVR_GetRenderTargetSize(&renderWidth, &renderHeight);
    renderWidth = DEFAULT_EYE_BUFFER_WIDTH;
    renderHeight = DEFAULT_EYE_BUFFER_HEIGHT;

    VRB_GL_CHECK(glViewport(0, 0, renderWidth * SCALE_FACTOR_VIEWPORT, renderHeight * SCALE_FACTOR_VIEWPORT));
    VRB_DEBUG("Recommended size is %ux%u", renderWidth, renderHeight);
    if (renderWidth == 0 || renderHeight == 0) {
      VRB_ERROR("Please check Wave server configuration");
      return;
    }
    if (immersiveDisplay) {
      immersiveDisplay->SetEyeResolution(renderWidth * SCALE_FACTOR_IMMERSIVE, renderHeight * SCALE_FACTOR_IMMERSIVE);
      immersiveDisplay->SetNativeFramebufferScaleFactor(SCALE_FACTOR_NATIVE);
    }
    InitializeTextureQueues();
  }

  void InitializeTextureQueues() {
    ReleaseTextureQueues();
    VRB_LOG("Create texture queues: %dx%d", renderWidth, renderHeight);
    leftTextureQueue = WVR_ObtainTextureQueue(WVR_TextureTarget_2D, WVR_TextureFormat_RGBA,
                                                WVR_TextureType_UnsignedByte,
                                                renderWidth * SCALE_FACTOR_TEXTURE,
                                                renderHeight * SCALE_FACTOR_TEXTURE, 0);
    FillFBOQueue(leftTextureQueue, leftFBOQueue);
    rightTextureQueue = WVR_ObtainTextureQueue(WVR_TextureTarget_2D, WVR_TextureFormat_RGBA,
                                                 WVR_TextureType_UnsignedByte,
                                                 renderWidth * SCALE_FACTOR_TEXTURE,
                                                 renderHeight * SCALE_FACTOR_TEXTURE, 0);
    FillFBOQueue(rightTextureQueue, rightFBOQueue);
  }

  void ReleaseTextureQueues() {
    if (leftTextureQueue) {
      WVR_ReleaseTextureQueue(leftTextureQueue);
      leftTextureQueue = nullptr;
    }
    leftFBOQueue.clear();
    if (rightTextureQueue) {
      WVR_ReleaseTextureQueue(rightTextureQueue);
      rightTextureQueue = nullptr;
    }
    rightFBOQueue.clear();
  }

  void Shutdown() {
    ReleaseTextureQueues();

    if (nullptr != handManager) {
      handManager->onDestroy();
      delete handManager;
      handManager = nullptr;
    }
    if (nullptr != controllerManager) {
      controllerManager->onDestroy();
      delete controllerManager;
      controllerManager = nullptr;
    }
  }

  void UpdateStandingMatrix() {
    if (!immersiveDisplay) {
      return;
    }
    WVR_PoseState_t head;
    WVR_PoseState_t ground;
    WVR_GetPoseState(WVR_DeviceType_HMD, WVR_PoseOriginModel_OriginOnHead, 0, &head);
    WVR_GetPoseState(WVR_DeviceType_HMD, WVR_PoseOriginModel_OriginOnGround, 0, &ground);
    if (!head.isValidPose || !ground.isValidPose) {
      immersiveDisplay->SetSittingToStandingTransform(vrb::Matrix::Translation(kAverageHeight));
      return;
    }
    const float delta = ground.poseMatrix.m[1][3] - head.poseMatrix.m[1][3];
    immersiveDisplay->SetSittingToStandingTransform(vrb::Matrix::Translation(vrb::Vector(0.0f, delta, 0.0f)));
  }

    void CreateController(Controller &controller) {
      if (!delegate) {
        VRB_ERROR("Failed to create controller. No ControllerDelegate has been set.");
        return;
      }
      vrb::Matrix beamTransform(vrb::Matrix::Identity());
      if (controller.is6DoF) {
        beamTransform.TranslateInPlace(vrb::Vector(0.0f, 0.01f, -0.05f));
      }
      delegate->CreateController(controller.index,
                                 controller.index,
                                 controller.is6DoF ? "HTC Vive 6DoF Controller"
                                                   : "HTC Vive 3DoF Controller",
                                 beamTransform);
      delegate->SetLeftHanded(controller.index, controller.hand == ElbowModel::HandEnum::Left);
      delegate->SetHapticCount(controller.index,
                               controller.interactionMode == WVR_InteractionMode_Controller ? 1
                                                                                            : 0);
      delegate->SetControllerType(controller.index, controller.is6DoF ? device::ViveFocusPlus :
                                                    device::ViveFocus);
      delegate->SetTargetRayMode(controller.index, device::TargetRayMode::TrackedPointer);


      if (controller.is6DoF) {
        const vrb::Matrix trans = vrb::Matrix::Position(vrb::Vector(0.0f, -0.021f, -0.03f));
        vrb::Matrix transform = vrb::Matrix::Rotation(vrb::Vector(1.0f, 0.0f, 0.0f), -0.70f);
        transform = transform.PostMultiply(trans);

        delegate->SetImmersiveBeamTransform(controller.index,
                                            beamTransform.PostMultiply(transform));
      }
      controller.created = true;
      controller.enabled = false;
    }

    void UpdateControllers() {
      if (!delegate) {
        return;
      }

      if (WVR_IsInputFocusCapturedBySystem()) {
        for (Controller &controller: controllers) {
          if (controller.enabled) {
            delegate->SetEnabled(controller.index, false);
            controller.enabled = false;
            VRB_LOG("DeviceDelegate::UpdateControllers: controller.enabled=false, WVR_IsInputFocusCapturedBySystem: true");
          }
        }
        return;
      }

      const WVR_InteractionMode interactionMode = WVR_GetInteractionMode();

      if(handManager != nullptr) {
        handManager->update();
      }

      for (Controller &controller: controllers) {
        const bool is6DoF = WVR_GetDegreeOfFreedom(controller.type) == WVR_NumDoF_6DoF;
        if (controller.is6DoF != is6DoF) {
          controller.is6DoF = is6DoF;
          if (is6DoF) {
            sixDoFControllerCount++;
          } else {
            sixDoFControllerCount--;
          }
          controller.created = false;
        }

        if (!controller.created) {
          VRB_LOG("Creating controller from UpdateControllers");
          CreateController(controller);
        }
        const bool isDeviceConnected = WVR_IsDeviceConnected(controller.type);
        const bool isHandAvailable = handManager? handManager->isHandAvailable(controller.hand) : false;
        if (!isDeviceConnected || interactionMode != controller.interactionMode ||
        (controller.interactionMode == WVR_InteractionMode_Hand && !isHandAvailable)) {
          if (controller.enabled) {
            delegate->SetEnabled(controller.index, false);
            controller.enabled = false;
              VRB_LOG("DeviceDelegate::UpdateControllers: controller.enabled=false, "
                      "isDeviceConnected: %d, WVR_GetInteractionMode: %d, controller.interactionMode: %d, isHandAvailable: %d",
                      isDeviceConnected, interactionMode, controller.interactionMode, isHandAvailable);
          }
          continue;
        }

        if (!controller.enabled) {
          device::CapabilityFlags flags = device::Orientation | device::GripSpacePosition;
          if (controller.is6DoF) {
            flags |= device::Position;
          } else {
            flags |= device::PositionEmulated;
          }
          controller.enabled = true;
          delegate->SetEnabled(controller.index, true);
          delegate->SetCapabilityFlags(controller.index, flags);
          delegate->SetModelVisible(controller.index, true);
          if(controller.interactionMode == WVR_InteractionMode_Controller){
            float level = WVR_GetDeviceBatteryPercentage(controller.type);

            const bool isInputFocusCapturedBySystem = WVR_IsInputFocusCapturedBySystem();
            const bool isDeviceConnected = WVR_IsDeviceConnected(controller.type);
            const bool isHandAvailable = handManager? handManager->isHandAvailable(controller.hand) : false;
            VRB_LOG("DeviceDelegate::UpdateControllers: WVR_GetDeviceBatteryPercentage: level:%f, "
                      "isDeviceConnected: %d, WVR_GetInteractionMode: %d, controller.interactionMode: %d, isHandAvailable: %d",
                    level,isDeviceConnected, interactionMode, controller.interactionMode, isHandAvailable);

            controllerManager->setBattery(controller.index, level);
          }
        }

        if (controller.interactionMode == WVR_InteractionMode_Hand) {
          if(handManager != nullptr) {
            handManager->updateHandState(controller);
          }
        } else {
          controllerManager->updateControllerState(controller);
        }
      }
    }

  void UpdateBoundary() {
    if (!immersiveDisplay) {
      return;
    }
    WVR_Arena_t arena = WVR_GetArena();
    if (arena.shape == WVR_ArenaShape_Rectangle &&
        arena.area.rectangle.width > 0 &&
        arena.area.rectangle.length > 0) {
      immersiveDisplay->SetStageSize(arena.area.rectangle.width, arena.area.rectangle.length);
    } else if (arena.shape == WVR_ArenaShape_Round && arena.area.round.diameter > 0) {
      immersiveDisplay->SetStageSize(arena.area.round.diameter, arena.area.round.diameter);
    } else {
      immersiveDisplay->SetStageSize(0.0f, 0.0f);
    }
  }
};

DeviceDelegateWaveVRPtr
DeviceDelegateWaveVR::Create(vrb::RenderContextPtr& aContext) {
  DeviceDelegateWaveVRPtr result = std::make_shared<vrb::ConcreteClass<DeviceDelegateWaveVR, DeviceDelegateWaveVR::State> >();
  if(result!= nullptr){
      result->m.context = aContext;
      result->m.Initialize();
  }
  else{
      VRB_ERROR("Null pointer, file: %s, function: %s, line: %d",__FILE__, __FUNCTION__, __LINE__);
  }
  return result;
}

void DeviceDelegateWaveVR::InitializeRender() {
  m.InitializeRender();
}

device::DeviceType
DeviceDelegateWaveVR::GetDeviceType() {
  return m.deviceType;
}

void
DeviceDelegateWaveVR::SetRenderMode(const device::RenderMode aMode) {
  if(m.handManager != nullptr) {
    m.handManager->setRenderMode(aMode);
  }
  m.controllerManager->setRenderMode(aMode);

  if (aMode == m.renderMode) {
    return;
  }

  m.renderMode = aMode;
  m.reorientMatrix = vrb::Matrix::Identity();

  uint32_t recommendedWidth, recommendedHeight;
  WVR_GetRenderTargetSize(&recommendedWidth, &recommendedHeight);
  recommendedWidth = DEFAULT_EYE_BUFFER_WIDTH;
  recommendedHeight = DEFAULT_EYE_BUFFER_HEIGHT;
  if (recommendedWidth != m.renderWidth || recommendedHeight != m.renderHeight) {
    m.renderWidth = recommendedWidth;
    m.renderHeight = recommendedHeight;
    m.InitializeTextureQueues();
  }
}

device::RenderMode
DeviceDelegateWaveVR::GetRenderMode() {
  return m.renderMode;
}

void
DeviceDelegateWaveVR::RegisterImmersiveDisplay(ImmersiveDisplayPtr aDisplay) {
  m.immersiveDisplay = std::move(aDisplay);

  if (!m.immersiveDisplay) {
    return;
  }

  m.immersiveDisplay->SetDeviceName("Wave");
  device::CapabilityFlags flags = device::Orientation | device::Present |
                                  device::InlineSession | device::ImmersiveVRSession;

  if (WVR_GetDegreeOfFreedom(WVR_DeviceType_HMD) == WVR_NumDoF_6DoF) {
    flags |= device::Position | device::StageParameters;
  } else {
    flags |= device::PositionEmulated;
  }

  m.immersiveDisplay->SetCapabilityFlags(flags);
  m.immersiveDisplay->SetEyeResolution(m.renderWidth * SCALE_FACTOR_IMMERSIVE, m.renderHeight * SCALE_FACTOR_IMMERSIVE);
  m.UpdateStandingMatrix();
  m.UpdateBoundary();
  m.InitializeCameras();
  m.immersiveDisplay->CompleteEnumeration();
}

void
DeviceDelegateWaveVR::SetImmersiveSize(const uint32_t aEyeWidth, const uint32_t aEyeHeight) {
  uint32_t recommendedWidth, recommendedHeight;
  WVR_GetRenderTargetSize(&recommendedWidth, &recommendedHeight);

  uint32_t targetWidth = m.renderWidth;
  uint32_t targetHeight = m.renderHeight;

  DeviceUtils::GetTargetImmersiveSize(aEyeWidth, aEyeHeight, recommendedWidth, recommendedHeight, targetWidth, targetHeight);
  targetWidth = DEFAULT_EYE_BUFFER_WIDTH;
  targetHeight = DEFAULT_EYE_BUFFER_HEIGHT;

  if (targetWidth != m.renderWidth || targetHeight != m.renderHeight) {
    m.renderWidth = targetWidth;
    m.renderHeight = targetHeight;
    m.InitializeTextureQueues();
  }
}

GestureDelegateConstPtr
DeviceDelegateWaveVR::GetGestureDelegate() {
  return m.gestures;
}
vrb::CameraPtr
DeviceDelegateWaveVR::GetCamera(const device::Eye aWhich) {
  const int32_t index = device::EyeIndex(aWhich);
  if (index < 0) { return nullptr; }
  return m.cameras[index];
}

const vrb::Matrix&
DeviceDelegateWaveVR::GetHeadTransform() const {
  return m.cameras[0]->GetHeadTransform();
}

const vrb::Matrix&
DeviceDelegateWaveVR::GetReorientTransform() const {
  return m.reorientMatrix;
}

void
DeviceDelegateWaveVR::SetReorientTransform(const vrb::Matrix& aMatrix) {
  m.reorientMatrix = aMatrix;
}

void
DeviceDelegateWaveVR::Reorient() {
  vrb::Matrix head = GetHeadTransform();
  m.reorientMatrix = DeviceUtils::CalculateReorientationMatrixOnHeadLock(head, kAverageHeight);
}

void
DeviceDelegateWaveVR::SetClearColor(const vrb::Color& aColor) {
  m.clearColor = aColor;
}

void
DeviceDelegateWaveVR::SetClipPlanes(const float aNear, const float aFar) {
  m.near = aNear;
  m.far = aFar;
  m.InitializeCameras();
}

void
DeviceDelegateWaveVR::SetControllerDelegate(ControllerDelegatePtr& aController) {
  m.delegate = aController;
  if (!m.delegate) {
    return;
  }
  for (Controller& controller: m.controllers) {
    VRB_LOG("Creating controller from SetControllerDelegate");
    m.CreateController(controller);
  }
}

void
DeviceDelegateWaveVR::ReleaseControllerDelegate() {
  m.delegate = nullptr;
}

int32_t
DeviceDelegateWaveVR::GetControllerModelCount() const {
  return kMaxControllerCount; // two controllers, two hands
}

// #define VRB_WAVE_EVENT_LOG_ENABLED 1
#if defined(VRB_WAVE_EVENT_LOG_ENABLED)
#  define VRB_WAVE_EVENT_LOG(x) VRB_DEBUG(x)
#else
#  define VRB_WAVE_EVENT_LOG(x)
#endif // VRB_WAVE_EVENT_DEBUG

void
DeviceDelegateWaveVR::ProcessEvents() {
  WVR_Event_t event;
  m.gestures->Reset();
  while (WVR_PollEventQueue(&event)) {
    WVR_EventType type = event.common.type;
    switch (type) {
      case WVR_EventType_Quit: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_Quit");
        m.isRunning = false;
        return;
      }
      case WVR_EventType_InteractionModeChanged: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_InteractionModeChanged");
      }
        break;
      case WVR_EventType_GazeTriggerTypeChanged: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_GazeTriggerTypeChanged");
      }
        break;
      case WVR_EventType_TrackingModeChanged: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_TrackingModeChanged");
      }
        break;
      case WVR_EventType_DeviceConnected: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_DeviceConnected");
      }
        break;
      case WVR_EventType_DeviceDisconnected: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_DeviceDisconnected");
      }
        break;
      case WVR_EventType_DeviceStatusUpdate: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_DeviceStatusUpdate");
      }
        break;
      case WVR_EventType_IpdChanged: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_IpdChanged");
        m.InitializeCameras();
      }
        break;
      case WVR_EventType_DeviceSuspend: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_DeviceSuspend");
        m.reorientMatrix = vrb::Matrix::Identity();
        m.ignoreNextRecenter = true;
      }
        break;
      case WVR_EventType_DeviceResume: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_DeviceResume");
        m.reorientMatrix = vrb::Matrix::Identity();
        m.UpdateBoundary();
      }
        break;
      case WVR_EventType_DeviceRoleChanged: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_DeviceRoleChanged");
      }
        break;
      case WVR_EventType_BatteryStatusUpdate: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_BatteryStatusUpdate");
      }
        break;
      case WVR_EventType_BatteryPercentageUpdate: {
          VRB_WAVE_EVENT_LOG("WVR_EventType_BatteryPercentageUpdate");
          float level = WVR_GetDeviceBatteryPercentage(event.device.deviceType);
          VRB_LOG("DeviceDelegate::ProcessEvents %d, WVR_GetDeviceBatteryPercentage: level:%f",type ,level);
          switch (event.device.deviceType) {
              case WVR_DeviceType_HMD:
                  break;
              case WVR_DeviceType_Controller_Right:
                  m.controllerManager->setBattery(1, level);
                  break;
              case WVR_DeviceType_Controller_Left:
                  m.controllerManager->setBattery(0, level);
                  break;
              default:
                  break;
          }
      }
        break;
      case WVR_EventType_ChargeStatusUpdate: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_ChargeStatusUpdate");
      }
        break;
      case WVR_EventType_DeviceErrorStatusUpdate: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_DeviceErrorStatusUpdate");
      }
        break;
      case WVR_EventType_BatteryTemperatureStatusUpdate: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_BatteryTemperatureStatusUpdate");
      }
        break;
      case WVR_EventType_RecenterSuccess: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_RecenterSuccess");
        WVR_InAppRecenter(WVR_RecenterType_YawAndPosition);
        m.recentered = !m.ignoreNextRecenter;
        m.controllerManager->setRecentered(!m.ignoreNextRecenter);
        m.ignoreNextRecenter = false;
        m.UpdateStandingMatrix();
      }
        break;
      case WVR_EventType_RecenterFail: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_RecenterFail");
      }
        break;
      case WVR_EventType_RecenterSuccess3DoF: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_RecenterSuccess_3DoF");
        WVR_InAppRecenter(WVR_RecenterType_YawAndPosition);
        m.recentered = !m.ignoreNextRecenter;
        m.controllerManager->setRecentered(!m.ignoreNextRecenter);
        m.ignoreNextRecenter = false;
      }
        break;
      case WVR_EventType_RecenterFail3DoF: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_RecenterFail_3DoF");
      }
        break;
      case WVR_EventType_ButtonPressed: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_ButtonPressed");
      }
        break;
      case WVR_EventType_ButtonUnpressed: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_ButtonUnpressed");
      }
        break;
      case WVR_EventType_TouchTapped: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_TouchTapped");
      }
        break;
      case WVR_EventType_TouchUntapped: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_TouchUntapped");
        break;
      }
      case WVR_EventType_LeftToRightSwipe: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_LeftToRightSwipe");
        m.gestures->AddGesture(GestureType::SwipeRight);
      }
        break;
      case WVR_EventType_RightToLeftSwipe: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_RightToLeftSwipe");
        m.gestures->AddGesture(GestureType::SwipeLeft);
      }
        break;
      case WVR_EventType_DownToUpSwipe: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_DownToUpSwipe");
      }
        break;
      case WVR_EventType_UpToDownSwipe: {
        VRB_WAVE_EVENT_LOG("WVR_EventType_UpToDownSwipe");
      }
        break;
      default: {
        VRB_WAVE_EVENT_LOG("Unknown WVR_EventType");
      }
        break;
    }
  }
  m.UpdateControllers();
}

void
DeviceDelegateWaveVR::StartFrame(const FramePrediction aPrediction) {
  mShouldRender = false;
  VRB_GL_CHECK(glClearColor(m.clearColor.Red(), m.clearColor.Green(), m.clearColor.Blue(), m.clearColor.Alpha()));
  if (!m.lastSubmitDiscarded) {
    m.leftFBOIndex = WVR_GetAvailableTextureIndex(m.leftTextureQueue);
    m.rightFBOIndex = WVR_GetAvailableTextureIndex(m.rightTextureQueue);
  }
  // Update cameras
  WVR_GetSyncPose(WVR_PoseOriginModel_OriginOnHead, m.devicePairs, WVR_DEVICE_COUNT_LEVEL_1);
  vrb::Matrix hmd = vrb::Matrix::Identity();
  if (m.devicePairs[WVR_DEVICE_HMD].pose.isValidPose) {
    hmd = vrb::Matrix::FromRowMajor(m.devicePairs[WVR_DEVICE_HMD].pose.poseMatrix.m);
    if (m.renderMode == device::RenderMode::StandAlone) {
      if (m.controllerManager->isRecentered()) {
        m.reorientMatrix = DeviceUtils::CalculateReorientationMatrix(hmd, kAverageHeight);
      }
      hmd.TranslateInPlace(kAverageHeight);
    }
    m.cameras[device::EyeIndex(device::Eye::Left)]->SetHeadTransform(hmd);
    m.cameras[device::EyeIndex(device::Eye::Right)]->SetHeadTransform(hmd);
  }

  m.recentered = false;
  if (!m.delegate) {
    return;
  }
  for (Controller& controller: m.controllers) {
    const WVR_DevicePosePair_t *devicePair = findDevicePair(controller.type);

    switch (controller.interactionMode) {
      case WVR_InteractionMode_Hand:
        if(m.handManager != nullptr) {
          m.handManager->updateHand(controller, *devicePair, hmd);
        }
        break;
      case WVR_InteractionMode_Controller:
        m.controllerManager->updateController(controller, *devicePair, hmd);
        break;
      default:
        break;
      }
    }
  mShouldRender = true;
}

WVR_DevicePosePair_t *DeviceDelegateWaveVR::findDevicePair(WVR_DeviceType type) {
  for (WVR_DevicePosePair_t &devicePair: m.devicePairs) {
    if (devicePair.type == type) {
      return &devicePair;
    }
  }
  return nullptr;
}

void
DeviceDelegateWaveVR::BindEye(const device::Eye aWhich) {
  if (m.currentFBO) {
    m.currentFBO->Unbind();
  }
  if (aWhich == device::Eye::Left) {
    m.currentFBO = m.leftFBOQueue[m.leftFBOIndex];
  } else if (aWhich == device::Eye::Right) {
    m.currentFBO = m.rightFBOQueue[m.rightFBOIndex];
  } else {
    m.currentFBO = nullptr;
  }
  if (m.currentFBO) {
    m.currentFBO->Bind();
    VRB_GL_CHECK(glViewport(0, 0, m.renderWidth * SCALE_FACTOR_VIEWPORT, m.renderHeight * SCALE_FACTOR_VIEWPORT));
    VRB_GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
  } else {
    VRB_ERROR("No FBO found");
  }
}

void
DeviceDelegateWaveVR::EndFrame(const FrameEndMode aMode) {
  if (m.currentFBO) {
    m.currentFBO->Unbind();
    m.currentFBO = nullptr;
  }

  m.lastSubmitDiscarded = aMode == DeviceDelegate::FrameEndMode::DISCARD;
  if (m.lastSubmitDiscarded) {
    return;
  }
  // Left eye
  WVR_TextureParams_t leftEyeTexture = WVR_GetTexture(m.leftTextureQueue, m.leftFBOIndex);
  WVR_SubmitError result = WVR_SubmitFrame(WVR_Eye_Left, &leftEyeTexture);
  if (result != WVR_SubmitError_None) {
    VRB_ERROR("Failed to submit left eye frame");
  }

  // Right eye
  WVR_TextureParams_t rightEyeTexture = WVR_GetTexture(m.rightTextureQueue, m.rightFBOIndex);
  result = WVR_SubmitFrame(WVR_Eye_Right, &rightEyeTexture);
  if (result != WVR_SubmitError_None) {
    VRB_ERROR("Failed to submit right eye frame");
  }
}

vrb::LoadTask DeviceDelegateWaveVR::GetControllerModelTask(int32_t aModelIndex) {
  vrb::RenderContextPtr localContext = m.context.lock();
  if (!localContext) {
    return nullptr;
  }

  const ControllerMetaInfo controllerMetaInfo = controllersInfo[aModelIndex];

  if (controllerMetaInfo.interactiveMode == WVR_InteractionMode_Hand) {
    if(m.handManager != nullptr) {
      return m.handManager->getHandModelTask(controllerMetaInfo);
    } else {
      return nullptr;
    }
  }
  return m.controllerManager->getControllerModelTask(controllerMetaInfo);
}

bool
DeviceDelegateWaveVR::IsRunning() {
  return m.isRunning;
}

DeviceDelegateWaveVR::DeviceDelegateWaveVR(State& aState) : m(aState) {}
DeviceDelegateWaveVR::~DeviceDelegateWaveVR() {
  m.Shutdown();
}

} // namespace crow
