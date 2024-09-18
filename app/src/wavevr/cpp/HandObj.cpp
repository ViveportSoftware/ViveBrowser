#define LOG_TAG "APHandObj"

//#define DRAW_AXIS

#include "log.h"

#include "Context.h"
#include "shared/quat.h"

#include "HandManager.h"
#include "HandObj.h"
#include "vrb/GLError.h"

HandObj::HandObj(HandManager *iMgr, HandTypeEnum iHandType)
    : mManager(iMgr), mHandType(iHandType), mThickness(0.001f), mHandScale(1.0f, 1.0f, 1.0f),
      mContouringOpacity(0.5f), mFillingOpacity(0.5f),
      mGraColorA{(29.0f / 255.0f), (189.0f / 255.0f), (247.0f / 255.0f), 0.0f},
      mGraColorB{(191.0f / 255.0f), (182.0f / 255.0f), (182.0f / 255.0f), 0.0f} {
  mShift.translate(1, 1.5, 2);
}

HandObj::~HandObj() {
  releaseGraphicsSystem();
}

bool HandObj::initializeGraphicsSystem() {
  bool success = true;
  const char *handFragmentP0 = "#version 300 es\n"
                               "precision mediump float;\n out vec4 oColor;\n"
                               //                                 "void main()\n{\n oColor = vec4(0, 0, 0, 1);\n}";
                               "void main()\n{\n oColor = vec4(1, 0, 0, 1);\n}";
  const char *handFragmentP1 = "#version 300 es\n"
                               "\n"
                               "precision mediump float;\n"
                               "\n"
                               "uniform vec4 graColorA;\n"
                               "uniform vec4 graColorB;\n"
                               "uniform float opacity;\n"
                               "uniform sampler2D alphaTex;\n"
                               "\n"
                               "in vec2 v2fCoord1;\n"
                               "in vec2 v2fCoord2;\n"
                               "\n"
                               "out vec4 oColor; \n"
                               "\n"
                               "void main()\n"
                               "{\n"
                               "    float smoothStepResult99 = smoothstep(0.7, 0.38, 1.0 - v2fCoord2.y);\n"
                               "    vec4 lerpResult100 = mix(graColorA, graColorB, smoothStepResult99);\n"
                               "    float tex2DNode92 = texture(alphaTex, v2fCoord1).r;\n"
                               //                                 "    oColor = vec4(lerpResult100.r, lerpResult100.g, lerpResult100.b, opacity * tex2DNode92);\n"
                               "      oColor = vec4(1, 0, 0, 1);\n"
                               "}";
  const char *handFragmentP2 = "#version 300 es\n"
                               "\n"
                               "precision mediump float;\n"
                               "\n"
                               "uniform vec4 graColorA;\n"
                               "uniform vec4 graColorB;\n"
                               "uniform float opacity;\n"
                               "uniform sampler2D alphaTex;\n"
                               "\n"
                               "in vec2 v2fCoord1;\n"
                               "in vec2 v2fCoord2;\n"
                               "\n"
                               "out vec4 oColor; \n"
                               "\n"
                               "void main()\n"
                               "{\n"
                               "    float smoothStepResult99 = smoothstep(0.7, 0.38, 1.0 - v2fCoord2.y);\n"
                               "    vec4 lerpResult100 = mix(graColorA, graColorB, smoothStepResult99);\n"
                               "    float color104 = 0.0;\n"
                               "    float color102 = 1.0;\n"
                               "    float smoothstepResult103 = smoothstep(-0.05, 1.0, 1.0 - v2fCoord2.y);\n"
                               "    float lerpResult105 = mix(color104, color102, smoothstepResult103);\n"
                               "    float tex2DNode92 = texture(alphaTex, v2fCoord1).r;\n"
                               //                                 "    oColor = vec4(lerpResult100.r, lerpResult100.g, lerpResult100.b, lerpResult105 * opacity * tex2DNode92);\n"
                               "      oColor = vec4(1, 0, 0, 1);\n"
                               "}";
  const char *handVertexP0 = "#version 300 es\n"
                             "\n"
                             "#define APPLY_BONE\n"
                             "\n"
                             "uniform mat4 projMat;\n"
                             "uniform mat4 viewMat;\n"
                             "uniform mat4 worldMat;\n"
                             "uniform mat3 normalMat; //vs\n"
                             "uniform mat4 boneMats[48]; //Os\n"
                             "\n"
                             "layout(location = 0) in vec3 v3Position;\n"
                             "layout(location = 1) in vec3 v3Normal;\n"
                             "layout(location = 2) in vec2 v2Coord1;\n"
                             "layout(location = 3) in vec2 v2Coord2;\n"
                             "layout(location = 5) in ivec4 v4BoneID;\n"
                             "layout(location = 6) in vec4 v4BoneWeight;\n"
                             "\n"
                             "void main() {\n"
                             "    vec4 localVertex;\n"
                             "\n"
                             "    //1. calculate vertex data.\n"
                             "#ifdef APPLY_BONE\n"
                             "    mat4 localPose = mat4(1.0);\n"
                             "\n"
                             "    localPose = boneMats[v4BoneID[0]] * v4BoneWeight[0];\n"
                             "    localPose += boneMats[v4BoneID[1]] * v4BoneWeight[1];\n"
                             "    localPose += boneMats[v4BoneID[2]] * v4BoneWeight[2];\n"
                             "    localPose += boneMats[v4BoneID[3]] * v4BoneWeight[3];\n"
                             "\n"
                             "    localVertex = localPose * vec4(v3Position.xyz, 1.0);\n"
                             "#else\n"
                             "    localVertex = vec4(v3Position.xyz, 1.0);\n"
                             "#endif\n"
                             "\n"
                             "    gl_Position = projMat * viewMat * worldMat * localVertex;\n"
                             "}";
  const char *handVertexP1 = "#version 300 es\n"
                             "\n"
                             "#define APPLY_BONE\n"
                             "\n"
                             "uniform mat4 projMat;\n"
                             "uniform mat4 viewMat;\n"
                             "uniform mat4 worldMat;\n"
                             "uniform mat3 normalMat; //vs\n"
                             "uniform mat4 boneMats[48]; //Os\n"
                             "uniform float thickness;\n"
                             "\n"
                             "layout(location = 0) in vec3 v3Position;\n"
                             "layout(location = 1) in vec3 v3Normal;\n"
                             "layout(location = 2) in vec2 v2Coord1;\n"
                             "layout(location = 3) in vec2 v2Coord2;\n"
                             "layout(location = 5) in ivec4 v4BoneID;\n"
                             "layout(location = 6) in vec4 v4BoneWeight;\n"
                             "\n"
                             "out vec2 v2fCoord1;\n"
                             "out vec2 v2fCoord2;\n"
                             "\n"
                             "void main() {\n"
                             "    vec4 localVertex;\n"
                             "    vec4 localNormal;\n"
                             "\n"
                             "    //1. calculate vertex data.\n"
                             "#ifdef APPLY_BONE\n"
                             "    mat4 localPose = mat4(1.0);\n"
                             "\n"
                             "    localPose = boneMats[v4BoneID[0]] * v4BoneWeight[0];\n"
                             "    localPose += boneMats[v4BoneID[1]] * v4BoneWeight[1];\n"
                             "    localPose += boneMats[v4BoneID[2]] * v4BoneWeight[2];\n"
                             "    localPose += boneMats[v4BoneID[3]] * v4BoneWeight[3];\n"
                             "\n"
                             "    localVertex = localPose * vec4(v3Position.xyz, 1.0);\n"
                             "    localNormal = localPose * vec4(v3Normal, 0.0);\n"
                             "#else\n"
                             "    localVertex = vec4(v3Position.xyz, 1.0);\n"
                             "    localNormal = vec4(v3Normal, 0.0);\n"
                             "#endif\n"
                             "    localVertex.xyz += localNormal.xyz * thickness;\n"
                             "    v2fCoord1 = v2Coord1;\n"
                             "    v2fCoord2 = v2Coord2;\n"
                             "\n"
                             "    gl_Position = projMat * viewMat * worldMat * localVertex;\n"
                             "}";
  const char *handVertexP2 = "#version 300 es\n"
                             "\n"
                             "#define APPLY_BONE\n"
                             "\n"
                             "uniform mat4 projMat;\n"
                             "uniform mat4 viewMat;\n"
                             "uniform mat4 worldMat;\n"
                             "uniform mat3 normalMat; //vs\n"
                             "uniform mat4 boneMats[48]; //Os\n"
                             "\n"
                             "layout(location = 0) in vec3 v3Position;\n"
                             "layout(location = 1) in vec3 v3Normal;\n"
                             "layout(location = 2) in vec2 v2Coord1;\n"
                             "layout(location = 3) in vec2 v2Coord2;\n"
                             "layout(location = 5) in ivec4 v4BoneID;\n"
                             "layout(location = 6) in vec4 v4BoneWeight;\n"
                             "\n"
                             "out vec2 v2fCoord1;\n"
                             "out vec2 v2fCoord2;\n"
                             "\n"
                             "void main() {\n"
                             "    vec4 localVertex;\n"
                             "\n"
                             "    //1. calculate vertex data.\n"
                             "#ifdef APPLY_BONE\n"
                             "    mat4 localPose = mat4(1.0);\n"
                             "\n"
                             "    localPose = boneMats[v4BoneID[0]] * v4BoneWeight[0];\n"
                             "    localPose += boneMats[v4BoneID[1]] * v4BoneWeight[1];\n"
                             "    localPose += boneMats[v4BoneID[2]] * v4BoneWeight[2];\n"
                             "    localPose += boneMats[v4BoneID[3]] * v4BoneWeight[3];\n"
                             "\n"
                             "    localVertex = localPose * vec4(v3Position.xyz, 1.0);\n"
                             "#else\n"
                             "    localVertex = vec4(v3Position.xyz, 1.0);\n"
                             "#endif\n"
                             "    v2fCoord1 = v2Coord1;\n"
                             "    v2fCoord2 = v2Coord2;\n"
                             "\n"
                             "    gl_Position = projMat * viewMat * worldMat * localVertex;\n"
                             "}";

  const char *shaderNames[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber] = {
      {
          "handDepth",
          "handContouring",
          "handFilling"
      },
      {
          "handDepthMV",
          "handContouringMV",
          "handFillingMV"
      }
  };
  const char *vpaths[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber] = {
      {
          "shader/vertex/hand_p0_vertex.glsl",
          "shader/vertex/hand_p1_vertex.glsl",
          "shader/vertex/hand_p2_vertex.glsl"
      },
      {
          "shader/vertex/hand_p0_vertex.glsl",
          "shader/vertex/hand_p1_vertex.glsl",
          "shader/vertex/hand_p2_vertex.glsl"
      }
  };
  const char *fpaths[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber] = {
      {
          "shader/fragment/hand_p0_fragment.glsl",
          "shader/fragment/hand_p1_fragment.glsl",
          "shader/fragment/hand_p2_fragment.glsl"
      },
      {
          "shader/fragment/hand_p0_fragment.glsl",
          "shader/fragment/hand_p1_fragment.glsl",
          "shader/fragment/hand_p2_fragment.glsl"
      }
  };

  const char *vertexShader[ShaderProgramID_MaxNumber] = {
      handVertexP0,
      handVertexP1,
      handVertexP2
  };
  const char *fragmentShader[ShaderProgramID_MaxNumber] = {
      handFragmentP0,
      handFragmentP1,
      handFragmentP2
  };
  for (uint32_t mode = 0; mode < DrawMode_MaxModeMumber; ++mode) {
    for (uint32_t pID = 0; pID < ShaderProgramID_MaxNumber; ++pID) {
      mShaders[mode][pID] = Shader::findShader(vpaths[mode][pID], fpaths[mode][pID]);
      if (mShaders[mode][pID] != nullptr) {
        LOGI("(%d): Shader find!!!", mHandType);
      } else {
        const char *vstr = vertexShader[pID];
        const char *fstr = fragmentShader[pID];

        mShaders[mode][pID] = std::make_shared<Shader>(shaderNames[mode][pID],
                                                       vpaths[mode][pID], vstr,
                                                       fpaths[mode][pID], fstr);
        bool ret = mShaders[mode][pID]->compile();

        if (ret == false) {
          LOGE("(%d): Compile shader error!!!", mHandType);
          success = false;
        } else {
          Shader::putShader(mShaders[mode][pID]);
        }
      }
      if (success) {
        mProjMatrixLocations[mode][pID] = mShaders[mode][pID]->getUniformLocation("projMat");
        mViewMatrixLocations[mode][pID] = mShaders[mode][pID]->getUniformLocation("viewMat");
        mWorldMatrixLocations[mode][pID] = mShaders[mode][pID]->getUniformLocation("worldMat");
        mNormalMatrixLocations[mode][pID] = mShaders[mode][pID]->getUniformLocation(
            "normalMat");
        mSkeletonMatrixLocations[mode][pID] = mShaders[mode][pID]->getUniformLocation(
            "boneMats");
        mColorLocations[mode][pID] = mShaders[mode][pID]->getUniformLocation("color");
        mAlphaTexLocations[mode][pID] = mShaders[mode][pID]->getUniformLocation("alphaTex");
        mThicknessLocations[mode][pID] = mShaders[mode][pID]->getUniformLocation("thickness");
        mOpacityLocations[mode][pID] = mShaders[mode][pID]->getUniformLocation("opacity");
        mGraColorALocations[mode][pID] = mShaders[mode][pID]->getUniformLocation("graColorA");
        mGraColorBLocations[mode][pID] = mShaders[mode][pID]->getUniformLocation("graColorB");

        LOGI(
            "H(%d): Mode[%d][%d]: proj(%d) view(%d) world(%d) normal(%d) skeleton(%d) color(%d) alphaTex(%d), thinkness(%d) opac(%d) GCA(%d) GCB(%d)",
            mHandType,
            mode, pID,
            mProjMatrixLocations[mode][pID],
            mViewMatrixLocations[mode][pID],
            mWorldMatrixLocations[mode][pID],
            mNormalMatrixLocations[mode][pID],
            mSkeletonMatrixLocations[mode][pID],
            mColorLocations[mode][pID],
            mAlphaTexLocations[mode][pID],
            mThicknessLocations[mode][pID],
            mOpacityLocations[mode][pID],
            mGraColorALocations[mode][pID],
            mGraColorBLocations[mode][pID]);
      }
    }
  }
  return success;
}

