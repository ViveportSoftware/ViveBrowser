/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ControllerContainer.h"
#include "Controller.h"
#include "Pointer.h"
#include "Quad.h"
#include "DeviceUtils.h"

#include "vrb/ConcreteClass.h"
#include "vrb/Color.h"
#include "vrb/CreationContext.h"
#include "vrb/Geometry.h"
#include "vrb/Group.h"
#include "vrb/Matrix.h"
#include "vrb/ModelLoaderAndroid.h"
#include "vrb/Program.h"
#include "vrb/ProgramFactory.h"
#include "vrb/RenderState.h"
#include "vrb/TextureGL.h"
#include "vrb/Toggle.h"
#include "vrb/Transform.h"
#include "vrb/Vector.h"
#include "vrb/VertexArray.h"

#include <assert.h>

using namespace vrb;

namespace crow {

struct ControllerContainer::State {
  std::vector<Controller> list;
  CreationContextWeak context;
  TogglePtr root;
  GroupPtr pointerContainer;
  std::vector<GroupPtr> models;
  std::vector<GeometryPtr> beamModels;
  vrb::TextureGLPtr beamTextureNormal;
  vrb::TextureGLPtr beamTexturePress;
  bool visible = false;
  vrb::Color pointerColor;
  int gazeIndex = -1;
  int handIndex = 2;
  uint64_t immersiveFrameId;
  uint64_t lastImmersiveFrameId;
  ModelLoaderAndroidPtr loader;
  std::vector<vrb::LoadTask> loadTask;

  void Initialize(vrb::CreationContextPtr& aContext) {
    context = aContext;
    root = Toggle::Create(aContext);
    visible = true;
    pointerColor = vrb::Color(1.0f, 1.0f, 1.0f, 1.0f);
    immersiveFrameId = 0;
    lastImmersiveFrameId = 0;
  }

  bool Contains(const int32_t aControllerIndex) {
    return (aControllerIndex >= 0) && (aControllerIndex < list.size());
  }

  void SetUpModelsGroup(const int32_t aModelIndex) {
    if (aModelIndex >= models.size()) {
      models.resize((size_t)(aModelIndex + 1));
    }
    if (!models[aModelIndex]) {
      CreationContextPtr create = context.lock();
      models[aModelIndex] = std::move(Group::Create(create));
    }
  }

  void updatePointerColor(Controller& aController) {
    if (aController.beamParent && aController.beamParent->GetNodeCount() > 0) {
      GeometryPtr geometry = std::dynamic_pointer_cast<vrb::Geometry>(aController.beamParent->GetNode(0));
      if (geometry) {
        geometry->GetRenderState()->SetMaterial(pointerColor, pointerColor, vrb::Color(0.0f, 0.0f, 0.0f), 0.0f);
      }
    }
    if (aController.pointer) {
      aController.pointer->SetPointerColor(pointerColor);
    }
  }

