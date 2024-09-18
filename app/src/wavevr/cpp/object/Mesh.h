#pragma once

#include <string>

enum VertexAttribEnum
{
    VertexAttrib_Vertices = 0,
    VertexAttrib_Normals,
    VertexAttrib_TexCoords,
    VertexAttrib_TexCoord2s,
    VertexAttrib_Color,
    VertexAttrib_BoneID,
    VertexAttrib_BoneWeight,
    VertexAttrib_MaxDefineValue
};

class Mesh
{
public:
    explicit Mesh();
    virtual ~Mesh();
public:
    void setName(const std::string &iName);
    std::string getName() const;
    void createVertexBufferData(VertexAttribEnum iVALocation, float *iData, uint32_t iSize, uint32_t iDimension);
    void createVertexBoneIDData(const uint32_t *iData, uint32_t iSize, uint32_t iDimension);
    void createIndexBufferData(uint32_t *iData, uint32_t iSize, uint32_t iType);
    void createVAO(); //call it after we initialize all vertex buffers.
    void draw();
    void releaseGLComp();
protected:
    uint32_t mVAttribBuffers[VertexAttrib_MaxDefineValue];
    uint32_t mVAttribDimension[VertexAttrib_MaxDefineValue];
    uint32_t mIndicesBuffer;
    uint32_t mFaceType;
    uint32_t mIndiceSize;
    uint32_t mVAOID;
    std::string mName;
};