void HandObj::setTexture(Texture *iTexture) {
  mHandAlpha = iTexture;
}

void HandObj::updateSkeleton(const Matrix4 iSkeletonPoses[sMaxSupportJointNumbers],
                             const Vector3 &iHandScale) {
  mHandScale = iHandScale;
  //
  mWristPose = iSkeletonPoses[HandBone_Wrist];
  Matrix4 wristPoseInv = mWristPose;
  wristPoseInv.invert();

  for (uint32_t jCount = 0; jCount < sMaxSupportJointNumbers; ++jCount) {
    mSkeletonPoses[jCount] = (wristPoseInv * iSkeletonPoses[jCount]); // convert to model space.
  }
}

Vector4 HandObj::calculateJointWorldPosition(uint32_t jID) const {
  Vector4 wp;
  Vector3 lp = mHandModel.getModelJointLocalPosition(jID);

  if (mHandModel.mJointParentTable[jID] == sIdentityJoint) {
    wp = Vector4(lp.x, lp.y, lp.z, 1.0f);
  } else {
    uint32_t parentJointID = mHandModel.mJointParentTable[jID];
    Matrix4 parentJointTrans = mFinalSkeletonPoses[parentJointID];
    wp = parentJointTrans * Vector4(lp.x, lp.y, lp.z, 1.0f);
  }

  return wp;
}

