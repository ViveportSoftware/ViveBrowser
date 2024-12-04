/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Pointer.h"
#include "DeviceDelegate.h"
#include "VRLayer.h"
#include "VRLayerNode.h"
#include "VRBrowser.h"
#include "Widget.h"
#include "vrb/ConcreteClass.h"
#include "vrb/Color.h"
#include "vrb/CreationContext.h"
#include "vrb/Geometry.h"
#include "vrb/Matrix.h"
#include "vrb/ModelLoaderAndroid.h"
#include "vrb/Program.h"
#include "vrb/ProgramFactory.h"
#include "vrb/RenderState.h"
#include "vrb/RenderContext.h"
#include "vrb/TextureCubeMap.h"
#include "vrb/Toggle.h"
#include "vrb/Transform.h"
#include "vrb/VertexArray.h"

#include <array>

#define POINTER_COLOR_OUTER vrb::Color(0.0f, 0.0f, 0.0f)
#define POINTER_COLOR_RING vrb::Color(1.0f, 1.0f, 1.0f, 0.0f)
#define POINTER_COLOR_CIRCLE vrb::Color(0.0f, 179.0f / 255.0f, 227.0f / 255.0f)

using namespace vrb;

namespace crow {

    const float kOffset = 0.01f;
    const int kResolution = 24;
    const float kRingInnerRadius = 0.008f;
    const float kRingOuterRadius = 0.01168f;
    const float kCircleInnerRadius = kRingOuterRadius / 2.0f;
    const float kPi32 = float(M_PI);



    struct Pointer::State {
        vrb::CreationContextWeak context;
        vrb::TogglePtr root;
        VRLayerQuadPtr layer;
        vrb::TransformPtr transform;
        vrb::TransformPtr pointerScale;
        vrb::TogglePtr shapeToggle;


        vrb::GeometryPtr circleGeometry;
        vrb::GeometryPtr circleShadow;
        vrb::GeometryPtr ringGeometry;
        vrb::GeometryPtr ringShadow;


        WidgetPtr hitWidget;
        vrb::Color pointerColor;
        Shape mShape = Shape::Ring;

        State() = default;
        ~State() {
            if (root) {
                root->RemoveFromParents();
            }
        }

        void Initialize() {
            vrb::CreationContextPtr create = context.lock();
            root = vrb::Toggle::Create(create);
            transform = vrb::Transform::Create(create);
            pointerScale = vrb::Transform::Create(create);
            pointerScale->SetTransform(vrb::Matrix::Identity());
            transform->AddNode(pointerScale);
            shapeToggle = vrb::Toggle::Create(create);
            pointerScale->AddNode(shapeToggle);
            root->AddNode(transform);
            root->ToggleAll(false);
            pointerColor = POINTER_COLOR_RING;
        }

        vrb::GeometryPtr createCircle(const int resolution, const float radius, const float zOffset, const float yOffset = 0.0f) {
            vrb::CreationContextPtr create = context.lock();
            vrb::GeometryPtr geometry = vrb::Geometry::Create(create);
            vrb::VertexArrayPtr array = vrb::VertexArray::Create(create);
            geometry->SetVertexArray(array);

            array->AppendNormal(vrb::Vector(0.0f, 0.0f, 1.0f));

            for (int i = 0; i <= resolution; i++) {
                std::vector<int> normalIndex;
                normalIndex.push_back(0);
                normalIndex.push_back(0);
                normalIndex.push_back(0);

                std::vector<int> index;

                array->AppendVertex(vrb::Vector(0.0f, 0.0f + yOffset, zOffset));
                index.push_back(i*3 + 1);

                array->AppendVertex(vrb::Vector(
                        radius * cosf(i * kPi32 * 2 / resolution),
                        radius * sinf(i * kPi32 * 2 / resolution) + yOffset,
                        zOffset));
                index.push_back(i*3 + 2);

                array->AppendVertex(vrb::Vector(
                        radius * cosf((i + 1) * kPi32 * 2 / resolution),
                        radius * sinf((i + 1) * kPi32 * 2 / resolution) + yOffset,
                        zOffset));
                index.push_back(i*3 + 3);

                std::vector<int> uvIndex;

                geometry->AddFace(index, uvIndex, normalIndex);
            }

            return geometry;
        }


