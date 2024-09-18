#define LOG_TAG "APHandModule"

#include "log.h"

#include "shared/quat.h"
#include "ControllerManager.h"
#include "ControllerDelegate.h"
#include "ElbowModel.h"
#include "vrb/RenderContext.h"
#include "vrb/CreationContext.h"
#include "vrb/Geometry.h"
#include "vrb/Group.h"
#include "vrb/Vector.h"
#include "vrb/Program.h"
#include "vrb/ProgramFactory.h"
#include "vrb/VertexArray.h"
#include "vrb/TextureGL.h"
#include "vrb/Matrix.h"
#include "vrb/RenderState.h"
#include "vrb/Color.h"
#include "wvr/wvr_device.h"
#include <wvr/wvr.h>
#include <wvr/wvr_types.h>

#include <mutex>

namespace crow {
  static const int32_t kRecenterDelay = 72;
  static const int32_t kControllerCount = 2;  // Left, Right

  ControllerManager::ControllerManager(crow::ControllerDelegatePtr &controllerDelegatePtr,
                                       vrb::RenderContextWeak contextPtr) :
      delegate(controllerDelegatePtr), context(contextPtr),
      renderMode(device::RenderMode::StandAlone), recentered(false),
      modelCachedData{}, isModelDataReady{} {
    memset((void *) modelCachedData, 0, sizeof(WVR_CtrlerModel_t *) * kControllerCount);
    memset((void *) isModelDataReady, 0, sizeof(bool) * kControllerCount);
  }

  ControllerManager::~ControllerManager() {
  }

  void ControllerManager::onCreate() {
    elbow = ElbowModel::Create();
  }

  void ControllerManager::onDestroy() {
    for (auto index = 0; index < kMaxControllerCount; index++) {
      if (modelCachedData[index] != nullptr) {
        WVR_ReleaseControllerModel(
            &modelCachedData[index]); //we will clear cached data ptr to nullptr.
      }
    }
  }