void HandObj::loadModel(WVR_HandModel_t *iHandModel) {
  mHandModel.loadModelToGraphicsResource(iHandModel);
}

void HandObj::render(
    const Matrix4 iProj,
    const Matrix4 iEye,
    const Matrix4 &viewMatrix,
    const Matrix4 &iHandPose) {
  //1. cache depth and alpha setting.
  GLboolean oldDepth, oldAlpha;
  GLboolean oldCullingFace;
  GLint oldDepthFunc;
  GLboolean colorMaskes[4] = {GL_TRUE};
  GLboolean depthMask = GL_TRUE;
  GLboolean lastPolygonOffsetFill;
  GLfloat lastFactor, lastUnits;
  oldDepth = glIsEnabled(GL_DEPTH_TEST);
  glGetIntegerv(GL_DEPTH_FUNC, &oldDepthFunc);
  oldAlpha = glIsEnabled(GL_BLEND);
  lastPolygonOffsetFill = glIsEnabled(GL_POLYGON_OFFSET_FILL);
  oldCullingFace = glIsEnabled(GL_CULL_FACE);
  glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &lastFactor);
  glGetFloatv(GL_POLYGON_OFFSET_UNITS, &lastUnits);
  glGetBooleanv(GL_COLOR_WRITEMASK, colorMaskes);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);

  Matrix4 projectionMatrix = iProj * iEye;

  //2. draw
  for (uint32_t jointID = 0; jointID < sMaxSupportJointNumbers; ++jointID) {
    if (mHandModel.mJointUsageTable[jointID] == 1) {
      Vector4 wp = calculateJointWorldPosition(jointID);
      mFinalSkeletonPoses[jointID] = mSkeletonPoses[jointID];
      mFinalSkeletonPoses[jointID][12] = wp.x;
      mFinalSkeletonPoses[jointID][13] = wp.y;
      mFinalSkeletonPoses[jointID][14] = wp.z;
      mModelSkeletonPoses[jointID] =
          mFinalSkeletonPoses[jointID] * mHandModel.mJointInvTransMats[jointID];
    }

    memcpy(mSkeletonMatrices + jointID * 16, mModelSkeletonPoses[jointID].get(),
           16 * sizeof(GLfloat));
  }

  Matrix4 scaleFactor;
  LOGI("Hand(%d) : mHandScale(%lf, %lf, %lf)", mHandType, mHandScale.x, mHandScale.y,
       mHandScale.z);
  scaleFactor.scale(mHandScale.x, mHandScale.y, mHandScale.z);
  Matrix4 scaledPose = mShift * mWristPose * scaleFactor;
  //3. render
  //-------------------- step 1 --------------------
  uint32_t mode = DrawMode_General;
  if (mHandModel.mInitialized == false) {
    return;
  }

  Shader *targetShader = nullptr;
  uint32_t stepID = ShaderProgramID_FinalDepthWriting;
  targetShader = mShaders[mode][stepID].get();

  VRB_GL_CHECK(glEnable(GL_DEPTH_TEST));
  VRB_GL_CHECK(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));
  VRB_GL_CHECK(glDepthMask(GL_TRUE));
  if (targetShader == nullptr) {
    LOGI("target shader nullptr");
    return;
  }

  if (mHandAlpha == nullptr) {
    LOGI("mHandAlpha nullptr");
    return;
  }
  targetShader->useProgram();

  VRB_GL_CHECK(glActiveTexture(GL_TEXTURE0));
  mHandAlpha->bindTexture();
  VRB_GL_CHECK(glUniform1i(mAlphaTexLocations[mode][stepID], 0));
  VRB_GL_CHECK(glUniform1f(mThicknessLocations[mode][stepID], mThickness));
  VRB_GL_CHECK(glUniform1f(mOpacityLocations[mode][stepID], mContouringOpacity));
  VRB_GL_CHECK(glUniform4fv(mGraColorALocations[mode][stepID], 1, mGraColorA));
  VRB_GL_CHECK(glUniform4fv(mGraColorBLocations[mode][stepID], 1, mGraColorB));
  VRB_GL_CHECK(
      glUniformMatrix4fv(mProjMatrixLocations[mode][stepID], 1, false, projectionMatrix.get()));
  VRB_GL_CHECK(glUniformMatrix4fv(mViewMatrixLocations[mode][stepID], 1, false, viewMatrix.get()));
  VRB_GL_CHECK(glUniformMatrix4fv(mWorldMatrixLocations[mode][stepID], 1, false, scaledPose.get()));
  VRB_GL_CHECK(
      glUniformMatrix4fv(mSkeletonMatrixLocations[mode][stepID], sMaxSupportJointNumbers, false,
                         mSkeletonMatrices));

  mHandModel.mMesh.draw();

  VRB_GL_CHECK(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));

  //-------------------- step 2 --------------------
  stepID = ShaderProgramID_FinalContouring;
  targetShader = mShaders[mode][stepID].get();

  if (targetShader == nullptr) {
    LOGI("target shader nullptr");
    return;
  }

  if (mHandAlpha == nullptr) {
    LOGI("mHandAlpha nullptr");
    return;
  }

  glEnable(GL_CULL_FACE);
  glEnable(GL_BLEND);

  glBlendFuncSeparate(
      GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
      GL_ONE, GL_ONE);
  glFrontFace(GL_CCW);
  glCullFace(GL_FRONT);

  targetShader->useProgram();
  glActiveTexture(GL_TEXTURE0);
  mHandAlpha->bindTexture();

  glUniform1i(mAlphaTexLocations[mode][stepID], 0);
  glUniform1f(mThicknessLocations[mode][stepID], mThickness);
  glUniform1f(mOpacityLocations[mode][stepID], mContouringOpacity);
  glUniform4fv(mGraColorALocations[mode][stepID], 1, mGraColorA);
  glUniform4fv(mGraColorBLocations[mode][stepID], 1, mGraColorB);
  glUniformMatrix4fv(mProjMatrixLocations[mode][stepID], 1, false, projectionMatrix.get());
  glUniformMatrix4fv(mViewMatrixLocations[mode][stepID], 1, false, viewMatrix.get());
  glUniformMatrix4fv(mWorldMatrixLocations[mode][stepID], 1, false, scaledPose.get());
  glUniformMatrix4fv(mSkeletonMatrixLocations[mode][stepID], sMaxSupportJointNumbers, false,
                     mSkeletonMatrices);

  mHandModel.mMesh.draw();

  //-------------------- step 3 --------------------
  stepID = ShaderProgramID_FinalFilling;
  targetShader = mShaders[mode][stepID].get();

  glBlendFuncSeparate(
      GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
      GL_ONE, GL_ONE);
  glCullFace(GL_BACK);

  if (targetShader == nullptr) {
    LOGI("target shader nullptr");
    return;
  }

  if (mHandAlpha == nullptr) {
    LOGI("mHandAlpha nullptr");
    return;
  }

  targetShader->useProgram();

  glActiveTexture(GL_TEXTURE0);
  mHandAlpha->bindTexture();
  glUniform1i(mAlphaTexLocations[mode][stepID], 0);
  glUniform1f(mOpacityLocations[mode][stepID], mFillingOpacity);
  glUniform4fv(mGraColorALocations[mode][stepID], 1, mGraColorA);
  glUniform4fv(mGraColorBLocations[mode][stepID], 1, mGraColorB);
  glUniformMatrix4fv(mProjMatrixLocations[mode][stepID], 1, false, projectionMatrix.get());
  glUniformMatrix4fv(mViewMatrixLocations[mode][stepID], 1, false, viewMatrix.get());
  glUniformMatrix4fv(mWorldMatrixLocations[mode][stepID], 1, false, scaledPose.get());
  glUniformMatrix4fv(mSkeletonMatrixLocations[mode][stepID], sMaxSupportJointNumbers, false,
                     mSkeletonMatrices);

  mHandModel.mMesh.draw();

  targetShader->unuseProgram();
  mHandAlpha->unbindTexture();

  glBlendFuncSeparate(
      GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
      GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


  //4. status recovering.
  if (lastPolygonOffsetFill == GL_TRUE) {
    glEnable(GL_POLYGON_OFFSET_FILL);
  } else {
    glDisable(GL_POLYGON_OFFSET_FILL);
  }
  glPolygonOffset(lastFactor, lastUnits);

  if (oldDepth == GL_TRUE) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
  glDepthFunc(oldDepthFunc);

  if (oldAlpha == GL_TRUE) {
    glEnable(GL_BLEND);
  } else {
    glDisable(GL_BLEND);
  }

  if (oldCullingFace == GL_TRUE) {
    glEnable(GL_CULL_FACE);
  } else {
    glDisable(GL_CULL_FACE);
  }

  glColorMask(colorMaskes[0], colorMaskes[1], colorMaskes[2], colorMaskes[3]);
  glDepthMask(depthMask);
}

void HandObj::releaseHandGraphicsResource() {
  mHandModel.releaseGraphicsResource();
  mHandAlpha = nullptr;

}

void HandObj::releaseGraphicsSystem() {
  releaseHandGraphicsResource();
}