        vrb::GeometryPtr createRing(const int resolution, const float innerRadius, const float outerRadius, const float zOffset, const float yOffset = 0.0f) {
            vrb::CreationContextPtr create = context.lock();
            vrb::GeometryPtr geometry = vrb::Geometry::Create(create);
            vrb::VertexArrayPtr array = vrb::VertexArray::Create(create);
            geometry->SetVertexArray(array);

            array->AppendNormal(vrb::Vector(0.0f, 0.0f, 1.0f));

            for (int i = 0; i <= resolution; i++) {
                float angle = i * 2.0f * kPi32 / resolution;
                array->AppendVertex(vrb::Vector(innerRadius * cosf(angle), innerRadius * sinf(angle) + yOffset, zOffset));
            }

            for (int i = 0; i <= resolution; i++) {
                float angle = i * 2.0f * kPi32 / resolution;
                array->AppendVertex(vrb::Vector(outerRadius * cosf(angle), outerRadius * sinf(angle) + yOffset, zOffset));
            }

            for (int i = 1; i <= resolution; i++) {
                int innerIndex1 = i;
                int innerIndex2 = i + 1;
                int outerIndex1 = innerIndex1 + resolution + 1;
                int outerIndex2 = innerIndex2 + resolution + 1;

                std::vector<int> indices1 = {innerIndex1, outerIndex1, outerIndex2};
                geometry->AddFace(indices1, {}, {});

                std::vector<int> indices2 = {innerIndex1, outerIndex2, innerIndex2};
                geometry->AddFace(indices2, {}, {});
            }

            return geometry;
        }

        void LoadGeometry() {
            VRB_LOG("VRBROSER LoadGeometry Shape::Ring")
            vrb::CreationContextPtr create = context.lock();



            ringGeometry = createRing(kResolution, kRingInnerRadius, kRingOuterRadius, kOffset);
            vrb::ProgramPtr ringPogram = create->GetProgramFactory()->CreateProgram(create, 0);
            vrb::RenderStatePtr ringState = vrb::RenderState::Create(create);
            ringState->SetProgram(ringPogram);
            ringState->SetMaterial(pointerColor, pointerColor, vrb::Color(0.0f, 0.0f, 0.0f), 0.0f);
            ringGeometry->SetRenderState(ringState);
            shapeToggle->AddNode(ringGeometry);

            VRB_LOG("VRBROSER LoadGeometry Shape::Circle")
            circleGeometry = createCircle(kResolution, kCircleInnerRadius, kOffset);
            //vrb::GeometryPtr geometryOuter = createCircle(kResolution, kOuterRadius, kOffset);
            vrb::ProgramPtr circleProgram = create->GetProgramFactory()->CreateProgram(create, 0);
            vrb::RenderStatePtr circleState = vrb::RenderState::Create(create);
            circleState->SetProgram(circleProgram);
            circleState->SetMaterial(POINTER_COLOR_CIRCLE, POINTER_COLOR_CIRCLE, vrb::Color(0.0f, 0.0f, 0.0f), 0.0f);
            circleGeometry->SetRenderState(circleState);
            shapeToggle->AddNode(circleGeometry);


            VRB_LOG("VRBROSER LoadGeometry Shadow")
            ringShadow = createRing(kResolution, kRingInnerRadius, kRingOuterRadius, kOffset, -0.002);
            vrb::ProgramPtr ringShadowProgram = create->GetProgramFactory()->CreateProgram(create, 0);
            vrb::RenderStatePtr ringShadowState = vrb::RenderState::Create(create);
            ringShadowState->SetProgram(ringShadowProgram);
            ringShadowState->SetMaterial(POINTER_COLOR_OUTER, POINTER_COLOR_OUTER, vrb::Color(0.0f, 0.0f, 0.0f), 0.0f);
            ringShadow->SetRenderState(ringShadowState);
            shapeToggle->AddNode(ringShadow);

            circleShadow = createCircle(kResolution, kCircleInnerRadius, kOffset, -0.002);
            //vrb::GeometryPtr geometryOuter = createCircle(kResolution, kOuterRadius, kOffset);
            vrb::ProgramPtr circleShadowProgram = create->GetProgramFactory()->CreateProgram(create, 0);
            vrb::RenderStatePtr circleShadowState = vrb::RenderState::Create(create);
            circleShadowState->SetProgram(circleShadowProgram);
            circleShadowState->SetMaterial(POINTER_COLOR_OUTER, POINTER_COLOR_OUTER, vrb::Color(0.0f, 0.0f, 0.0f), 0.0f);
            circleShadow->SetRenderState(circleShadowState);
            shapeToggle->AddNode(circleShadow);



            mShape = Shape::Ring;
            shapeToggle->ToggleChild(*circleGeometry, false);
            shapeToggle->ToggleChild(*circleShadow, false);
        }

    };

    bool
    Pointer::IsLoaded() const {
        return m.layer || (m.ringGeometry && m.circleGeometry);
    }

