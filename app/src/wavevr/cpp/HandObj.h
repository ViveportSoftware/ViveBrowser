#pragma once

#include "DrawEnum.h"
#include "HandModel.h"

enum ShaderProgramIDEnum {
    ShaderProgramID_FinalDepthWriting = 0,
    ShaderProgramID_FinalContouring,
    ShaderProgramID_FinalFilling,
    ShaderProgramID_MaxNumber
};

class HandManager;

class HandObj {
public:
    explicit HandObj(HandManager *iMgr, HandTypeEnum iHandType);

    ~HandObj();

public:
    bool initializeGraphicsSystem();

    void loadModel(WVR_HandModel_t *iHandModel);

    void releaseHandGraphicsResource();

    void releaseGraphicsSystem();

public:
    void setTexture(Texture *iTexture);

    void updateSkeleton(const Matrix4 iSkeletonPoses[sMaxSupportJointNumbers],
                        const Vector3 &iHandScale);

    Vector4 calculateJointWorldPosition(uint32_t jID) const;

    void render(
            const Matrix4 iProj,
            const Matrix4 iEye,
            const Matrix4 &viewMatrix,
            const Matrix4 &iHandPose
    );

protected:
    HandTypeEnum mHandType;
    HandManager *mManager;
protected:
    HandModel mHandModel;
    Texture *mHandAlpha;
protected:
    Vector3 mHandScale;
protected:
    std::shared_ptr<Shader> mShaders[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber];
    int32_t mProjMatrixLocations[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber];
    int32_t mViewMatrixLocations[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber];
    int32_t mWorldMatrixLocations[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber];
    int32_t mNormalMatrixLocations[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber];
    int32_t mSkeletonMatrixLocations[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber];
    int32_t mColorLocations[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber];
    int32_t mAlphaTexLocations[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber];
    int32_t mThicknessLocations[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber];
    int32_t mOpacityLocations[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber];
    int32_t mGraColorALocations[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber];
    int32_t mGraColorBLocations[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber];
protected:
    Matrix4 mWristPose;
    Matrix4 mSkeletonPoses[sMaxSupportJointNumbers]; //tracking data in model space.
    Matrix4 mModelSkeletonPoses[sMaxSupportJointNumbers]; //final skinned input.
    Matrix4 mFinalSkeletonPoses[sMaxSupportJointNumbers]; //final joint mat in model space.
    float mSkeletonMatrices[16 * sMaxSupportJointNumbers];
protected:
    Matrix4 mShift;
protected:
    float mThickness;
    float mContouringOpacity;
    float mFillingOpacity;
    float mGraColorA[4];
    float mGraColorB[4];
};
