/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Contains implementation of a class VirtualFakeCamera that encapsulates
 * functionality of a fake camera.
 */

#define LOG_NDEBUG 1
#define LOG_TAG "VirtualCamera_FakeCamera"
#include <log/log.h>
#include <cutils/properties.h>
#include "VirtualFakeCamera.h"
#include "VirtualCameraFactory.h"
#include "VirtualFakeCameraDevice.h"
#include "VirtualFakeRotatingCameraDevice.h"

namespace android {

VirtualFakeCamera::VirtualFakeCamera(int cameraId, bool facingBack, struct hw_module_t *module)
    : VirtualCamera(cameraId, module), mFacingBack(facingBack), mFakeCameraDevice(nullptr) {
    const char *key = "ro.kernel.remote.camera.fake.rotating";
    char prop[PROPERTY_VALUE_MAX];
    if (property_get(key, prop, nullptr) > 0) {
        mFakeCameraDevice = new VirtualFakeRotatingCameraDevice(this);
    } else {
        mFakeCameraDevice = new VirtualFakeCameraDevice(this);
    }
}

VirtualFakeCamera::~VirtualFakeCamera() { delete mFakeCameraDevice; }

/****************************************************************************
 * Public API overrides
 ***************************************************************************/

status_t VirtualFakeCamera::Initialize(const char *device_name, const char *frame_dims,
                                       const char *facing_dir) {
    status_t res = mFakeCameraDevice->Initialize(device_name);
    if (res != NO_ERROR) {
        return res;
    }

    const char *facing = mFacingBack ? VirtualCamera::FACING_BACK : VirtualCamera::FACING_FRONT;

    mParameters.set(VirtualCamera::FACING_KEY, facing);
    ALOGE("%s: Fake camera is facing %s", __FUNCTION__, facing);

    mParameters.set(VirtualCamera::ORIENTATION_KEY,
                    gVirtualCameraFactory.getFakeCameraOrientation());
    mParameters.set(CameraParameters::KEY_ROTATION,
                    gVirtualCameraFactory.getFakeCameraOrientation());

    res = VirtualCamera::Initialize(device_name, frame_dims, facing_dir);
    if (res != NO_ERROR) {
        return res;
    }

    /*
     * Parameters provided by the camera device.
     */

    /* 352x288, 320x240 and 176x144 frame dimensions are required by
     * the framework for video mode preview and video recording. */
    mParameters.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, "640x480,352x288,320x240");
    mParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES,
                    "640x480,352x288,320x240,176x144");
    mParameters.set(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES, "640x480,352x288,320x240,176x144");
    mParameters.set(CameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO, "1920x1080");

    mParameters.setPreviewSize(1920, 1080);
    mParameters.setPictureSize(1920, 1080);

    return NO_ERROR;
}

VirtualCameraDevice *VirtualFakeCamera::getCameraDevice() { return mFakeCameraDevice; }

}; /* namespace android */