  void ControllerManager::updateControllerState(Controller &controller) {
    uint32_t ctl_button = WVR_GetInputDeviceCapability(controller.type, WVR_InputType_Button);
    uint32_t ctl_touch = WVR_GetInputDeviceCapability(controller.type, WVR_InputType_Touch);
//    uint32_t ctl_analog = WVR_GetInputDeviceCapability(controller.type, WVR_InputType_Analog);

    // Flow fix MOHUS
    //const bool bumperPressed = (controller.is6DoF) ? WVR_GetInputButtonState(controller.type, WVR_InputId_Alias1_Trigger)
    //                          : WVR_GetInputButtonState(controller.type, WVR_InputId_Alias1_Bumper);
    const bool bumperPressed = WVR_GetInputButtonState(controller.type,
                                                       WVR_InputId_Alias1_Trigger);

    // ABXY buttons
    if (controller.hand == ElbowModel::HandEnum::Left) {
        const bool xPressed = WVR_GetInputButtonState(controller.type, WVR_InputId_Alias1_X);
        delegate->SetButtonState(controller.index, ControllerDelegate::BUTTON_X,
                                   device::kImmersiveButtonA, xPressed, xPressed);

        const bool yPressed = WVR_GetInputButtonState(controller.type, WVR_InputId_Alias1_Y);
        delegate->SetButtonState(controller.index, ControllerDelegate::BUTTON_Y,
                                   device::kImmersiveButtonB, yPressed, yPressed);
    } else if (controller.hand == ElbowModel::HandEnum::Right) {
        const bool aPressed = WVR_GetInputButtonState(controller.type, WVR_InputId_Alias1_A);
        delegate->SetButtonState(controller.index, ControllerDelegate::BUTTON_A,
                                   device::kImmersiveButtonA, aPressed, aPressed);

        const bool bPressed = WVR_GetInputButtonState(controller.type, WVR_InputId_Alias1_B);
        delegate->SetButtonState(controller.index, ControllerDelegate::BUTTON_B,
                                   device::kImmersiveButtonB, bPressed, bPressed);
    }

    const bool touchpadPressed = WVR_GetInputButtonState(controller.type,
                                                         WVR_InputId_Alias1_Touchpad);
    const bool touchpadTouched = WVR_GetInputTouchState(controller.type,
                                                        WVR_InputId_Alias1_Touchpad);
    const bool menuPressed = WVR_GetInputButtonState(controller.type,
                                                     WVR_InputId_Alias1_Menu);

    // Although Focus only has two buttons, in order to match WebXR input profile (squeeze placeholder),
    // we make Focus has three buttons.
    delegate->SetButtonCount(controller.index, 7);
    delegate->SetButtonState(controller.index, ControllerDelegate::BUTTON_TOUCHPAD,
                             device::kImmersiveButtonTouchpad, touchpadPressed,
                             touchpadTouched);
    delegate->SetButtonState(controller.index, ControllerDelegate::BUTTON_TRIGGER,
                             device::kImmersiveButtonTrigger, bumperPressed, bumperPressed);
    if (controller.is6DoF) {
      const bool gripPressed = WVR_GetInputButtonState(controller.type,
                                                       WVR_InputId_Alias1_Grip);

      if (renderMode == device::RenderMode::StandAlone) {
        if (gripPressed && (controller.gripPressedCount >= 0)) {
          controller.gripPressedCount++;
        } else if (!gripPressed) {
          controller.gripPressedCount = 0;
        }
        if (controller.gripPressedCount > kRecenterDelay) {
          WVR_InAppRecenter(WVR_RecenterType_YawAndPosition);
          recentered = true;
          controller.gripPressedCount = -1;
        }
      } else {
        delegate->SetButtonState(controller.index, ControllerDelegate::BUTTON_SQUEEZE,
                                 device::kImmersiveButtonSqueeze,
                                 gripPressed, gripPressed);
        controller.gripPressedCount = 0;
      }
      if (gripPressed && renderMode == device::RenderMode::Immersive) {
        delegate->SetSqueezeActionStart(controller.index);
      } else {
        delegate->SetSqueezeActionStop(controller.index);
      }
    }

    if (bumperPressed && renderMode == device::RenderMode::Immersive) {
      delegate->SetSelectActionStart(controller.index);
    } else {
      delegate->SetSelectActionStop(controller.index);
    }
    delegate->SetButtonState(controller.index, ControllerDelegate::BUTTON_APP, -1,
                             menuPressed,
                             menuPressed);

    float axisX = 0.0f;
    float axisY = 0.0f;
    if (touchpadTouched) {
      WVR_Axis_t axis = WVR_GetInputAnalogAxis(controller.type, WVR_InputId_Alias1_Touchpad);
      axisX = axis.x;
      axisY = -axis.y;
      // In case we have thumbstick we don't send the touchpad touched event
      if (!(ctl_touch & WVR_InputId_Alias1_Thumbstick)) {
        // We are matching touch pad range from {-1, 1} to the Oculus {0, 1}.
        delegate->SetTouchPosition(controller.index, (axis.x + 1) * 0.5f, (-axis.y + 1) * 0.5f);
        controller.touched = true;
      }
      delegate->SetScrolledDelta(
          controller.index,
          -axis.x, axis.y);
    } else if (controller.touched) {
      if (!(ctl_touch & WVR_InputId_Alias1_Thumbstick)) {
        controller.touched = false;
        delegate->EndTouch(controller.index);
      }
    }

    if (controller.is6DoF) {
      const int32_t kNumAxes = 4;
      float immersiveAxes[kNumAxes];
      immersiveAxes[device::kImmersiveAxisTouchpadX] = immersiveAxes[device::kImmersiveAxisTouchpadY] = 0.0f;
      immersiveAxes[device::kImmersiveAxisThumbstickX] = axisX;
      immersiveAxes[device::kImmersiveAxisThumbstickY] = axisY;
      delegate->SetAxes(controller.index, immersiveAxes, kNumAxes);
    } else {
      const int32_t kNumAxes = 2;
      float immersiveAxes[kNumAxes] = {0.0f, 0.0f};
      immersiveAxes[device::kImmersiveAxisTouchpadX] = axisX;
      immersiveAxes[device::kImmersiveAxisTouchpadY] = axisY;
      delegate->SetAxes(controller.index, immersiveAxes, kNumAxes);
    }

    updateHaptics(controller);
  }

