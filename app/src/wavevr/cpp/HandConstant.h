#pragma once

enum HandTypeEnum {
    Hand_Left = 0,
    Hand_Right,
    Hand_MaxNumber
};

enum HandBoneEnum
{
    HandBone_Palm = 0,
    HandBone_Wrist,
    HandBone_ThumbJoint0,
    HandBone_ThumbJoint1,
    HandBone_ThumbJoint2,
    HandBone_ThumbJointTip,
    HandBone_IndexJoint0,
    HandBone_IndexJoint1,
    HandBone_IndexJoint2,
    HandBone_IndexJoint3,
    HandBone_IndexJointTip,
    HandBone_MiddleJoint0,
    HandBone_MiddleJoint1,
    HandBone_MiddleJoint2,
    HandBone_MiddleJoint3,
    HandBone_MiddleJointTip,
    HandBone_RingJoint0,
    HandBone_RingJoint1,
    HandBone_RingJoint2,
    HandBone_RingJoint3,
    HandBone_RingJointTip,
    HandBone_PinkyJoint0,
    HandBone_PinkyJoint1,
    HandBone_PinkyJoint2,
    HandBone_PinkyJoint3,
    HandBone_PinkyJointTip,
    HandBone_MaxNumber
};

static const uint32_t sMaxSupportJointNumbers = 48;
static const int32_t sIdentityJoint = sMaxSupportJointNumbers - 1;
