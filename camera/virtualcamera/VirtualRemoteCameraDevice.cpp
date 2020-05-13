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
 * Contains implementation of a class VirtualRemoteCameraDevice that encapsulates
 * an virtual camera device connected to the host.
 */

// #define LOG_NDEBUG 0
#define LOG_TAG "VirtualCamera_RemoteDevice"
#include <log/log.h>
#include "VirtualRemoteCamera.h"
#include "VirtualRemoteCameraDevice.h"

namespace android
{

    VirtualRemoteCameraDevice::VirtualRemoteCameraDevice(VirtualRemoteCamera *camera_hal)
        : VirtualCameraDevice(camera_hal),
          mRemoteClient()
    {
    }

    VirtualRemoteCameraDevice::~VirtualRemoteCameraDevice()
    {
    }

    /****************************************************************************
     * Public API
     ***************************************************************************/

    status_t VirtualRemoteCameraDevice::Initialize(const char *device_name)
    {
        /* Connect to the service. */
        char connect_str[256];
        memset(connect_str, 0, sizeof(connect_str));
        snprintf(connect_str, sizeof(connect_str), "name=%s", device_name);
        status_t res = mRemoteClient.connectClient(connect_str);
        if (res != NO_ERROR)
        {
            return res;
        }

        /* Initialize base class. */
        res = VirtualCameraDevice::Initialize();
        if (res == NO_ERROR)
        {
            ALOGV("%s: Connected to the virtual camera service '%s'",
                  __FUNCTION__, device_name);
            mDeviceName = device_name;
        }
        else
        {
            mRemoteClient.queryDisconnect();
        }

        return res;
    }

    /****************************************************************************
     * Virtual camera device abstract interface implementation.
     ***************************************************************************/

    status_t VirtualRemoteCameraDevice::connectDevice()
    {
        ALOGV("%s", __FUNCTION__);

        Mutex::Autolock locker(&mObjectLock);
        if (!isInitialized())
        {
            ALOGE("%s: Remote camera device is not initialized.", __FUNCTION__);
            return EINVAL;
        }
        if (isConnected())
        {
            ALOGW("%s: Remote camera device '%s' is already connected.",
                  __FUNCTION__, (const char *)mDeviceName);
            return NO_ERROR;
        }

        /* Connect to the camera device via emulator. */
        const status_t res = mRemoteClient.queryConnect();
        if (res == NO_ERROR)
        {
            ALOGV("%s: Connected to device '%s'",
                  __FUNCTION__, (const char *)mDeviceName);
            mState = ECDS_CONNECTED;
        }
        else
        {
            ALOGE("%s: Connection to device '%s' failed",
                  __FUNCTION__, (const char *)mDeviceName);
        }

        return res;
    }

    status_t VirtualRemoteCameraDevice::disconnectDevice()
    {
        ALOGV("%s", __FUNCTION__);

        Mutex::Autolock locker(&mObjectLock);
        if (!isConnected())
        {
            ALOGW("%s: Remote camera device '%s' is already disconnected.",
                  __FUNCTION__, (const char *)mDeviceName);
            return NO_ERROR;
        }
        if (isStarted())
        {
            ALOGE("%s: Cannot disconnect from the started device '%s.",
                  __FUNCTION__, (const char *)mDeviceName);
            return EINVAL;
        }

        /* Disconnect from the camera device via emulator. */
        const status_t res = mRemoteClient.queryDisconnect();
        if (res == NO_ERROR)
        {
            ALOGV("%s: Disonnected from device '%s'",
                  __FUNCTION__, (const char *)mDeviceName);
            mState = ECDS_INITIALIZED;
        }
        else
        {
            ALOGE("%s: Disconnection from device '%s' failed",
                  __FUNCTION__, (const char *)mDeviceName);
        }

        return res;
    }

    status_t VirtualRemoteCameraDevice::startDevice(int width,
                                                   int height,
                                                   uint32_t pix_fmt)
    {
        ALOGV("%s", __FUNCTION__);

        Mutex::Autolock locker(&mObjectLock);
        if (!isConnected())
        {
            ALOGE("%s: Remote camera device '%s' is not connected.",
                  __FUNCTION__, (const char *)mDeviceName);
            return EINVAL;
        }
        if (isStarted())
        {
            ALOGW("%s: Remote camera device '%s' is already started.",
                  __FUNCTION__, (const char *)mDeviceName);
            return NO_ERROR;
        }

        status_t res = VirtualCameraDevice::commonStartDevice(width, height, pix_fmt);
        if (res != NO_ERROR)
        {
            ALOGE("%s: commonStartDevice failed", __FUNCTION__);
            return res;
        }

        /* Allocate preview frame buffer. */
        /* TODO: Watch out for preview format changes! At this point we implement
        * RGB32 only.*/
        mPreviewFrames[0].resize(mTotalPixels);
        mPreviewFrames[1].resize(mTotalPixels);

        mFrameBufferPairs[0].first = mFrameBuffers[0].data();
        mFrameBufferPairs[0].second = mPreviewFrames[0].data();

        mFrameBufferPairs[1].first = mFrameBuffers[1].data();
        mFrameBufferPairs[1].second = mPreviewFrames[1].data();

        /* Start the actual camera device. */
        res = mRemoteClient.queryStart(mPixelFormat, mFrameWidth, mFrameHeight);
        if (res == NO_ERROR)
        {
            ALOGV("%s: Remote camera device '%s' is started for %.4s[%dx%d] frames",
                  __FUNCTION__, (const char *)mDeviceName,
                  reinterpret_cast<const char *>(&mPixelFormat),
                  mFrameWidth, mFrameHeight);
            mState = ECDS_STARTED;
        }
        else
        {
            ALOGE("%s: Unable to start device '%s' for %.4s[%dx%d] frames",
                  __FUNCTION__, (const char *)mDeviceName,
                  reinterpret_cast<const char *>(&pix_fmt), width, height);
        }

        return res;
    }

