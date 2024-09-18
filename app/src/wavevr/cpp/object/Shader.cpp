// "WaveVR SDK
// © 2017 HTC Corporation. All Rights Reserved.
//
// Unless otherwise required by copyright law and practice,
// upon the execution of HTC SDK license agreement,
// HTC grants you access to and use of the WaveVR SDK(s).
// You shall fully comply with all of HTC’s SDK license agreement terms and
// conditions signed by you and all SDK and API requirements,
// specifications, and documentation provided by HTC to You."

#define LOG_TAG "Shader"
#include "Shader.h"
#include <vector>
#include "../log.h"
#include "vrb/GLError.h"

Shader::Shader(const char * name, const char * vname, const char * vertex, const char * fname, const char * fragment) :
    mName(name), mVName(vname), mFName(fname), mVertexShader(vertex), mFragmentShader(fragment), mProgramId(0) {
}

Shader::~Shader() {
    if (mProgramId == 0)
        return;
    glDeleteProgram(mProgramId);
    mProgramId = 0;
}

bool Shader::hasShaderError(const char * name, int shaderId) {
    GLint result = GL_FALSE;
    int infoLength = 0;
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infoLength);
        std::vector<GLchar> errorMessage(infoLength + 1);
        glGetShaderInfoLog(shaderId, infoLength, NULL, &errorMessage[0]);
        LOGE("Unable to compile shader \"%s\" (%u):\n%s",
            name, shaderId, &errorMessage[0]);
        return false;
    }
    return true;
}

bool Shader::compile() {
    if (mProgramId != 0)
        return false;

    if (mVertexShader == NULL || mFragmentShader == NULL)
        return false;

    mProgramId = glCreateProgram();

    int vshader = glCreateShader(GL_VERTEX_SHADER);
    VRB_GL_CHECK(glShaderSource(vshader, 1, &mVertexShader, NULL));
    VRB_GL_CHECK(glCompileShader(vshader));
    if (!hasShaderError(mVName, vshader)) {
        VRB_GL_CHECK(glDeleteShader(vshader));
        VRB_GL_CHECK(glDeleteProgram(mProgramId));
        mProgramId = 0;
        return false;
    }
    VRB_GL_CHECK(glAttachShader(mProgramId, vshader));
    VRB_GL_CHECK(glDeleteShader(vshader));

    int fshader = glCreateShader(GL_FRAGMENT_SHADER);
    VRB_GL_CHECK(glShaderSource(fshader, 1, &mFragmentShader, NULL));
    VRB_GL_CHECK(glCompileShader(fshader));
    if (!hasShaderError(mFName, fshader)) {
        VRB_GL_CHECK(glDeleteShader(vshader));
        VRB_GL_CHECK(glDeleteShader(fshader));
        VRB_GL_CHECK(glDeleteProgram(mProgramId));
        mProgramId = 0;
        return false;
    }
    VRB_GL_CHECK(glAttachShader(mProgramId, fshader));
    VRB_GL_CHECK(glDeleteShader(fshader));

    VRB_GL_CHECK(glLinkProgram(mProgramId));
    GLint programSuccess = GL_TRUE;
    VRB_GL_CHECK(glGetProgramiv(mProgramId, GL_LINK_STATUS, &programSuccess));
    if (programSuccess != GL_TRUE) {
        LOGE("%s - Error linking program %d!\n", mName, mProgramId);
        VRB_GL_CHECK(glDeleteProgram( mProgramId ));
        mProgramId = 0;
        return false;
    }

    mVertexShader = NULL;
    mFragmentShader = NULL;
    VRB_GL_CHECK(glUseProgram(mProgramId));
    VRB_GL_CHECK(glUseProgram(0));

    LOGD("%s - Program %d Compiled", mName, mProgramId);
    return true;
}

int Shader::getUniformLocation(const char * name) {
    int location = glGetUniformLocation(mProgramId, name);
    if (location == -1)
        LOGE("Unable to find \"%s\" uniform in \"%s\" shader(%u)", name, mName, mProgramId);
    return location;
}

int Shader::getAttributeLocation(const char * name) {
    int location = glGetAttribLocation(mProgramId, name);
    if (location == -1)
        LOGE("Unable to find \"%s\" attrib in \"%s\" shader(%u)", name, mName, mProgramId);
    return location;
}

std::vector<std::weak_ptr<Shader> > Shader::mShaderPool;

void Shader::putShader(const std::shared_ptr<Shader>& shader) {
    mShaderPool.push_back(shader);
}

std::shared_ptr<Shader> Shader::findShader(const char * vname, const char * fname) {
    const auto N = mShaderPool.size();
    bool needClean = false;
    std::shared_ptr<Shader> ret;
    for (auto i = mShaderPool.rbegin(); i != mShaderPool.rend(); i++) {
        if (i->expired()) {
            needClean = true;
            continue;
        }
        auto shader = i->lock();
        if (0 == strcmp(shader->mVName, vname) &&
            0 == strcmp(shader->mFName, fname) &&
            shader->mProgramId != 0) {
            //LOGD("Found exist shaders \"%s\", \"%s\"", vname, fname);
            ret = shader;
            break;
        }
    }
    if (needClean) {
        // erase can't use reversed iterator
        for (auto i = mShaderPool.begin(); i != mShaderPool.end();) {
            if (i->expired())
                i = mShaderPool.erase(i);
            else
                i++;
        }
    }
    return ret;
}
