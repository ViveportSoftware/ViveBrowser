//
// Created by Christ_Chen on 2024/9/24.
//
#include "vrb/Geometry.h"
#include "vrb/Forward.h"

#ifndef VIVE_BROWSER_CHROMIUM_HANDGEOMETRY_H
#define VIVE_BROWSER_CHROMIUM_HANDGEOMETRY_H

namespace crow {
  class HandGeometry;
  typedef std::shared_ptr<HandGeometry> HandGeometryPtr;
//TODO
// Move the rendering code in BrowserWorld::DrawWorld to HandGeometry.
  class HandGeometry : public vrb::Geometry{
  public:
    static HandGeometryPtr Create(vrb::CreationContextPtr& aContext);
    void Draw(const vrb::Camera &aCamera, const vrb::Matrix &aModelTransform) override;
    void SetFillingProgram(const vrb::ProgramPtr &program);
    void SetContouringProgram(const vrb::ProgramPtr &program);
  protected:
    struct State;
    HandGeometry(State& aState, vrb::CreationContextPtr& aContext);
    ~HandGeometry();
  private:
    const float mThickness=0.001f;
    const float mFillingOpacity=0.5f;
    const float mContouringOpacity=0.5f;
    const float mGraColorA[4]{(255.0f/255.0f), (255.0f/255.0f), (255.0f/255.0f), 0.0f};
    const float mGraColorB[4]{(255.0f/255.0f), (255.0f/255.0f), (255.0f/255.0f), 0.0f};

    void DrawImplement(const vrb::RenderStatePtr& aRenderState, const vrb::Camera &aCamera, const vrb::Matrix &aModelTransform,
                       float thickness, float opacity);

    State& m;
    HandGeometry() = delete;
  };

} // crow

#endif //VIVE_BROWSER_CHROMIUM_HANDGEOMETRY_H
