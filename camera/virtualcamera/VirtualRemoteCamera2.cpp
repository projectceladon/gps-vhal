/*
 * Copyright (C) 2012 The Android Open Source Project
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
 * Contains implementation of a class VirtualRemoteCamera2 that encapsulates
 * functionality of a host webcam with further processing to simulate the
 * capabilities of a v2 camera device.
 */

#define LOG_NDEBUG 0
#define LOG_TAG "VirtualCamera_RemoteCamera2"
#include <log/log.h>
#include <cutils/properties.h>
#include "VirtualRemoteCamera2.h"
#include "VirtualCameraFactory.h"

namespace android
{

    VirtualRemoteCamera2::VirtualRemoteCamera2(int cameraId,
                                               bool facingBack,
                                               struct hw_module_t *module)
        : VirtualCamera2(cameraId, module),
          mFacingBack(facingBack)
    {
        ALOGD("Constructing virtual remote camera 2 facing %s",
              facingBack ? "back" : "front");
    }

    VirtualRemoteCamera2::~VirtualRemoteCamera2()
    {
    }

    /****************************************************************************
     * Public API overrides
     ***************************************************************************/

    status_t VirtualRemoteCamera2::Initialize(const char *device_name,
                            const char *frame_dims,
                            const char *facing_dir)
    {
        return NO_ERROR;
    }

}; /* namespace android */
