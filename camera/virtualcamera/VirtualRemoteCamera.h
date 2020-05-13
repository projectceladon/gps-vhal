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

#ifndef HW_EMULATOR_CAMERA_VIRTUALD_REMOTE_CAMERA_H
#define HW_EMULATOR_CAMERA_VIRTUALD_REMOTE_CAMERA_H

/*
 * Contains declaration of a class VirtualRemoteCamera that encapsulates
 * functionality of an virtual camera connected to the host.
 */

#include "VirtualCamera.h"
#include "VirtualRemoteCameraDevice.h"

namespace android
{

    /* Encapsulates functionality of an virtual camera connected to the host.
    */
    class VirtualRemoteCamera : public VirtualCamera
    {
    public:
        /* Constructs VirtualRemoteCamera instance. */
        VirtualRemoteCamera(int cameraId, struct hw_module_t *module);

        /* Destructs VirtualRemoteCamera instance. */
        ~VirtualRemoteCamera();

        /***************************************************************************
         * VirtualCamera virtual overrides.
         **************************************************************************/

    public:
        /* Initializes VirtualRemoteCamera instance. */
        status_t Initialize(const char *device_name,
                            const char *frame_dims,
                            const char *facing_dir);

        /***************************************************************************
         * VirtualCamera abstract API implementation.
         **************************************************************************/

    protected:
        /* Gets virtual camera device ised by this instance of the virtual camera.
        */
        VirtualCameraDevice *getCameraDevice();

        /***************************************************************************
         * Data memebers.
         **************************************************************************/

    protected:
        /* Contained remote camera device object. */
        VirtualRemoteCameraDevice mRemoteCameraDevice;

        /* Supported frame dimensions reported by the camera device. */
        String8 mFrameDims;
    };

}; /* namespace android */

#endif /* HW_EMULATOR_CAMERA_VIRTUALD_REMOTE_CAMERA_H */
