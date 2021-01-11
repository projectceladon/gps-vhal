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

#ifndef HW_EMULATOR_CAMERA_VIRTUALD_FAKE_CAMERA_H
#define HW_EMULATOR_CAMERA_VIRTUALD_FAKE_CAMERA_H

/*
 * Contains declaration of a class VirtualFakeCamera that encapsulates
 * functionality of a fake camera. This class is nothing more than a placeholder
 * for VirtualFakeCameraDevice instance.
 */

#include "VirtualCamera.h"

namespace android {

/* Encapsulates functionality of a fake camera.
 * This class is nothing more than a placeholder for VirtualFakeCameraDevice
 * instance that emulates a fake camera device.
 */
class VirtualFakeCamera : public VirtualCamera {
public:
    /* Constructs VirtualFakeCamera instance. */
    VirtualFakeCamera(int cameraId, bool facingBack, struct hw_module_t *module);

    /* Destructs VirtualFakeCamera instance. */
    ~VirtualFakeCamera();

    /****************************************************************************
     * VirtualCamera virtual overrides.
     ***************************************************************************/

public:
    /* Initializes VirtualFakeCamera instance. */
    status_t Initialize(const char *device_name, const char *frame_dims, const char *facing_dir);

    /****************************************************************************
     * VirtualCamera abstract API implementation.
     ***************************************************************************/

protected:
    /* Gets virtual camera device ised by this instance of the virtual camera.
     */
    VirtualCameraDevice *getCameraDevice();

    /****************************************************************************
     * Data memebers.
     ***************************************************************************/

protected:
    /* Facing back (true) or front (false) switch. */
    bool mFacingBack;

    /* Contained fake camera device object. */
    VirtualCameraDevice *mFakeCameraDevice;
};

}; /* namespace android */

#endif /* HW_EMULATOR_CAMERA_VIRTUALD_FAKE_CAMERA_H */