  void
  SetVisible(Controller& controller, const bool aVisible) {
    if (controller.transform && visible) {
      root->ToggleChild(*controller.transform, aVisible);
    }
    if (controller.pointer && !aVisible) {
      controller.pointer->SetVisible(false);
    }
  }
};

ControllerContainerPtr
ControllerContainer::Create(vrb::CreationContextPtr& aContext, const vrb::GroupPtr& aPointerContainer, const ModelLoaderAndroidPtr& aLoader) {
  auto result = std::make_shared<vrb::ConcreteClass<ControllerContainer, ControllerContainer::State> >(aContext);
  result->m.pointerContainer = aPointerContainer;
  result->m.loader = aLoader;
  return result;
}

TogglePtr
ControllerContainer::GetRoot() const {
  return m.root;
}

void
ControllerContainer::LoadControllerModel(const int32_t aModelIndex, const ModelLoaderAndroidPtr& aLoader, const std::string& aFileName) {
  m.SetUpModelsGroup(aModelIndex);
  aLoader->LoadModel(aFileName, m.models[aModelIndex]);
}

void
ControllerContainer::LoadControllerModel(const int32_t aModelIndex) {
  m.SetUpModelsGroup(aModelIndex);
  if (m.loadTask[aModelIndex]) {
    m.loader->LoadModel(m.loadTask[aModelIndex], m.models[aModelIndex]);
  } else {
    VRB_ERROR("No model load task fork model: %d", aModelIndex)
  }
}

void ControllerContainer::SetControllerModelTask(const int32_t aModelIndex, const vrb::LoadTask& aTask) {
  m.SetUpModelsGroup(aModelIndex);
  m.loadTask.resize(aModelIndex + 1, aTask);
}

GeometryPtr InitializeBeamModel(CreationContextPtr create){
    VRB_LOG("[ControllerContainer] InitializeBeamModel");

    VertexArrayPtr array = VertexArray::Create(create);
    const float kLength = -1.0f;
    const float kHeight = 0.002f;

    const int sgm=20;
    //Vertex
    for (int i = 1; i <= sgm; ++i) {
        float  x=  cosf(((float )i/sgm)*2*M_PI)*kHeight;
        float  y=  sinf(((float )i/sgm)*2*M_PI)*kHeight;
        array->AppendVertex(Vector(x, y, 0.0f));

        //UV
        const float u = (float) i-1 / (float) sgm;
        const float  v = 1.0f;
        array->AppendUV(Vector(u, v, 0.0f));
    }
    array->AppendVertex(Vector(0.0f, 0.0f, kLength)); // Tip
    //UV
    array->AppendUV(Vector(0, 0, 0.0f));

    //Normal
    for (int i = 1; i <= sgm; ++i) {
        float  x=  cosf(((float )i/sgm)*2*M_PI);
        float  y=  sinf(((float )i/sgm)*2*M_PI);
        array->AppendNormal(Vector(x, y, 0.0f));
    }
    array->AppendNormal(Vector(0.0f, 0.0f, -1.0f).Normalize()); // in to the screen

    ProgramPtr program = create->GetProgramFactory()->CreateProgram(create, vrb::FeatureTexture);
    RenderStatePtr state = RenderState::Create(create);
    state->SetProgram(program);
    state->SetMaterial(Color(1.0f, 1.0f, 1.0f),
                       Color(1.0f, 1.0f, 1.0f),
                       Color(0.0f, 0.0f, 0.0f),
                       0.0f);
    state->SetLightsEnabled(false);
    //VRB_LOG("LoadTexture texture->GetHeight %d ", texture->GetHeight());
    GeometryPtr geometry = Geometry::Create(create);
    geometry->SetVertexArray(array);
    geometry->SetRenderState(state);

    std::vector<int> index;
    std::vector<int> uvIndex;

    for (int i = 1; i <= sgm; ++i) {
        index.clear();
        int p=i+1>sgm?i+1-sgm:i+1;
        index.push_back(p);
        index.push_back(i);
        index.push_back(sgm+1);
        geometry->AddFace(index, index, index);
    }
    index.clear();
    for (int i = 1; i <= sgm; ++i){
        index.push_back(i);
    }
    geometry->AddFace(index, index, index);
    return geometry;
}

void
ControllerContainer::InitializeBeam() {
  VRB_LOG("[ControllerContainer] InitializeBeam");

  if(m.beamModels.size() == beamSize){
      return;
  }

  m.beamModels.resize(beamSize);
  for (int i = 0; i < beamSize; ++i) {
      CreationContextPtr create = m.context.lock();
      m.beamTextureNormal = create->LoadTexture("beam_white.png") ;
      m.beamTexturePress  = create->LoadTexture("beam_blue.png") ;
      GeometryPtr gp= InitializeBeamModel(create);
      gp->GetRenderState()->SetTexture(m.beamTextureNormal);
      m.beamModels[i] = std::move(gp);
  }

  VRB_LOG("[ControllerContainer] InitializeBeam m.list.size(): %zu", m.list.size());
  for (int i = 0; i < m.list.size(); ++i) {
      Controller& controller = m.list[i];
      if (controller.beamParent && i <= beamSize) {
          GeometryPtr beamModel = m.beamModels[i];
          if(!beamModel)
              VRB_ERROR("[ControllerContainer] InitializeBeam beamModel is null");
          controller.beamParent->AddNode(beamModel);
      }
  }
}

void ControllerContainer::SetHandJointLocations(const int32_t aControllerIndex, std::vector<vrb::Matrix> jointTransforms,
                                                std::vector<float> jointRadii)
{
    if (!m.Contains(aControllerIndex))
        return;

    Controller &controller = m.list[aControllerIndex];
    if (!m.root)
        return;

    controller.handJointTransforms = std::move(jointTransforms);
    controller.handJointRadii = std::move(jointRadii);

    // Initialize left and right hands action button, which for now triggers back navigation
    // and exit app respectively.
    // Note that Quest's runtime already shows the hamburger menu button when left
    // hand is facing head and the system menu for the right hand gesture.
#if !defined(OCULUSVR)
    if (controller.handActionButtonToggle == nullptr) {
        CreationContextPtr create = m.context.lock();

        TextureGLPtr texture = create->LoadTexture(controller.leftHanded ? "menu.png" : "exit.png") ;
        assert(texture);
        if (texture == nullptr) {
            VRB_ERROR("Null pointer, file: %s, function: %s, line: %d",__FILE__, __FUNCTION__, __LINE__);
            return;
        }
        texture->SetTextureParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        texture->SetTextureParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        const float iconWidth = 0.03f;
        const float aspect = (float)texture->GetWidth() / (float)texture->GetHeight();

        QuadPtr icon = Quad::Create(create, iconWidth, iconWidth / aspect, nullptr);
        if (icon == nullptr) {
            VRB_ERROR("Null pointer, file: %s, function: %s, line: %d",__FILE__, __FUNCTION__, __LINE__);
            return;
        }
        icon->SetTexture(texture, texture->GetWidth(), texture->GetHeight());
        icon->UpdateProgram("");

        controller.handActionButtonToggle = Toggle::Create(create);
        controller.handActionButtonTransform = Transform::Create(create);
        controller.handActionButtonToggle->AddNode(controller.handActionButtonTransform);
        controller.handActionButtonTransform->AddNode(icon->GetRoot());
        controller.handActionButtonToggle->ToggleAll(false);
        m.root->AddNode(controller.handActionButtonToggle);
    }
#endif
}

void ControllerContainer::SetMode(const int32_t aControllerIndex, ControllerMode aMode)
{
  if (!m.Contains(aControllerIndex))
    return;
  m.list[aControllerIndex].mode = aMode;
}

void ControllerContainer::SetSelectFactor(const int32_t aControllerIndex, float aFactor)
{
  if (!m.Contains(aControllerIndex))
    return;
  m.list[aControllerIndex].selectFactor = aFactor;
}

void ControllerContainer::SetAimEnabled(const int32_t aControllerIndex, bool aEnabled) {
    if (!m.Contains(aControllerIndex))
        return;
    m.list[aControllerIndex].hasAim = aEnabled;
}

void ControllerContainer::SetHandActionEnabled(const int32_t aControllerIndex, bool aEnabled) {
    if (!m.Contains(aControllerIndex))
        return;
    m.list[aControllerIndex].handActionEnabled = aEnabled;
    if (m.list[aControllerIndex].handActionButtonToggle)
        m.list[aControllerIndex].handActionButtonToggle->ToggleAll(aEnabled);
}

void
ControllerContainer::Reset() {
  for (Controller& controller: m.list) {
    controller.DetachRoot();
    controller.Reset();
  }
}

std::vector<Controller>&
ControllerContainer::GetControllers() {
  return m.list;
}

const std::vector<Controller>&
ControllerContainer::GetControllers() const {
  return m.list;
}

// crow::ControllerDelegate interface
uint32_t
ControllerContainer::GetControllerCount() {
  return (uint32_t)m.list.size();
}

void
ControllerContainer::CreateController(const int32_t aControllerIndex, const int32_t aModelIndex, const std::string& aImmersiveName) {
  CreateController(aControllerIndex, aModelIndex, aImmersiveName, vrb::Matrix::Identity());
}

void
ControllerContainer::CreateController(const int32_t aControllerIndex, const int32_t aModelIndex, const std::string& aImmersiveName, const vrb::Matrix& aBeamTransform) {
  VRB_LOG("[ControllerContainer] CreateController aControllerIndex: %d, aModelIndex: %d, aImmersiveName: %s", aControllerIndex, aModelIndex, aImmersiveName.c_str());
  if ((size_t)aControllerIndex >= m.list.size()) {
    m.list.resize((size_t)aControllerIndex + 1);
  }
  Controller& controller = m.list[aControllerIndex];
  controller.DetachRoot();
  controller.Reset();
  controller.index = aControllerIndex;
  controller.immersiveName = aImmersiveName;
  controller.beamTransformMatrix = aBeamTransform;
  controller.immersiveBeamTransform = aBeamTransform;
  if (aModelIndex < 0) {
    return;
  }
  m.SetUpModelsGroup(aModelIndex);
  CreationContextPtr create = m.context.lock();
  controller.transform = Transform::Create(create);
  controller.pointer = Pointer::Create(create);
  controller.pointer->SetVisible(true);

  if (aControllerIndex != m.gazeIndex) {
    if ((m.models.size() >= aModelIndex) && m.models[aModelIndex]) {


      if(aControllerIndex >= m.handIndex) {//is hand
        controller.modelParent = Transform::Create(create);
        controller.modelParent->SetTransform(aBeamTransform);
        controller.modelParent->AddNode(m.models[aModelIndex]);

        controller.modelToggle = vrb::Toggle::Create(create);
        controller.modelToggle->AddNode(controller.modelParent);
        controller.modelToggle->ToggleAll(true);

        controller.beamParent = Transform::Create(create);
        controller.beamParent->SetTransform(aBeamTransform);

        controller.beamToggle = vrb::Toggle::Create(create);
        controller.beamToggle->AddNode(controller.beamParent);
        controller.beamToggle->ToggleAll(true);

        controller.transform->AddNode(controller.beamToggle);
        controller.transform->AddNode(controller.modelToggle);
      } else {
        controller.modelToggle = vrb::Toggle::Create(create);
        controller.modelToggle->AddNode(m.models[aModelIndex]);
        controller.modelToggle->ToggleAll(true);

        controller.beamParent = Transform::Create(create);
        controller.beamParent->SetTransform(aBeamTransform);

        controller.beamToggle = vrb::Toggle::Create(create);
        controller.beamToggle->AddNode(controller.beamParent);
        controller.beamToggle->ToggleAll(true);

        controller.transform->AddNode(controller.beamToggle);
        controller.transform->AddNode(controller.modelToggle);
      }


      if(m.beamModels.size()==beamSize && aControllerIndex <= beamSize && controller.beamParent){
          GeometryPtr beamModel = m.beamModels[aControllerIndex];
          controller.beamParent->AddNode(beamModel);
      }

      // If the model is not yet loaded we trigger the load task
      if (m.models[aModelIndex]->GetNodeCount() == 0  &&
          m.loadTask.size() > aControllerIndex + 1 && m.loadTask[aModelIndex]) {
        m.loader->LoadModel(m.loadTask[aModelIndex], m.models[aModelIndex]);
      }
    } else {
      VRB_ERROR("Failed to add controller model")
    }
  }

  if (m.root) {
    m.root->AddNode(controller.transform);
    m.root->ToggleChild(*controller.transform, false);
  }
  if (m.pointerContainer) {
    m.pointerContainer->AddNode(controller.pointer->GetRoot());
  }
  m.updatePointerColor(controller);
}

void
ControllerContainer::SetImmersiveBeamTransform(const int32_t aControllerIndex,
        const vrb::Matrix& aImmersiveBeamTransform) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }
  m.list[aControllerIndex].immersiveBeamTransform = aImmersiveBeamTransform;
}

