#ifndef DEVICE_DELEGATE_WAVE_VR_DOT_H
#define DEVICE_DELEGATE_WAVE_VR_DOT_H

#include "vrb/Forward.h"
#include "vrb/MacroUtils.h"
#include "vrb/Vector.h"
#include "vrb/Matrix.h"
#include "DeviceDelegate.h"
#include "shared/Matrices.h"
#include "ElbowModel.h"
#include "Controller.h"
#include <wvr/wvr_hand.h>
#include <memory>

const uint32_t kHandCount = 2;
const uint32_t kJointCount = 26;

namespace crow {

class DeviceDelegateWaveVR;
typedef std::shared_ptr<DeviceDelegateWaveVR> DeviceDelegateWaveVRPtr;

class DeviceDelegateWaveVR : public DeviceDelegate {
public:
  static DeviceDelegateWaveVRPtr Create(vrb::RenderContextPtr& aContext);
  void InitializeRender();
  // DeviceDelegate interface
  device::DeviceType GetDeviceType() override;
  void SetRenderMode(const device::RenderMode aMode) override;
  device::RenderMode GetRenderMode() override;
  void RegisterImmersiveDisplay(ImmersiveDisplayPtr aDisplay) override;
  void SetImmersiveSize(const uint32_t aEyeWidth, const uint32_t aEyeHeight) override;
  GestureDelegateConstPtr GetGestureDelegate() override;
  vrb::CameraPtr GetCamera(const device::Eye aWhich) override;
  const vrb::Matrix& GetHeadTransform() const override;
  const vrb::Matrix& GetReorientTransform() const override;
  void SetReorientTransform(const vrb::Matrix& aMatrix) override;
  void Reorient() override;
  void SetClearColor(const vrb::Color& aColor) override;
  void SetClipPlanes(const float aNear, const float aFar) override;
  void SetControllerDelegate(ControllerDelegatePtr& aController) override;
  void ReleaseControllerDelegate() override;
  int32_t GetControllerModelCount() const override;
  void ProcessEvents() override;
  void StartFrame(const FramePrediction aPrediction) override;
  void BindEye(const device::Eye aWhich) override;
  void EndFrame(const FrameEndMode aMode) override;
  vrb::LoadTask GetControllerModelTask(int32_t index) override;
  // DeviceDelegateWaveVR interface
  bool IsRunning();
protected:
  struct State;
  DeviceDelegateWaveVR(State& aState);
  virtual ~DeviceDelegateWaveVR();
private:
  State& m;
  VRB_NO_DEFAULTS(DeviceDelegateWaveVR)
private:
  WVR_DevicePosePair_t *findDevicePair(WVR_DeviceType type);
  void UpdateHand(Controller &controller, const WVR_DevicePosePair_t &devicePair,
                    const vrb::Matrix &hmd);
  void UpdateController(Controller &controller, const WVR_DevicePosePair_t &devicePair,
                          const vrb::Matrix &hmd);
};

} // namespace crow
#endif // DEVICE_DELEGATE_WAVE_VR_DOT_H
