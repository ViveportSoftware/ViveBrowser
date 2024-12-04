//
// Created by Christ_Chen on 2024/9/24.
//

#include "HandGeometry.h"
#include "vrb/ConcreteClass.h"
#include "vrb/RenderBuffer.h"
#include "vrb/Geometry.h"
#include "vrb/private/GeometryState.h"
#include "vrb/GLError.h"
#include "vrb/Logger.h"
#include "vrb/Camera.h"
#include "vrb/Matrix.h"
#include "vrb/RenderBuffer.h"
#include "vrb/RenderState.h"
#include "vrb/Texture.h"
#include "vrb/VertexArray.h"
#include "vrb/Vector.h"

#include <vector>

namespace crow {

struct HandGeometry::State: public vrb::Geometry::State {
    vrb::ProgramPtr contouringProgram;
    vrb::ProgramPtr fillingProgram;
};

HandGeometryPtr
HandGeometry::Create(vrb::CreationContextPtr& aContext) {
  return std::make_shared<vrb::ConcreteClass<HandGeometry, HandGeometry::State> >(aContext);
}

void HandGeometry::Draw(const vrb::Camera &aCamera, const vrb::Matrix &aModelTransform) {

  //Christ: enhance hand model, write depth buffer to cull overlay fragments.
  //cache depth and alpha setting.
  GLboolean oldDepth, oldAlpha;
  GLboolean depthMask = GL_TRUE;
  GLboolean colorMaskes[4] = {GL_TRUE};

  GLboolean oldCullingFace;
  GLboolean lastPolygonOffsetFill;
  GLfloat lastFactor, lastUnits;
  oldDepth = VRB_GL_CHECK(glIsEnabled(GL_DEPTH_TEST));
  VRB_GL_CHECK(glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask));
  VRB_GL_CHECK(glGetBooleanv(GL_COLOR_WRITEMASK, colorMaskes));

  oldAlpha = VRB_GL_CHECK(glIsEnabled(GL_BLEND));
  lastPolygonOffsetFill = VRB_GL_CHECK(glIsEnabled(GL_POLYGON_OFFSET_FILL));
  oldCullingFace = VRB_GL_CHECK(glIsEnabled(GL_CULL_FACE));
  VRB_GL_CHECK(glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &lastFactor));
  VRB_GL_CHECK(glGetFloatv(GL_POLYGON_OFFSET_UNITS, &lastUnits));

  //-------------------- step 1 --------------------
  //update depth buffer only
  VRB_GL_CHECK(glCullFace(GL_BACK));
  VRB_GL_CHECK(glEnable(GL_DEPTH_TEST));
  VRB_GL_CHECK(glDepthMask(GL_TRUE));
  VRB_GL_CHECK(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));
  m.renderState->SetProgram(m.fillingProgram);
  DrawImplement(m.renderState ,aCamera, aModelTransform, 0, mFillingOpacity);
  VRB_GL_CHECK(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));

  //-------------------- step 2 --------------------
  //cache depthFunc state
  GLint depthFunc= -1;
  VRB_GL_CHECK(glGetIntegerv(GL_DEPTH_FUNC, &depthFunc));
  VRB_GL_CHECK(glDepthFunc(GL_LEQUAL));
  VRB_GL_CHECK(glEnable(GL_CULL_FACE));
  VRB_GL_CHECK(glEnable(GL_BLEND));
  VRB_GL_CHECK(glBlendFuncSeparate(
          GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
          GL_ONE, GL_ONE));
  VRB_GL_CHECK(glFrontFace(GL_CCW));
  VRB_GL_CHECK(glCullFace(GL_FRONT));
  m.renderState->SetProgram(m.contouringProgram);
  DrawImplement(m.renderState ,aCamera, aModelTransform, mThickness, mContouringOpacity);

  //-------------------- step 3 --------------------
  VRB_GL_CHECK(glBlendFuncSeparate(
          GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
          GL_ONE, GL_ONE));
  VRB_GL_CHECK(glCullFace(GL_BACK));

  m.renderState->SetProgram(m.fillingProgram);
  DrawImplement(m.renderState ,aCamera, aModelTransform, 0, mFillingOpacity);

  //restore depthFunc state
  if(depthFunc > 0)
  VRB_GL_CHECK(glDepthFunc(depthFunc));

  VRB_GL_CHECK(glBlendFuncSeparate(
          GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
          GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));


  //status recovering.
  if (lastPolygonOffsetFill == GL_TRUE) {
    VRB_GL_CHECK(glEnable(GL_POLYGON_OFFSET_FILL));
  } else {
    VRB_GL_CHECK(glDisable(GL_POLYGON_OFFSET_FILL));
  }
  VRB_GL_CHECK(glPolygonOffset(lastFactor, lastUnits));

  if (oldDepth == GL_TRUE) {
    VRB_GL_CHECK(glEnable(GL_DEPTH_TEST));
  } else {
    VRB_GL_CHECK(glDisable(GL_DEPTH_TEST));
  }
  VRB_GL_CHECK(glDepthMask(depthMask));

  if (oldCullingFace == GL_TRUE) {
    VRB_GL_CHECK(glEnable(GL_CULL_FACE));
  } else {
    VRB_GL_CHECK(glDisable(GL_CULL_FACE));
  }
  if (oldAlpha == GL_TRUE) {
    VRB_GL_CHECK(glEnable(GL_BLEND));
  } else {
    VRB_GL_CHECK(glDisable(GL_BLEND));
  }

  VRB_GL_CHECK(glColorMask(colorMaskes[0], colorMaskes[1], colorMaskes[2], colorMaskes[3]));
}