void
ControllerContainer::SetBeamTransform(const int32_t aControllerIndex,
                                      const vrb::Matrix &aBeamTransform) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }
  Controller &controller = m.list[aControllerIndex];
  vrb::Matrix new_transform = aBeamTransform;
  if (aControllerIndex >= m.handIndex) {
    //lerp Translation
    Vector oldTranslation = controller.beamTransformMatrix.GetTranslation();
    Vector newTranslation = aBeamTransform.GetTranslation();
    float lerpRatio = 0.5f;
    Vector lerpedTranslation = oldTranslation * lerpRatio + newTranslation * (1.0f - lerpRatio);
    new_transform = aBeamTransform.Translate(lerpedTranslation - aBeamTransform.GetTranslation());
  }

  if (controller.beamParent) {
    controller.beamParent->SetTransform(new_transform);
  }
  controller.beamTransformMatrix = new_transform;
}

void
ControllerContainer::SetBeamColor(const int32_t aControllerIndex, const BeamColor beamColor) {
    VRB_LOG("[ControllerContainer] SetBeamColor aControllerIndex: %d, beamColor: %d", aControllerIndex, beamColor);
    if (!m.Contains(aControllerIndex)) {
        return;
    }
    if(aControllerIndex>=m.beamModels.size()){
        return;
    }

    if(beamColor== BeamColor::Normal){
        m.beamModels[aControllerIndex]->GetRenderState()->SetTexture(m.beamTextureNormal);

    } else if (beamColor== BeamColor::Press){
        m.beamModels[aControllerIndex]->GetRenderState()->SetTexture(m.beamTexturePress);
    }
}