  void ControllerManager::updateController(Controller &controller,
                                           const WVR_DevicePosePair_t &devicePair,
                                           const vrb::Matrix &hmd) {
    if (!controller.enabled) {
      return;
    }
    //float level = WVR_GetDeviceBatteryPercentage(controller.type);
    //delegate->SetBatteryLevel(controller.index, (int) (level * 100.0f));
    const WVR_PoseState_t &pose = devicePair.pose;
    if (!pose.isValidPose) {
      return;
    }
    controller.transform = vrb::Matrix::FromRowMajor(pose.poseMatrix.m);
    if (elbow && !pose.is6DoFPose) {
      controller.transform = elbow->GetTransform(controller.hand, hmd, controller.transform);
    } else if (renderMode == device::RenderMode::StandAlone) {
      controller.transform.TranslateInPlace(kAverageHeight);

      static vrb::Matrix transform(vrb::Matrix::Identity());
      transform = vrb::Matrix::Rotation(vrb::Vector(1.0f, 0.0f, 0.0f), 0.00f);
      const vrb::Matrix trans = vrb::Matrix::Position(vrb::Vector(0.0f, -0.06f, 0.07f));
      transform = transform.PostMultiply(trans);
      controller.transform = controller.transform.PostMultiply(transform);
    }


    if (renderMode == device::RenderMode::Immersive && pose.is6DoFPose) {
      static vrb::Matrix transform(vrb::Matrix::Identity());
      if (transform.IsIdentity()) {
        transform = vrb::Matrix::Rotation(vrb::Vector(1.0f, 0.0f, 0.0f), 0.70f);
        const vrb::Matrix trans = vrb::Matrix::Position(vrb::Vector(0.0f, 0.0f, -0.01f));
        transform = transform.PostMultiply(trans);
      }
      controller.transform = controller.transform.PostMultiply(transform);
    }
    delegate->SetTransform(controller.index, controller.transform);
  }

