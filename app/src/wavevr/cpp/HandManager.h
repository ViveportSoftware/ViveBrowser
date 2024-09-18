#pragma once

#include <mutex>
#include <thread>
#include <atomic>
#include <string>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <wvr/wvr_types.h>
#include <wvr/wvr_hand.h>
#include <wvr/wvr_device.h>

#include "DrawEnum.h"

#include "shared/Matrices.h"
#include "object/Mesh.h"
#include "object/Texture.h"
#include "object/Shader.h"

#include "HandConstant.h"
#include "HandObj.h"
#include "vrb/CameraEye.h"
#include "ElbowModel.h"
#include "ControllerDelegate.h"
#include "Controller.h"
#include "vrb/LoaderThread.h"

#include <wvr/wvr_hand_render_model.h>
#include <mutex>

namespace crow {

  class HandManager {
  public:
    HandManager(WVR_HandTrackerType iTrackingType, crow::ControllerDelegatePtr &controllerDelegatePtr);

    ~HandManager();

  public:
    void onCreate();

    void onDestroy();

    void setRenderMode(const device::RenderMode mode);

    void updateHand(Controller &controller, const WVR_DevicePosePair_t &devicePair,
                    const vrb::Matrix &hmd);

    void updateHandState(Controller &controller);

  public:
    void update();

  protected:
    void startHandTracking();

    void stopHandTracking();

  public:
    void calculateHandMatrices();

    bool isHandAvailable(const crow::ElbowModel::HandEnum hand);

    vrb::LoadTask getHandModelTask(ControllerMetaInfo controllerMetaInfo);

  private:
    float getPinchStrength(const crow::ElbowModel::HandEnum hand);

    vrb::Matrix getRayMatrix(const crow::ElbowModel::HandEnum hand);

    Vector4 calculateJointWorldPosition(const int handIndex, uint32_t jID) const;
    Vector3 getModelJointLocalPosition(const int handIndex, uint32_t iJointID) const;
  private:
    crow::ControllerDelegatePtr &delegate;
    device::RenderMode renderMode;

    WVR_HandRenderModel_t *modelCachedData = nullptr;
    bool isModelDataReady[kMaxControllerCount];
    std::mutex mCachedDataMutex[kMaxControllerCount];

  protected:
    WVR_HandTrackerType mTrackingType;
    WVR_HandTrackerInfo_t mHandTrackerInfo;
    WVR_HandTrackingData_t mHandTrackingData;
    WVR_HandPoseData_t mHandPoseData;
    std::atomic<bool> mStartFlag;
  protected:
    bool mIsPrintedSkeErrLog[Hand_MaxNumber];
  protected:

    Matrix4 mSkeletonPoses[Hand_MaxNumber][sMaxSupportJointNumbers]; //tracking data in model space.
    Matrix4 mModelSkeletonPoses[Hand_MaxNumber][sMaxSupportJointNumbers]; //final skinned input.
    Matrix4 mFinalSkeletonPoses[Hand_MaxNumber][sMaxSupportJointNumbers]; //final joint mat in model space.
    float mSkeletonMatrices[Hand_MaxNumber][16 * sMaxSupportJointNumbers];

    Matrix4 mJointInvTransMats[Hand_MaxNumber][sMaxSupportJointNumbers];
    Matrix4 mJointTransMats[Hand_MaxNumber][sMaxSupportJointNumbers];
    Matrix4 mJointLocalTransMats[Hand_MaxNumber][sMaxSupportJointNumbers];
    uint32_t mJointParentTable[Hand_MaxNumber][sMaxSupportJointNumbers];
    int32_t mJointUsageTable[Hand_MaxNumber][sMaxSupportJointNumbers]; //0 means we don't use it.

    Matrix4 mJointMats[Hand_MaxNumber][sMaxSupportJointNumbers]; //Store mapping-convert poses.
    Matrix4 mHandPoseMats[Hand_MaxNumber];

    bool mIsHandPoseValids[Hand_MaxNumber];
  protected:
    Matrix4 mHandRayMats[Hand_MaxNumber];
  protected:
    vrb::TextureGLPtr mTexture;
    Matrix4 mShift;
    Matrix4 mProjectionMatrix;
    Matrix4 mEyeMatrix;
    Matrix4 mHeadMatrix;
  protected:
    std::thread mLoadModelThread;
    std::mutex mGraphicsChangedMutex;
    EGLContext mEGLInitContext;
    EGLDisplay mEGLInitDisplay;

  };
}