    void
    Pointer::Load(const DeviceDelegatePtr& aDevice) {
        VRLayerQuadPtr layer = aDevice->CreateLayerQuad(36, 36, VRLayerQuad::SurfaceType::AndroidSurface);
        if (layer) {
            m.layer = layer;
            m.layer->SetTintColor(m.pointerColor);
            const float size = kRingOuterRadius *  2.0f;
            layer->SetWorldSize(size, size);
            layer->SetSurfaceChangedDelegate([](const VRLayer& aLayer, VRLayer::SurfaceChange aChange, const std::function<void()>& aCallback) {
                auto& quad = static_cast<const VRLayerQuad&>(aLayer);
                if (aChange == VRLayer::SurfaceChange::Create ||
                    aChange == VRLayer::SurfaceChange::Invalidate) {
                    auto pointerColor = quad.GetTintColor();
                    // @FIXME: Move this to vrb::Color::toAndroidColor() eventually.
                    int32_t color = ((int32_t) (255.0f * pointerColor.Alpha()) << 24) |
                                    ((int32_t) (255.0f * pointerColor.Red()) << 16) |
                                    ((int32_t) (255.0f * pointerColor.Green()) << 8) |
                                    ((int32_t) (255.0f * pointerColor.Blue()));
                    VRBrowser::RenderPointerLayer(quad.GetSurface(), color, aCallback);
                }
            });
            vrb::CreationContextPtr create = m.context.lock();
            m.pointerScale->AddNode(VRLayerNode::Create(create, layer));
        } else {
            m.LoadGeometry();
        }
    }

    void
    Pointer::SetVisible(bool aVisible) {
        m.root->ToggleAll(aVisible);

    }

    void
    Pointer::SetTransform(const vrb::Matrix& aTransform) {
        m.transform->SetTransform(aTransform);
    }

    void
    Pointer::SetScale(const float scale) {
        if (m.layer) {
            float size = kRingOuterRadius *  2.0f * scale;
            m.layer->SetWorldSize(size, size);
        } else {
            m.pointerScale->SetTransform(vrb::Matrix::Identity().ScaleInPlace(
                    vrb::Vector(scale, scale, 1.0)));
        }
    }

    void
    Pointer::SetPointerColor(const vrb::Color& aColor) {
        if (aColor == m.pointerColor)
            return;

        m.pointerColor = aColor;
        if (m.layer) {
            m.layer->SetTintColor(aColor);
            m.layer->NotifySurfaceChanged(VRLayer::SurfaceChange::Invalidate, NULL);
        }
        if ((m.mShape == Shape::Ring) && (m.ringGeometry)) {
            m.ringGeometry->GetRenderState()->SetMaterial(aColor, aColor, vrb::Color(0.0f, 0.0f, 0.0f), 0.0f);
        }
    }



    void
    Pointer::SetShape(const Shape shape){
        if(m.mShape == shape){
            //VRB_WARN("VRBROWSER Pointer::SetShape not need change, return ")
            return;
        }
        m.mShape = shape;
        if(shape == Shape::Ring){
            m.shapeToggle->ToggleChild(*m.ringGeometry, true);
            m.shapeToggle->ToggleChild(*m.ringShadow, true);
            m.shapeToggle->ToggleChild(*m.circleGeometry, false);
            m.shapeToggle->ToggleChild(*m.circleShadow, false);
        }
        else{
            m.shapeToggle->ToggleChild(*m.ringGeometry, false);
            m.shapeToggle->ToggleChild(*m.ringShadow, false);
            m.shapeToggle->ToggleChild(*m.circleGeometry, true);
            m.shapeToggle->ToggleChild(*m.circleShadow, true);
        }
    }



    void
    Pointer::SetHitWidget(const crow::WidgetPtr &aWidget) {
        m.hitWidget = aWidget;
        if (m.layer) {
            m.layer->SetDrawInFront(aWidget && aWidget->IsResizing());
        }
    }

    vrb::NodePtr
    Pointer::GetRoot() const {
        return m.root;
    }

    const WidgetPtr&
    Pointer::GetHitWidget() const {
        return m.hitWidget;
    }


    PointerPtr
    Pointer::Create(vrb::CreationContextPtr aContext) {
        PointerPtr result = std::make_shared<vrb::ConcreteClass<Pointer, Pointer::State> >(aContext);
        if(result){
            result->m.Initialize();
        }
        else{
            VRB_ERROR("Null pointer, file: %s, function: %s, line: %d",__FILE__, __FUNCTION__, __LINE__);
        }
        return result;
    }


    Pointer::Pointer(State& aState, vrb::CreationContextPtr& aContext) : m(aState) {
        m.context = aContext;
    }

} // namespace crow