  void ControllerManager::updateHaptics(Controller &controller) {
    vrb::RenderContextPtr renderContext = context.lock();
    if (!renderContext) {
      return;
    }
    if (!delegate) {
      return;
    }

    uint64_t inputFrameID = 0;
    float pulseDuration = 0.0f, pulseIntensity = 0.0f;
    delegate->GetHapticFeedback(controller.index, inputFrameID, pulseDuration, pulseIntensity);
    if (inputFrameID > 0 && pulseIntensity > 0.0f && pulseDuration > 0) {
      if (controller.inputFrameID != inputFrameID) {
        // When there is a new input frame id from haptic vibration,
        // that means we start a new session for a vibration.
        controller.inputFrameID = inputFrameID;
        controller.remainingVibrateTime = pulseDuration;
        controller.lastHapticUpdateTimeStamp = renderContext->GetTimestamp();
      } else {
        // We are still running the previous vibration.
        // So, it needs to reduce the delta time from the last vibration.
        const double timeStamp = renderContext->GetTimestamp();
        controller.remainingVibrateTime -= (timeStamp - controller.lastHapticUpdateTimeStamp);
        controller.lastHapticUpdateTimeStamp = timeStamp;
      }

      if (controller.remainingVibrateTime > 0.0f && renderMode == device::RenderMode::Immersive) {
        // THe duration time unit needs to be transformed from milliseconds to microseconds.
        // The gamepad extensions API does not yet have independent control
        // of frequency and intensity. It only has vibration value (0.0 ~ 1.0).
        // In this WaveVR SDK, the value makes more sense to be intensity because frequency can't
        // < 1.0 here.
        int intensity = ceil(pulseIntensity * 5);
        intensity = intensity <= 5 ? intensity : 5;
        WVR_TriggerVibration(controller.type, WVR_InputId_Max,
                             controller.remainingVibrateTime * 1000.0f,
                             1, WVR_Intensity(intensity));
      } else {
        // The remaining time is zero or exiting the immersive mode, stop the vibration.
#if !defined(__arm__) // It will crash at WaveVR SDK arm32, let's skip it.
        WVR_TriggerVibration(controller.type, WVR_InputId_Max, 0, 0, WVR_Intensity_Normal);
#endif
        controller.remainingVibrateTime = 0.0f;
      }
    } else if (controller.remainingVibrateTime > 0.0f) {
      // While the haptic feedback is terminated from the client side,
      // but it still have remaining time, we need to ask for stopping vibration.
#if !defined(__arm__) // It will crash at WaveVR SDK arm32, let's skip it.
      WVR_TriggerVibration(controller.type, WVR_InputId_Max, 0, 0, WVR_Intensity_Normal);
#endif
      controller.remainingVibrateTime = 0.0f;
    }
  }

  void ControllerManager::setBattery(int deviceId, float level){

    delegate->SetBatteryLevel(deviceId, (int) (level * 100.0f));
    //VRB_LOG("ControllerManager::setBattery: device: %d percent: %f", deviceId, level)
  }

  bool ControllerManager::isRecentered() {
    bool result = recentered;
    recentered = false;
    return result;
  }

  void ControllerManager::setRecentered(bool value) {
    recentered = value;
  }

  void ControllerManager::setRenderMode(const device::RenderMode mode) {
    renderMode = mode;
  }