void
ControllerContainer::SetFocused(const int32_t aControllerIndex) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }
  for (Controller& controller: m.list) {
    controller.focused = controller.index == aControllerIndex;
  }
}

void
ControllerContainer::DestroyController(const int32_t aControllerIndex) {
  if (m.Contains(aControllerIndex)) {
    m.list[aControllerIndex].DetachRoot();
    m.list[aControllerIndex].Reset();
  }
}

void
ControllerContainer::SetCapabilityFlags(const int32_t aControllerIndex, const device::CapabilityFlags aFlags) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }

  m.list[aControllerIndex].deviceCapabilities = aFlags;
}

void
ControllerContainer::SetEnabled(const int32_t aControllerIndex, const bool aEnabled) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }
  m.list[aControllerIndex].enabled = aEnabled;
  if (!aEnabled) {
    m.list[aControllerIndex].focused = false;
    SetMode(aControllerIndex, ControllerMode::None);
  }
  m.SetVisible(m.list[aControllerIndex], aEnabled);
}

void
ControllerContainer::SetModelVisible(const int32_t aControllerIndex, const bool aVisible) {
    if (!m.Contains(aControllerIndex))
        return;

    if (m.list[aControllerIndex].modelToggle)
        m.list[aControllerIndex].modelToggle->ToggleAll(aVisible);
}

