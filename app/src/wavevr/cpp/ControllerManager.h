#pragma once

#include "ControllerDelegate.h"
#include "Controller.h"
#include "vrb/LoaderThread.h"
#include <wvr/wvr_ctrller_render_model.h>
#include <mutex>

namespace crow {
  class ControllerManager {
  public:
    ControllerManager(crow::ControllerDelegatePtr &controllerDelegatePtr,
                      vrb::RenderContextWeak contextPtr);

    ~ControllerManager();

  public:
    void onCreate();

    void onDestroy();

    void updateControllerState(crow::Controller &controller);

    void updateController(Controller &controller, const WVR_DevicePosePair_t &devicePair,
                          const vrb::Matrix &hmd);

    void setRenderMode(const device::RenderMode mode);

    bool isRecentered();

    void setRecentered(bool value);

    vrb::LoadTask getControllerModelTask(ControllerMetaInfo controllerMetaInfo);

    void setBattery(int deviceId, float level);

  private:
    void updateHaptics(Controller &controller);

  private:
    vrb::RenderContextWeak context;
    crow::ControllerDelegatePtr &delegate;
    device::RenderMode renderMode;
    bool recentered;

    ElbowModelPtr elbow;

    WVR_CtrlerModel_t *modelCachedData[kMaxControllerCount];
    bool isModelDataReady[kMaxControllerCount];
    std::mutex mCachedDataMutex[kMaxControllerCount];

  };
}