void
HandGeometry::DrawImplement(const vrb::RenderStatePtr &renderState, const vrb::Camera &aCamera,
                            const vrb::Matrix &aModelTransform, float thickness,
                            float opacity) {
  bool enable = renderState->Enable(aCamera.GetPerspective(), aCamera.GetView(), aModelTransform);
  if (enable) {
    const bool kUseTexture = m.UseTexture();
    const bool kUseTextureUV2 = m.UseTextureUV2();
    const bool kUseColor = m.UseColor();
    const bool kUseJoints = m.UseJoints();
    const GLsizei kSize = m.renderBuffer->VertexSize();
    m.renderBuffer->Bind();

    GLint uniformOpacity = -1;
    GLint uniformGraColorA = -1;
    GLint uniformGraColorB = -1;
    GLint uniformThickness = -1;
    GLint attributeUV2 = -1;

    renderState->TryGetUniform("opacity", uniformOpacity);
    renderState->TryGetUniform("graColorA", uniformGraColorA);
    renderState->TryGetUniform("graColorB", uniformGraColorB);
    renderState->TryGetUniform("thickness", uniformThickness);
    renderState->TryGetAttribute("a_uv2", attributeUV2);

    //VRB_ERROR("[DrawImplement] attributeUV2: %d",attributeUV2);


    if (uniformThickness >= 0)
    VRB_GL_CHECK(glUniform1f(uniformThickness, thickness));
    if (uniformOpacity >= 0)
    VRB_GL_CHECK(glUniform1f(uniformOpacity, opacity));
    if (uniformGraColorA >= 0)
    VRB_GL_CHECK(glUniform4fv(uniformGraColorA, 1, mGraColorA));
    if (uniformGraColorB >= 0)
    VRB_GL_CHECK(glUniform4fv(uniformGraColorB, 1, mGraColorB));

    VRB_GL_CHECK(glVertexAttribPointer((GLuint) renderState->AttributePosition(),
                                       m.renderBuffer->PositionLength(), GL_FLOAT, GL_FALSE, kSize,
                                       (const GLvoid *) m.renderBuffer->PositionOffset()));


    VRB_GL_CHECK(glVertexAttribPointer((GLuint) renderState->AttributeNormal(),
                                         m.renderBuffer->NormalLength(), GL_FLOAT, GL_FALSE, kSize,
                                         (const GLvoid *) m.renderBuffer->NormalOffset()));

    if (kUseTexture) {
      VRB_GL_CHECK(glVertexAttribPointer((GLuint)renderState->AttributeUV(), m.renderBuffer->UVLength(), GL_FLOAT,
                                         GL_FALSE, kSize, (const GLvoid*)m.renderBuffer->UVOffset()));
    }
    if (kUseColor) {
      VRB_GL_CHECK(glVertexAttribPointer((GLuint)renderState->AttributeColor(), m.renderBuffer->ColorLength(),
                                         GL_FLOAT, GL_FALSE, kSize, (const GLvoid*)m.renderBuffer->ColorOffset()));
    }

    if (kUseJoints && renderState->AttributeJoint() >= 0 ) {
      VRB_GL_CHECK(glVertexAttribPointer((GLuint) renderState->AttributeJoint(),
                                         m.renderBuffer->JointIdLength(), GL_FLOAT, GL_FALSE,
                                         kSize, (const GLvoid*) m.renderBuffer->JointIdOffset()));
      VRB_GL_CHECK(glVertexAttribPointer((GLuint) renderState->AttributeJointWeight(),
                                         m.renderBuffer->JointWeightLength(), GL_FLOAT, GL_FALSE, kSize,
                                         (const GLvoid*) m.renderBuffer->JointWeightOffset()));
    }
    if(attributeUV2>=0){
      VRB_GL_CHECK(glVertexAttribPointer((GLuint)attributeUV2, m.renderBuffer->UV2Length(), GL_FLOAT,
                                         GL_FALSE, kSize, (const GLvoid*)m.renderBuffer->UV2Offset()));
    }

    VRB_GL_CHECK(glEnableVertexAttribArray((GLuint)renderState->AttributePosition()));
    VRB_GL_CHECK(glEnableVertexAttribArray((GLuint)renderState->AttributeNormal()));
    if (kUseTexture) {
      VRB_GL_CHECK(glEnableVertexAttribArray((GLuint)renderState->AttributeUV()));
    }
    if (kUseColor) {
      VRB_GL_CHECK(glEnableVertexAttribArray((GLuint)renderState->AttributeColor()));
    }
    if (kUseJoints && renderState->AttributeJoint() >= 0 ) {
      VRB_GL_CHECK(glEnableVertexAttribArray((GLuint) renderState->AttributeJoint()));
      VRB_GL_CHECK(glEnableVertexAttribArray((GLuint) renderState->AttributeJointWeight()));
    }
    if(attributeUV2>=0){
      VRB_GL_CHECK(glEnableVertexAttribArray((GLuint)attributeUV2));
    }

    const int32_t maxLength = m.renderBuffer->IndexCount();
    if (m.rangeLength == 0) {
      VRB_GL_CHECK(glDrawElements(GL_TRIANGLES, maxLength, GL_UNSIGNED_SHORT, 0));
    } else if ((m.rangeStart + m.rangeLength) <= maxLength) {
      VRB_GL_CHECK(glDrawElements(GL_TRIANGLES, m.rangeLength, GL_UNSIGNED_SHORT, (void*)(m.rangeStart * sizeof(GLushort))));
    } else {
      VRB_WARN("Invalid geometry range (%u-%u). Max geometry length %d", m.rangeStart, m.rangeLength + m.rangeLength, maxLength);
    }
    VRB_GL_CHECK(glDisableVertexAttribArray((GLuint)renderState->AttributePosition()));
    VRB_GL_CHECK(glDisableVertexAttribArray((GLuint)renderState->AttributeNormal()));
    if (kUseTexture) {
      VRB_GL_CHECK(glDisableVertexAttribArray((GLuint)renderState->AttributeUV()));
    }
    if (kUseColor) {
      VRB_GL_CHECK(glDisableVertexAttribArray((GLuint)renderState->AttributeColor()));
    }
    if (kUseJoints && renderState->AttributeJoint() >= 0 ) {
      VRB_GL_CHECK(glDisableVertexAttribArray((GLuint) renderState->AttributeJoint()));
      VRB_GL_CHECK(glDisableVertexAttribArray((GLuint) renderState->AttributeJointWeight()));
    }
    if(attributeUV2>=0){
      VRB_GL_CHECK(glDisableVertexAttribArray((GLuint)attributeUV2));
    }

    m.renderBuffer->Unbind();
    renderState->Disable();
  }
}

HandGeometry::HandGeometry(HandGeometry::State &aState, vrb::CreationContextPtr &aContext)
    : Geometry::Geometry(aState, aContext),
    m(aState) {
}

HandGeometry::~HandGeometry() {}

void HandGeometry::SetContouringProgram(const vrb::ProgramPtr &program) {
  m.contouringProgram = program;
}

void HandGeometry::SetFillingProgram(const vrb::ProgramPtr &program) {
  m.fillingProgram = program;
}

} // crow