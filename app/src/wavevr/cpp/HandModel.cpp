#define LOG_TAG "APHandModel"

#include "log.h"

#include "HandModel.h"

void PrintMatrix4(const Matrix4 &iMat)
{
    LOGI("|%3.6lf, %3.6lf, %3.6lf, %3.6lf|",  iMat[ 0], iMat[ 1], iMat[ 2], iMat[ 3]);
    LOGI("|%3.6lf, %3.6lf, %3.6lf, %3.6lf|",  iMat[ 4], iMat[ 5], iMat[ 6], iMat[ 7]);
    LOGI("|%3.6lf, %3.6lf, %3.6lf, %3.6lf|",  iMat[ 8], iMat[ 9], iMat[10], iMat[11]);
    LOGI("|%3.6lf, %3.6lf, %3.6lf, %3.6lf|",  iMat[12], iMat[13], iMat[14], iMat[15]);
}

//----------------- Hand Model ------------------------------
HandModel::HandModel()
: mInitialized(false)
{
}

HandModel::~HandModel()
{
}

void HandModel::loadModelToGraphicsResource(const WVR_HandModel_t *iModel)
{
    for (uint32_t jointID = 0; jointID < sMaxSupportJointNumbers; ++jointID) {
        float *mat;

        mat = ((float*)(*iModel).jointInvTransMats + jointID * 16);
        mJointInvTransMats[jointID].set(mat);
        mat = ((float*)(*iModel).jointTransMats + jointID * 16);
        mJointTransMats[jointID].set(mat);
        mat = ((float*)(*iModel).jointLocalTransMats + jointID * 16);
        mJointLocalTransMats[jointID].set(mat);

        mJointParentTable[jointID] = (*iModel).jointParentTable[jointID];
        mJointUsageTable[jointID]  = (*iModel).jointUsageTable[jointID];

#if defined(HAND_DEBUG_LOG)
        if (mJointUsageTable[jointID] == 1) {
            LOGI("------ Joint[%d] ------", jointID);
            LOGI("Node[%d]", jointID);
            PrintMatrix4(mJointTransMats[jointID]);
            LOGI("Bone[%d]", jointID);
            PrintMatrix4(mJointInvTransMats[jointID]);
            LOGI("Node*Bone[%d]", jointID);
            PrintMatrix4(mJointTransMats[jointID] * mJointInvTransMats[jointID]);
        } else {
            LOGI("------ Joint[%d] no use ------", jointID);
        }
#endif
    }

    for (uint32_t jointID = 0; jointID < sMaxSupportJointNumbers; ++jointID) {
        if (mJointUsageTable[jointID] == 1) {
            uint32_t pID = 0;
            uint32_t parentID = mJointParentTable[jointID];
            if (parentID != sIdentityJoint) {
                Vector3 lp = getModelJointLocalPosition(jointID);
                Vector3 wp = getModelJointWorldPosition(jointID);
                Vector3 pwp = getModelJointWorldPosition(parentID);
                Vector3 localDir = mJointInvTransMats[jointID] * (pwp - wp);
                Vector3 localDirNormal = localDir;
                localDirNormal.normalize();
                LOGI("Joint Local Axis Info : (%d:p(%d)) is LocalP:(%lf, %lf, %lf) LocalD:(%lf, %lf, %lf)[(%lf, %lf, %lf)]",
                jointID, mJointParentTable[jointID],
                lp.x, lp.y, lp.z,
                localDir.x, localDir.y, localDir.z,
                localDirNormal.x, localDirNormal.y, localDirNormal.z);
            } else {
                Vector3 lp = getModelJointLocalPosition(jointID);
                LOGI("Joint Local Axis Info : (%d:p(%d)) P:(%lf, %lf, %lf) Dir: (0,0,-1)",
                jointID, mJointParentTable[jointID],
                lp.x, lp.y, lp.z);
            }
        }
    }


    //glPart.
    mMesh.createVertexBufferData(VertexAttrib_Vertices, (*iModel).vertices.buffer, (*iModel).vertices.size, (*iModel).vertices.dimension);
    mMesh.createVertexBufferData(VertexAttrib_Normals, (*iModel).normals.buffer, (*iModel).normals.size, (*iModel).normals.dimension);
    mMesh.createVertexBufferData(VertexAttrib_TexCoords, (*iModel).texCoords.buffer, (*iModel).texCoords.size, (*iModel).texCoords.dimension);
    mMesh.createVertexBufferData(VertexAttrib_TexCoord2s, (*iModel).texCoord2s.buffer, (*iModel).texCoord2s.size, (*iModel).texCoord2s.dimension);
    mMesh.createVertexBufferData(VertexAttrib_BoneWeight, (*iModel).boneWeights.buffer, (*iModel).boneWeights.size, (*iModel).boneWeights.dimension);
    mMesh.createVertexBoneIDData((*iModel).boneIDs.buffer, (*iModel).boneIDs.size, (*iModel).boneIDs.dimension);
    mMesh.createIndexBufferData((*iModel).indices.buffer, (*iModel).indices.size, (*iModel).indices.type);
    mInitialized = true;
}

void HandModel::releaseGraphicsResource()
{
    mInitialized = false;
    mMesh.releaseGLComp();
}

Vector3 HandModel::getModelJointWorldPosition(uint32_t iJointID) const
{
    Vector3 result;
    if (iJointID < sMaxSupportJointNumbers) {
        result = Vector3(
            mJointTransMats[iJointID][12],
            mJointTransMats[iJointID][13],
            mJointTransMats[iJointID][14]);
    }
    return result;
}

Vector3 HandModel::getModelJointLocalPosition(uint32_t iJointID) const
{
    Vector3 result;
    if (iJointID < sMaxSupportJointNumbers) {
        result = Vector3(
            mJointLocalTransMats[iJointID][12],
            mJointLocalTransMats[iJointID][13],
            mJointLocalTransMats[iJointID][14]);
    }
    return result;
}