    status_t VirtualRemoteCameraDevice::stopDevice()
    {
        ALOGV("%s", __FUNCTION__);

        Mutex::Autolock locker(&mObjectLock);
        if (!isStarted())
        {
            ALOGW("%s: Remote camera device '%s' is not started.",
                  __FUNCTION__, (const char *)mDeviceName);
            return NO_ERROR;
        }

        /* Stop the actual camera device. */
        status_t res = mRemoteClient.queryStop();
        if (res == NO_ERROR)
        {
            mPreviewFrames[0].clear();
            mPreviewFrames[1].clear();
            // No need to keep all that memory around as capacity, shrink it
            mPreviewFrames[0].shrink_to_fit();
            mPreviewFrames[1].shrink_to_fit();

            VirtualCameraDevice::commonStopDevice();
            mState = ECDS_CONNECTED;
            ALOGV("%s: Remote camera device '%s' is stopped",
                  __FUNCTION__, (const char *)mDeviceName);
        }
        else
        {
            ALOGE("%s: Unable to stop device '%s'",
                  __FUNCTION__, (const char *)mDeviceName);
        }

        return res;
    }

    /****************************************************************************
     * VirtualCameraDevice virtual overrides
     ***************************************************************************/

    status_t VirtualRemoteCameraDevice::getCurrentFrame(void *buffer,
                                                       uint32_t pixelFormat,
                                                       int64_t *timestamp)
    {
        if (!isStarted())
        {
            ALOGE("%s: Device is not started", __FUNCTION__);
            return EINVAL;
        }
        if (buffer == nullptr)
        {
            ALOGE("%s: Invalid buffer provided", __FUNCTION__);
            return EINVAL;
        }

        FrameLock lock(*this);
        const void *primary = mCameraThread->getPrimaryBuffer();
        auto frameBufferPair = reinterpret_cast<const FrameBufferPair *>(primary);
        uint8_t *frame = frameBufferPair->first;

        if (frame == nullptr)
        {
            ALOGE("%s: No frame", __FUNCTION__);
            return EINVAL;
        }

        if (timestamp != nullptr)
        {
            *timestamp = mCameraThread->getPrimaryTimestamp();
        }

        return getCurrentFrameImpl(reinterpret_cast<const uint8_t *>(frame),
                                   reinterpret_cast<uint8_t *>(buffer),
                                   pixelFormat);
    }

    status_t VirtualRemoteCameraDevice::getCurrentPreviewFrame(void *buffer,
                                                              int64_t *timestamp)
    {
        if (!isStarted())
        {
            ALOGE("%s: Device is not started", __FUNCTION__);
            return EINVAL;
        }
        if (buffer == nullptr)
        {
            ALOGE("%s: Invalid buffer provided", __FUNCTION__);
            return EINVAL;
        }

        FrameLock lock(*this);
        const void *primary = mCameraThread->getPrimaryBuffer();
        auto frameBufferPair = reinterpret_cast<const FrameBufferPair *>(primary);
        uint32_t *previewFrame = frameBufferPair->second;

        if (previewFrame == nullptr)
        {
            ALOGE("%s: No frame", __FUNCTION__);
            return EINVAL;
        }
        if (timestamp != nullptr)
        {
            *timestamp = mCameraThread->getPrimaryTimestamp();
        }
        memcpy(buffer, previewFrame, mTotalPixels * 4);
        return NO_ERROR;
    }

    const void *VirtualRemoteCameraDevice::getCurrentFrame()
    {
        if (mCameraThread.get() == nullptr)
        {
            return nullptr;
        }

        const void *primary = mCameraThread->getPrimaryBuffer();
        auto frameBufferPair = reinterpret_cast<const FrameBufferPair *>(primary);
        uint8_t *frame = frameBufferPair->first;

        return frame;
    }

    /****************************************************************************
     * Worker thread management overrides.
     ***************************************************************************/

    bool VirtualRemoteCameraDevice::produceFrame(void *buffer, int64_t *timestamp)
    {
        auto frameBufferPair = reinterpret_cast<FrameBufferPair *>(buffer);
        uint8_t *rawFrame = frameBufferPair->first;
        uint32_t *previewFrame = frameBufferPair->second;

        status_t query_res = mRemoteClient.queryFrame(rawFrame, previewFrame,
                                                    mFrameBufferSize,
                                                    mTotalPixels * 4,
                                                    mWhiteBalanceScale[0],
                                                    mWhiteBalanceScale[1],
                                                    mWhiteBalanceScale[2],
                                                    mExposureCompensation,
                                                    timestamp);
        if (query_res != NO_ERROR)
        {
            ALOGE("%s: Unable to get current video frame: %s",
                  __FUNCTION__, strerror(query_res));
            return false;
        }
        return true;
    }

    void *VirtualRemoteCameraDevice::getPrimaryBuffer()
    {
        return &mFrameBufferPairs[0];
    }
    void *VirtualRemoteCameraDevice::getSecondaryBuffer()
    {
        return &mFrameBufferPairs[1];
    }

}; /* namespace android */