  vrb::LoadTask ControllerManager::getControllerModelTask(ControllerMetaInfo controllerMetaInfo) {
    return [this, controllerMetaInfo](vrb::CreationContextPtr &aContext) -> vrb::GroupPtr {
      // Load controller model from SDK
      WVR_DeviceType deviceType = controllerMetaInfo.type;
      const int index = static_cast<int>(controllerMetaInfo.hand);
      VRB_LOG("[WaveVR] (%p) Loading internal controller model: %d", this, index);

      {//Critical Section: Clear flag and cached parsed data.
        std::lock_guard<std::mutex> lockGuard(mCachedDataMutex[index]);
        if (modelCachedData[index] != nullptr) {
          WVR_ReleaseControllerModel(
              &modelCachedData[index]); //we will clear cached data ptr to nullptr.
        }
        isModelDataReady[index] = false;
      }//Critical Section: Clear flag and cached parsed data.(End)
      //2. Load ctrler model data.
      WVR_Result result = WVR_GetCurrentControllerModel(deviceType,
                                                        &modelCachedData[index]);
      if (result == WVR_Success) {
        {//Critical Section: Set data ready flag.
          std::lock_guard<std::mutex> lockGuard(mCachedDataMutex[index]);
          VRB_LOG("[WaveVR] (%d[%p]) Controller model from the SDK successfully loaded",
                  deviceType, this)
          isModelDataReady[index] = true;
        }//Critical Section: Set data ready flag.(End)
      } else {
        //MOHUS for Wave fix
//        const bool is6DoF = WVR_GetDegreeOfFreedom(controllerMetaInfo.type) == WVR_NumDoF_6DoF;
//        VRB_LOG("[WaveVR] (%d) Controller is6DoF: %d", controllerMetaInfo.type, is6DoF)
//        if(!is6DoF){
//          deviceType = WVR_DeviceType_Controller_Right;//WVR_DeviceType_HMD;
//        }
//        result = WVR_GetCurrentControllerModel(deviceType, &modelCachedData[index]);
//        if (result == WVR_Success) {
//          {//Critical Section: Set data ready flag.
//            std::lock_guard<std::mutex> lockGuard(mCachedDataMutex[index]);
//            VRB_LOG(
//                "[WaveVR] (%d[%p]) Controller model from the SDK successfully loaded by second step",
//                deviceType, this)
//            isModelDataReady[index] = true;
//          }//Critical Section: Set data ready flag.(End)
//        } else {
          VRB_LOG("[WaveVR] (%d[%p]): Load fail. Reason(%d)", deviceType, this, result);
//        }
      }

      if (!isModelDataReady[index]) {
        return nullptr;
      }

      timespec start;
      clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);

      WVR_CtrlerModel_t &modelData = (*modelCachedData[index]);

      VRB_LOG("[WaveVR] modelData.name: %s, %s", modelData.name,
              modelData.compInfos.table[0].name);

      // Initialize textures
      WVR_CtrlerTexBitmapTable_t &comp_Textures = modelData.bitmapInfos;
      uint32_t wvrBitmapSize = comp_Textures.size;
      std::vector<vrb::TextureGLPtr> mTextureTable(wvrBitmapSize);
      VRB_LOG("[WaveVR] (%d[%p]): Initialize WVRTextures(%d)", deviceType, this,
              wvrBitmapSize);
      for (uint32_t texID = 0; texID < wvrBitmapSize; ++texID) {
        vrb::TextureGLPtr texture = vrb::TextureGL::Create(aContext);
        size_t texture_size =
            comp_Textures.table[texID].stride * comp_Textures.table[texID].height;
        std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(texture_size);
        memcpy(data.get(), (void *) comp_Textures.table[texID].bitmap, texture_size);
        texture->SetImageData(
            data,
            texture_size,
            comp_Textures.table[texID].width,
            comp_Textures.table[texID].height,
            GL_RGBA
        );
        mTextureTable[texID] = texture;
      }

      VRB_LOG("[WaveVR] (%d[%p]): Initialize meshes(%d)", deviceType, this,
              modelData.compInfos.size)

      vrb::GroupPtr root = vrb::Group::Create(aContext);

      // Get only the body, we are not using other components right now.
      for (uint32_t wvrCompID = 0;
           wvrCompID < 1 /*(*modelCachedData).compInfos.size*/; ++wvrCompID) {
        char *name = modelData.compInfos.table[wvrCompID].name;
        vrb::VertexArrayPtr array = vrb::VertexArray::Create(aContext);

        // Vertices

        WVR_VertexBuffer_t &comp_Vertices = modelData.compInfos.table[wvrCompID].vertices;
        if (comp_Vertices.buffer == nullptr || comp_Vertices.size == 0 ||
            comp_Vertices.dimension == 0) {
          VRB_LOG("Parameter invalid!!! iData(%p), iSize(%u), iType(%u)", comp_Vertices.buffer,
                  comp_Vertices.size, comp_Vertices.dimension);
          return nullptr;
        }

        uint32_t vertices_dim = comp_Vertices.dimension;
        if (vertices_dim == 3) {
          for (auto i = 0; i < comp_Vertices.size; i += vertices_dim) {
            auto vertex = vrb::Vector(comp_Vertices.buffer[i], comp_Vertices.buffer[i + 1],
                                      comp_Vertices.buffer[i + 2]);
            array->AppendVertex(vertex);
          }
        } else {
          VRB_ERROR("[WaveVR] (%d[%p]): vertex with wrong dimension: %d", deviceType, this,
                    vertices_dim)
        }

        // Normals

        WVR_VertexBuffer_t &comp_Normals = modelData.compInfos.table[wvrCompID].normals;
        if (comp_Normals.buffer == nullptr || comp_Normals.size == 0 ||
            comp_Normals.dimension == 0) {
          VRB_LOG("Parameter invalid!!! iData(%p), iSize(%u), iType(%u)", comp_Normals.buffer,
                  comp_Normals.size, comp_Normals.dimension);
          return nullptr;
        }

        uint32_t normals_dim = comp_Normals.dimension;
        if (normals_dim == 3) {
          for (auto i = 0; i < comp_Normals.size; i += normals_dim) {
            auto normal = vrb::Vector(comp_Normals.buffer[i], comp_Normals.buffer[i + 1],
                                      comp_Normals.buffer[i + 2]).Normalize();
            array->AppendNormal(normal);
          }
        } else {
          VRB_ERROR("[WaveVR] (%d[%p]): normal with wrong dimension: %d", deviceType, this,
                    normals_dim)
        }

        WVR_VertexBuffer_t &comp_TextCoord = modelData.compInfos.table[wvrCompID].texCoords;
        if (comp_TextCoord.buffer == nullptr || comp_TextCoord.size == 0 ||
            comp_TextCoord.dimension == 0) {
          VRB_LOG("Parameter invalid!!! iData(%p), iSize(%u), iType(%u)", comp_TextCoord.buffer,
                  comp_TextCoord.size, comp_TextCoord.dimension);
          return nullptr;
        }

        // UVs

        uint32_t texCoords_dim = comp_TextCoord.dimension;
        if (texCoords_dim == 2) {
          for (auto i = 0; i < comp_TextCoord.size; i += texCoords_dim) {
            auto textCoord = vrb::Vector(comp_TextCoord.buffer[i], comp_TextCoord.buffer[i + 1],
                                         0);
            array->AppendUV(textCoord);
          }
        } else {
          VRB_ERROR("[WaveVR] (%d[%p]): normal with wrong dimension: %d", deviceType, this,
                    texCoords_dim)
        }

        vrb::ProgramPtr program = aContext->GetProgramFactory()->CreateProgram(aContext,
                                                                               vrb::FeatureTexture);
        vrb::RenderStatePtr state = vrb::RenderState::Create(aContext);
        state->SetProgram(program);
        state->SetMaterial(
            vrb::Color(1.0f, 1.0f, 1.0f),
            vrb::Color(1.0f, 1.0f, 1.0f),
            vrb::Color(0.0f, 0.0f, 0.0f),
            0.0f);
        state->SetLightsEnabled(false);
        state->SetTexture(
            mTextureTable[modelData.compInfos.table[wvrCompID].texIndex]);
        vrb::GeometryPtr geometry = vrb::Geometry::Create(aContext);
        geometry->SetName(name);
        geometry->SetVertexArray(array);
        geometry->SetRenderState(state);

        // Indices

        WVR_IndexBuffer_t &comp_Indices = modelData.compInfos.table[wvrCompID].indices;
        if (comp_Indices.buffer == nullptr || comp_Indices.size == 0 || comp_Indices.type == 0) {
          VRB_ERROR("Parameter invalid!!! iData(%p), iSize(%u), iType(%u)", comp_Indices.buffer,
                    comp_Indices.size, comp_Indices.type);
          return nullptr;
        }

        uint32_t type = comp_Indices.type;
        for (auto i = 0; i < comp_Indices.size; i += type) {
          std::vector<int> indices;
          for (auto j = 0; j < type; j++) {
            indices.push_back(comp_Indices.buffer[i + j] + 1);
          }
          geometry->AddFace(indices, indices, indices);
        }

        root->AddNode(geometry);
      }

      timespec end;
      clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
      float time = (float) end.tv_sec + (((float) end.tv_nsec) / 1.0e9f) -
                   ((float) start.tv_sec + (((float) start.tv_nsec) / 1.0e9f));

      VRB_LOG("[WaveVR] (%d[%p]): Controller loaded in: %f", deviceType, this, time)

      return root;
    };
  }
}