void
ControllerContainer::SetControllerType(const int32_t aControllerIndex, device::DeviceType aType) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }
  Controller& controller = m.list[aControllerIndex];
  controller.type = aType;
}

void
ControllerContainer::SetTargetRayMode(const int32_t aControllerIndex, device::TargetRayMode aMode) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }
  Controller& controller = m.list[aControllerIndex];
  controller.targetRayMode = aMode;
}

void
ControllerContainer::SetTransform(const int32_t aControllerIndex, const vrb::Matrix& aTransform) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }
  Controller& controller = m.list[aControllerIndex];
  if(aControllerIndex>=m.handIndex){//is hand
    controller.transformMatrix = Matrix::Identity();
    if(controller.modelParent){
      controller.modelParent->SetTransform(aTransform);
    }
  } else{
    controller.transformMatrix = aTransform;
    if (controller.transform) {
      controller.transform->SetTransform(aTransform);
    }
  }
}

void
ControllerContainer::SetButtonCount(const int32_t aControllerIndex, const uint32_t aNumButtons) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }
  m.list[aControllerIndex].numButtons = aNumButtons;
}

void
ControllerContainer::SetButtonState(const int32_t aControllerIndex, const Button aWhichButton, const int32_t aImmersiveIndex, const bool aPressed, const bool aTouched, const float aImmersiveTrigger) {
  assert(kControllerMaxButtonCount > aImmersiveIndex
         && "Button index must < kControllerMaxButtonCount.");

  if (!m.Contains(aControllerIndex)) {
    return;
  }

  const int32_t immersiveButtonMask = 1 << aImmersiveIndex;

  if (aPressed) {
    m.list[aControllerIndex].buttonState |= aWhichButton;
  } else {
    m.list[aControllerIndex].buttonState &= ~aWhichButton;
  }

  if (aImmersiveIndex >= 0) {
    if (aPressed) {
      m.list[aControllerIndex].immersivePressedState |= immersiveButtonMask;
    } else {
      m.list[aControllerIndex].immersivePressedState &= ~immersiveButtonMask;
    }

    if (aTouched) {
      m.list[aControllerIndex].immersiveTouchedState |= immersiveButtonMask;
    } else {
      m.list[aControllerIndex].immersiveTouchedState &= ~immersiveButtonMask;
    }

    float trigger = aImmersiveTrigger;
    if (trigger < 0.0f) {
      trigger = aPressed ? 1.0f : 0.0f;
    }
    m.list[aControllerIndex].immersiveTriggerValues[aImmersiveIndex] = trigger;
  }
}

