#pragma once

#include <wvr/wvr_hand_render_model.h>

#include <string>

#include "shared/Matrices.h"
#include "object/Mesh.h"
#include "object/Texture.h"
#include "object/Shader.h"

#include "HandConstant.h"

class HandModel
{
public:
    HandModel();
    ~HandModel();
public:
    void loadModelToGraphicsResource(const WVR_HandModel_t *iModel);
    void releaseGraphicsResource();
public:
    Vector3 getModelJointWorldPosition(uint32_t iJointID) const;
    Vector3 getModelJointLocalPosition(uint32_t iJointID) const;
public:
    //Size same with sMaxSupportJointNumbers for avoiding B.C.
    Matrix4 mJointInvTransMats[sMaxSupportJointNumbers];
    Matrix4 mJointTransMats[sMaxSupportJointNumbers];
    Matrix4 mJointLocalTransMats[sMaxSupportJointNumbers];
    int32_t mJointParentTable[sMaxSupportJointNumbers];
    int32_t mJointUsageTable[sMaxSupportJointNumbers]; //0 means we don't use it.
    bool mInitialized;
    Mesh mMesh;
};