void
ControllerContainer::SetAxes(const int32_t aControllerIndex, const float* aData, const uint32_t aLength) {
  assert(kControllerMaxAxes >= aLength
         && "Axis length must <= kControllerMaxAxes.");

  if (!m.Contains(aControllerIndex)) {
    return;
  }

  m.list[aControllerIndex].numAxes = aLength;
  for (int i = 0; i < aLength; ++i) {
    m.list[aControllerIndex].immersiveAxes[i] = aData[i];
  }
}

void
ControllerContainer::SetHapticCount(const int32_t aControllerIndex, const uint32_t aNumHaptics) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }
  m.list[aControllerIndex].numHaptics = aNumHaptics;
}

uint32_t
ControllerContainer::GetHapticCount(const int32_t aControllerIndex) {
  if (!m.Contains(aControllerIndex)) {
    return 0;
  }

  return m.list[aControllerIndex].numHaptics;
}

void
ControllerContainer::SetHapticFeedback(const int32_t aControllerIndex, const uint64_t aInputFrameID,
                                        const float aPulseDuration, const float aPulseIntensity) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }
  m.list[aControllerIndex].inputFrameID = aInputFrameID;
  m.list[aControllerIndex].pulseDuration = aPulseDuration;
  m.list[aControllerIndex].pulseIntensity = aPulseIntensity;
}

void
ControllerContainer::GetHapticFeedback(const int32_t aControllerIndex, uint64_t & aInputFrameID,
                                        float& aPulseDuration, float& aPulseIntensity) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }
  aInputFrameID = m.list[aControllerIndex].inputFrameID;
  aPulseDuration = m.list[aControllerIndex].pulseDuration;
  aPulseIntensity = m.list[aControllerIndex].pulseIntensity;
}

void
ControllerContainer::SetSelectActionStart(const int32_t aControllerIndex) {
  if (!m.Contains(aControllerIndex) || !m.immersiveFrameId) {
    return;
  }

  if (m.list[aControllerIndex].selectActionStopFrameId >=
      m.list[aControllerIndex].selectActionStartFrameId) {
    m.list[aControllerIndex].selectActionStartFrameId = m.immersiveFrameId;
  }
}

void
ControllerContainer::SetSelectActionStop(const int32_t aControllerIndex) {
  if (!m.Contains(aControllerIndex) || !m.lastImmersiveFrameId) {
    return;
  }

  if (m.list[aControllerIndex].selectActionStartFrameId >
      m.list[aControllerIndex].selectActionStopFrameId) {
    m.list[aControllerIndex].selectActionStopFrameId = m.lastImmersiveFrameId;
  }
}

void
ControllerContainer::SetSqueezeActionStart(const int32_t aControllerIndex) {
  if (!m.Contains(aControllerIndex) || !m.immersiveFrameId) {
    return;
  }

  if (m.list[aControllerIndex].squeezeActionStopFrameId >=
      m.list[aControllerIndex].squeezeActionStartFrameId) {
    m.list[aControllerIndex].squeezeActionStartFrameId = m.immersiveFrameId;
  }
}

void
ControllerContainer::SetSqueezeActionStop(const int32_t aControllerIndex) {
  if (!m.Contains(aControllerIndex) || !m.lastImmersiveFrameId) {
    return;
  }

  if (m.list[aControllerIndex].squeezeActionStartFrameId >
      m.list[aControllerIndex].squeezeActionStopFrameId) {
    m.list[aControllerIndex].squeezeActionStopFrameId = m.lastImmersiveFrameId;
  }
}

void
ControllerContainer::SetLeftHanded(const int32_t aControllerIndex, const bool aLeftHanded) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }

  m.list[aControllerIndex].leftHanded = aLeftHanded;
}

void
ControllerContainer::SetTouchPosition(const int32_t aControllerIndex, const float aTouchX, const float aTouchY) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }
  Controller& controller = m.list[aControllerIndex];
  controller.touched = true;
  controller.touchX = aTouchX;
  controller.touchY = aTouchY;
}

void
ControllerContainer::EndTouch(const int32_t aControllerIndex) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }
  m.list[aControllerIndex].touched = false;
  m.list[aControllerIndex].touchX = 0;
  m.list[aControllerIndex].touchY = 0;
}

void
ControllerContainer::SetScrolledDelta(const int32_t aControllerIndex, const float aScrollDeltaX, const float aScrollDeltaY) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }
  Controller& controller = m.list[aControllerIndex];
  controller.scrollDeltaX = aScrollDeltaX;
  controller.scrollDeltaY = aScrollDeltaY;
}

void
ControllerContainer::SetBatteryLevel(const int32_t aControllerIndex, const int32_t aBatteryLevel) {
  if (!m.Contains(aControllerIndex)) {
    return;
  }
  m.list[aControllerIndex].batteryLevel = aBatteryLevel;
}
void ControllerContainer::SetPointerColor(const vrb::Color& aColor) const {
  m.pointerColor = aColor;
  for (Controller& controller: m.list) {
    m.updatePointerColor(controller);
  }
}

bool
ControllerContainer::IsVisible() const {
  return m.visible;
}

void
ControllerContainer::SetVisible(const bool aVisible) {
  VRB_LOG("[ControllerContainer] SetVisible %d", aVisible)
  if (m.visible == aVisible) {
    return;
  }
  m.visible = aVisible;
  if (aVisible) {
    for (int i = 0; i < m.list.size(); ++i) {
      if (m.list[i].enabled) {
        m.root->ToggleChild(*m.list[i].transform, true);
      }
    }
  } else {
    m.root->ToggleAll(false);
  }
}

void
ControllerContainer::SetGazeModeIndex(const int32_t aControllerIndex) {
  m.gazeIndex = aControllerIndex;
}

void ControllerContainer::SetJointsMatrices(const int32_t aControllerIndex, const std::string name, const float *matrices) {
  GroupPtr model = m.models[aControllerIndex];

    if (model == nullptr) {
        VRB_ERROR("Null pointer, file: %s, function: %s, line: %d", __FILE__, __FUNCTION__, __LINE__);
        return;
    }

  for(int i = 0; i < model->GetNodeCount(); i++) {
    if(name == model->GetNode(i)->GetName()){
      GeometryPtr geometry = std::dynamic_pointer_cast<vrb::Geometry>(model->GetNode(i));
      if (geometry) {
        geometry->GetRenderState()->SetJointsMatrices(matrices);
      }
      break;
    }
  }
}

void
ControllerContainer::SetFrameId(const uint64_t aFrameId) {
  if (m.immersiveFrameId) {
    m.lastImmersiveFrameId = aFrameId ? aFrameId : m.immersiveFrameId;
  } else {
    m.lastImmersiveFrameId = 0;
    for (Controller& controller: m.list) {
      controller.selectActionStartFrameId = controller.selectActionStopFrameId = 0;
      controller.squeezeActionStartFrameId = controller.squeezeActionStopFrameId = 0;
    }
  }
  m.immersiveFrameId = aFrameId;
}

ControllerContainer::ControllerContainer(State& aState, vrb::CreationContextPtr& aContext) : m(aState) {
  m.Initialize(aContext);
}

ControllerContainer::~ControllerContainer() {
  if (m.root) {
    m.root->RemoveFromParents();
    m.root = nullptr;
  }
}

} // namespace crow
