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
 * Contains implementation of a class VirtualCameraFactory that manages cameras
 * available for emulation.
 */

#define LOG_NDEBUG 0
#define LOG_TAG "VirtualCamera_Factory"

#include "VirtualCameraFactory.h"
#include "VirtualCameraHotplugThread.h"
#include "VirtualFakeCamera.h"
#include "VirtualFakeCamera2.h"
#include "VirtualFakeCamera3.h"
#include "VirtualRemoteCamera.h"
#include "VirtualRemoteCamera3.h"
#include "CameraSocketServerThread.h"

#include <log/log.h>
#include <cutils/properties.h>

extern camera_module_t HAL_MODULE_INFO_SYM;

/*
 * A global instance of VirtualCameraFactory is statically instantiated and
 * initialized when camera emulation HAL is loaded.
 */
android::VirtualCameraFactory gVirtualCameraFactory;

namespace android
{

    VirtualCameraFactory::VirtualCameraFactory() : mRemoteClient(),
                                                   mVirtualCameras(nullptr),
                                                   mVirtualCameraNum(0),
                                                   mFakeCameraNum(0),
                                                   mConstructedOK(false),
                                                   mCallbacks(nullptr)
    {

        /*
        * Figure out how many cameras need to be created, so we can allocate the
        * array of virtual cameras before populating it.
        */
        int virtualCamerasSize = 0;

        virtualCamerasSize = 2; //Reserve two remote cameras

        waitForRemoteSfFakeCameraPropertyAvailable();
        // Fake Cameras
        if (isFakeCameraEmulationOn(/* backCamera */ true))
        {
            mFakeCameraNum++;
        }
        if (isFakeCameraEmulationOn(/* backCamera */ false))
        {
            mFakeCameraNum++;
        }
        virtualCamerasSize += mFakeCameraNum;

        /*
        * We have the number of cameras we need to create, now allocate space for
        * them.
        */
        mVirtualCameras = new VirtualBaseCamera *[virtualCamerasSize];
        if (mVirtualCameras == nullptr)
        {
            ALOGE("%s: Unable to allocate virtual camera array for %d entries",
                  __FUNCTION__, mVirtualCameraNum);
            return;
        }
        if (mVirtualCameras != nullptr)
        {
            for (int n = 0; n < virtualCamerasSize; n++)
            {
                mVirtualCameras[n] = nullptr;
            }
        }

        // Create fake cameras, if enabled.
        if (isFakeCameraEmulationOn(/* backCamera */ true))
        {
            createFakeCamera(/* backCamera */ true);
        }
        if (isFakeCameraEmulationOn(/* backCamera */ false))
        {
            createFakeCamera(/* backCamera */ false);
        }

        ALOGE("%d cameras are being virtual. %d of them are fake cameras.",
              mVirtualCameraNum, mFakeCameraNum);

        // Create hotplug thread.
        {
            Vector<int> cameraIdVector;
            for (int i = 0; i < mVirtualCameraNum; ++i)
            {
                cameraIdVector.push_back(i);
            }
            mHotplugThread = new VirtualCameraHotplugThread(&cameraIdVector[0],
                                                            mVirtualCameraNum);
            mHotplugThread->run("VirtualCameraHotplugThread");
        }

        createSocketServer();

        mConstructedOK = true;
    }

    VirtualCameraFactory::~VirtualCameraFactory()
    {
        if (mVirtualCameras != nullptr)
        {
            for (int n = 0; n < mVirtualCameraNum; n++)
            {
                if (mVirtualCameras[n] != nullptr)
                {
                    delete mVirtualCameras[n];
                }
            }
            delete[] mVirtualCameras;
        }

        if (mHotplugThread != nullptr)
        {
            mHotplugThread->requestExit();
            mHotplugThread->join();
        }

        if (mRemoteClient.mCameraSocketFD > 0)
        {
            ALOGV("%s Close camera client fd(mRemoteClient.mCameraSocketFD=%d)", __FUNCTION__, mRemoteClient.mCameraSocketFD);
            close(mRemoteClient.mCameraSocketFD);
            mRemoteClient.mCameraSocketFD = -1;
        }

        if (mCSST != nullptr)
        {
            mCSST->requestExit();
            mCSST->join();
        }
    }

    bool VirtualCameraFactory::createSocketServer()
    {
        ALOGV("%s: Start to create socket server.", __FUNCTION__);

        char buf[PROPERTY_VALUE_MAX] = {
            '\0',
        };
        int containerId = 0;
        if (property_get("ro.boot.container.id", buf, "") > 0)
        {
            containerId = atoi(buf);
        }
        mCSST = new CameraSocketServerThread(containerId, gVirtualCameraFactory);
        mCSST->run("BackVirtualCameraSocketServerThread");
        ALOGV("%s: Finish to create socket server.", __FUNCTION__);
        return true;
    }

    void VirtualCameraFactory::cameraClientConnect(int socketFd)
    {
        ALOGV("%s socketFd = %d", __FUNCTION__, socketFd);
        std::vector<RemoteCameraInfo> remoteCameras;

        mRemoteClient.setCameraFD(socketFd);
        findRemoteCameras(&remoteCameras);

        createRemoteCameras(remoteCameras);

        //Set remote camera socket fd.
        if (mVirtualCameras != nullptr)
        {
            for (int n = 2; n < mVirtualCameraNum; n++)
            {
                if (mVirtualCameras[n] != nullptr)
                {
                    mVirtualCameras[n]->setCameraFD(socketFd);
                }
            }
        }
    }

    void VirtualCameraFactory::cameraClientDisconnect(int socketFd)
    {
        ALOGV("%s socketFd = %d", __FUNCTION__, socketFd);
        mRemoteClient.cleanCameraFD(socketFd);

        //Clean remote camera socket fd.
        int removeCameraCount = 0;
        if (mVirtualCameras != nullptr)
        {
            for (int n = 2; n < mVirtualCameraNum; n++)
            {
                if (mVirtualCameras[n] != nullptr)
                {
                    mVirtualCameras[n]->setCameraFD(socketFd);
                    /* delete mVirtualCameras[n];
                    mVirtualCameras[n] = nullptr; */
                    removeCameraCount++;
                }
            }
        }
        // mVirtualCameraNum -= removeCameraCount;
    }

    int VirtualCameraFactory::trySwitchRemoteCamera(int curCameraId)
    {
        // 0: Fake back camera  <--> 2: Remote back camera
        // 1: Fake front camera <--> 3: Remote front camera
        int cameraId = curCameraId;
        if (mRemoteClient.mCameraSocketFD > 0)
        {
            switch (curCameraId)
            {
            case 0:
                /* code */
                if (mVirtualCameraNum >= 3 && mVirtualCameras[2] != nullptr)
                {
                    cameraId = 2;
                    ALOGV("%s map curCameraId(%d) to cameraId(%d)", __FUNCTION__, curCameraId, cameraId);
                }
                break;
            case 1:
                /* code */
                if (mVirtualCameraNum >= 4 && mVirtualCameras[3] != nullptr)
                {
                    cameraId = 3;
                    ALOGV("%s map curCameraId(%d) to cameraId(%d)", __FUNCTION__, curCameraId, cameraId);
                }
                break;

            default:
                //ALOGV("%s map curCameraIdis %d. No map.", __FUNCTION__);
                break;
            }
        }
        else
        {
            ALOGV("%s Remote cmaera is not connected. Do not switch to remote camera. .", __FUNCTION__);
        }

        return cameraId;
    }

    /******************************************************************************
     * Camera HAL API handlers.
     *
     * Each handler simply verifies existence of an appropriate VirtualBaseCamera
     * instance, and dispatches the call to that instance.
     *
     *****************************************************************************/

    int VirtualCameraFactory::cameraDeviceOpen(int cameraId,
                                               hw_device_t **device)
    {
        ALOGV("%s: id = %d", __FUNCTION__, cameraId);

        *device = nullptr;

        if (!isConstructedOK())
        {
            ALOGE("%s: VirtualCameraFactory has failed to initialize",
                  __FUNCTION__);
            return -EINVAL;
        }

        if (cameraId < 0 || cameraId >= getVirtualCameraNum())
        {
            ALOGE("%s: Camera id %d is out of bounds (%d)",
                  __FUNCTION__, cameraId, getVirtualCameraNum());
            return -ENODEV;
        }
        cameraId = trySwitchRemoteCamera(cameraId);

	//Save global cameraId
	gVirtualCameraFactory.setmCameraId(cameraId);
        return mVirtualCameras[cameraId]->connectCamera(device);
    }

    int VirtualCameraFactory::getCameraInfo(int cameraId,
                                            struct camera_info *info)
    {
        ALOGV("%s: id = %d", __FUNCTION__, cameraId);

        if (!isConstructedOK())
        {
            ALOGE("%s: VirtualCameraFactory has failed to initialize",
                  __FUNCTION__);
            return -EINVAL;
        }

        if (cameraId < 0 || cameraId >= getVirtualCameraNum())
        {
            ALOGE("%s: Camera id %d is out of bounds (%d)",
                  __FUNCTION__, cameraId, getVirtualCameraNum());
            return -ENODEV;
        }
        cameraId = trySwitchRemoteCamera(cameraId);

        return mVirtualCameras[cameraId]->getCameraInfo(info);
    }

    int VirtualCameraFactory::setCallbacks(
        const camera_module_callbacks_t *callbacks)
    {
        ALOGV("%s: callbacks = %p", __FUNCTION__, callbacks);

        mCallbacks = callbacks;

        return OK;
    }

    void VirtualCameraFactory::getVendorTagOps(vendor_tag_ops_t *ops)
    {
        ALOGV("%s: ops = %p", __FUNCTION__, ops);
        // No vendor tags defined for emulator yet, so not touching ops.
    }

    /****************************************************************************
     * Camera HAL API callbacks.
     ***************************************************************************/

    int VirtualCameraFactory::device_open(const hw_module_t *module, const char *name, hw_device_t **device)
    {
        /*
        * Simply verify the parameters, and dispatch the call inside the
        * VirtualCameraFactory instance.
        */

        if (module != &HAL_MODULE_INFO_SYM.common)
        {
            ALOGE("%s: Invalid module %p expected %p",
                  __FUNCTION__, module, &HAL_MODULE_INFO_SYM.common);
            return -EINVAL;
        }
        if (name == nullptr)
        {
            ALOGE("%s: NULL name is not expected here", __FUNCTION__);
            return -EINVAL;
        }

        return gVirtualCameraFactory.cameraDeviceOpen(atoi(name), device);
    }

    int VirtualCameraFactory::get_number_of_cameras()
    {
        return gVirtualCameraFactory.getVirtualCameraNum();
    }

    int VirtualCameraFactory::get_camera_info(int camera_id,
                                              struct camera_info *info)
    {
        return gVirtualCameraFactory.getCameraInfo(camera_id, info);
    }

    int VirtualCameraFactory::set_callbacks(
        const camera_module_callbacks_t *callbacks)
    {
        return gVirtualCameraFactory.setCallbacks(callbacks);
    }

    void VirtualCameraFactory::get_vendor_tag_ops(vendor_tag_ops_t *ops)
    {
        gVirtualCameraFactory.getVendorTagOps(ops);
    }

    int VirtualCameraFactory::open_legacy(const struct hw_module_t *module,
                                          const char *id, uint32_t halVersion, struct hw_device_t **device)
    {
        // Not supporting legacy open.
        return -ENOSYS;
    }

    /********************************************************************************
     * Internal API
     *******************************************************************************/

    /*
    * Camera information tokens passed in response to the "list" factory query.
    */

    // Device name token.
    static const char *kListNameToken = "name=";
    // Frame dimensions token.
    static const char *kListDimsToken = "framedims=";
    // Facing direction token.
    static const char *kListDirToken = "dir=";

    bool VirtualCameraFactory::getTokenValue(const char *token,
                                             const std::string &s, char **value)
    {
        // Find the start of the token.
        size_t tokenStart = s.find(token);
        if (tokenStart == std::string::npos)
        {
            return false;
        }

        // Advance to the beginning of the token value.
        size_t valueStart = tokenStart + strlen(token);

        // Find the length of the token value.
        size_t valueLength = s.find(' ', valueStart) - valueStart;

        // Extract the value substring.
        std::string valueStr = s.substr(valueStart, valueLength);

        // Convert to char*.
        *value = new char[valueStr.length() + 1];
        if (*value == nullptr)
        {
            return false;
        }
        strcpy(*value, valueStr.c_str());

        ALOGV("%s: Parsed value is \"%s\"", __FUNCTION__, *value);

        return true;
    }

    void VirtualCameraFactory::findRemoteCameras(
        std::vector<RemoteCameraInfo> *remoteCameras)
    {
	ALOGV("%s", __func__);
        // Obtain camera list.
        char *cameraList = nullptr;
        status_t res = mRemoteClient.listCameras(&cameraList);

        /*
        * Empty list, or list containing just an EOL means that there were no
        * connected cameras found.
        */
        if (res != NO_ERROR || cameraList == nullptr || *cameraList == '\0' ||
            *cameraList == '\n')
        {
            if (cameraList != nullptr)
            {
                free(cameraList);
            }
            return;
        }

        /*
        * Calculate number of connected cameras. Number of EOLs in the camera list
        * is the number of the connected cameras.
        */

        std::string cameraListStr(cameraList);
        free(cameraList);

        size_t lineBegin = 0;
        size_t lineEnd = cameraListStr.find('\n');
        while (lineEnd != std::string::npos)
        {
            std::string cameraStr = cameraListStr.substr(lineBegin, lineEnd - lineBegin);
            // Parse the 'name', 'framedims', and 'dir' tokens.
            char *name, *frameDims, *dir;
            if (getTokenValue(kListNameToken, cameraStr, &name) &&
                getTokenValue(kListDimsToken, cameraStr, &frameDims) &&
                getTokenValue(kListDirToken, cameraStr, &dir))
            {
                // Push the camera info if it was all successfully parsed.
                remoteCameras->push_back(RemoteCameraInfo{
                    .name = name,
                    .frameDims = frameDims,
                    .dir = dir,
                });
            }
            else
            {
                ALOGW("%s: Bad camera information: %s", __FUNCTION__,
                      cameraStr.c_str());
            }
            // Skip over the newline for the beginning of the next line.
            lineBegin = lineEnd + 1;
            lineEnd = cameraListStr.find('\n', lineBegin);
        }
    }

    void VirtualCameraFactory::createRemoteCameras(
        const std::vector<RemoteCameraInfo> &remoteCameras)
    {
        /*
        * Iterate the list, creating, and initializing virtual REMOTE cameras for each
        * entry in the list.
        */

        /*
        * We use this index only for determining which direction the webcam should
        * face. Otherwise, mVirtualCameraNum represents the camera ID and the
        * index into mVirtualCameras.
        */
        int remoteIndex = 0;
        for (const auto &cameraInfo : remoteCameras)
        {
            /*
            * Here, we're assuming the first webcam is intended to be the back
            * camera and any other webcams are front cameras.
            */
            int halVersion = 0;
            if (remoteIndex == 0)
            {
                halVersion = getCameraHalVersion(/* backCamera */ true);
            }
            else
            {
                halVersion = getCameraHalVersion(/* backCamera */ false);
            }

            // Create and initialize REMOTE camera.
            VirtualBaseCamera *remoteCam = nullptr;
            status_t res;
            switch (halVersion)
            {
            case 1:
                VirtualRemoteCamera *remoteCamOne;
                remoteCamOne = new VirtualRemoteCamera(
                    mVirtualCameraNum, &HAL_MODULE_INFO_SYM.common);
                if (remoteCamOne == nullptr)
                {
                    ALOGE("%s: Unable to instantiate VirtualRemoteCamera",
                          __FUNCTION__);
                }
                else
                {
                    /*
                     * We have to initialize in each switch case, because
                     * VirtualBaseCamera::Initialize has a different method
                     * signature.
                     *
                     * TODO: Having an VirtualBaseRemoteCamera class
                     * could fix this issue.
                     */
                    res = remoteCamOne->Initialize(
                        cameraInfo.name,
                        cameraInfo.frameDims,
                        cameraInfo.dir);
                }
                remoteCam = remoteCamOne;
                break;
            case 2:
                ALOGE("%s: REMOTE support for camera hal version %d is not "
                      "implemented",
                      __FUNCTION__, halVersion);
                break;
            case 3:
                VirtualRemoteCamera3 *remoteCamThree;
                remoteCamThree = new VirtualRemoteCamera3(
                    mVirtualCameraNum, &HAL_MODULE_INFO_SYM.common);
                if (remoteCamThree == nullptr)
                {
                    ALOGE("%s: Unable to instantiate VirtualRemoteCamera3",
                          __FUNCTION__);
                }
                else
                {
                    res = remoteCamThree->Initialize(
                        cameraInfo.name,
                        cameraInfo.frameDims,
                        cameraInfo.dir);
                }
                remoteCam = remoteCamThree;
                break;
            default:
                ALOGE("%s: Unknown camera hal version requested: %d",
                      __FUNCTION__, halVersion);
            }

            if (remoteCam == nullptr)
            {
                ALOGE("%s: Unable to instantiate VirtualRemoteCamera",
                      __FUNCTION__);
            }
            else
            {
                if (res == NO_ERROR)
                {
                    mVirtualCameras[mVirtualCameraNum] = remoteCam;
                    remoteIndex++;
                    mVirtualCameraNum++;
                }
                else
                {
                    delete remoteCam;
                }
            }
        }
    }

    void VirtualCameraFactory::createFakeCamera(bool backCamera)
    {
        int halVersion = getCameraHalVersion(backCamera);

        /*
        * Create and initialize the fake camera, using the index into
        * mVirtualCameras as the camera ID.
        */
        switch (halVersion)
        {
        case 1:
            mVirtualCameras[mVirtualCameraNum] =
                new VirtualFakeCamera(mVirtualCameraNum, backCamera,
                                      &HAL_MODULE_INFO_SYM.common);
            break;
        case 2:
            mVirtualCameras[mVirtualCameraNum] =
                new VirtualFakeCamera2(mVirtualCameraNum, backCamera,
                                       &HAL_MODULE_INFO_SYM.common);
            break;
        case 3:
        {
            const char *key = "ro.kernel.remote.camera.fake.rotating";
            char prop[PROPERTY_VALUE_MAX];
            if (property_get(key, prop, nullptr) > 0)
            {
                mVirtualCameras[mVirtualCameraNum] =
                    new VirtualFakeCamera(mVirtualCameraNum, backCamera,
                                          &HAL_MODULE_INFO_SYM.common);
            }
            else
            {
                mVirtualCameras[mVirtualCameraNum] =
                    new VirtualFakeCamera3(mVirtualCameraNum, backCamera,
                                           &HAL_MODULE_INFO_SYM.common);
            }
        }
        break;
        default:
            ALOGE("%s: Unknown %s camera hal version requested: %d",
                  __FUNCTION__, backCamera ? "back" : "front", halVersion);
        }

        if (mVirtualCameras[mVirtualCameraNum] == nullptr)
        {
            ALOGE("%s: Unable to instantiate fake camera class", __FUNCTION__);
        }
        else
        {
            ALOGV("%s: %s camera device version is %d", __FUNCTION__,
                  backCamera ? "Back" : "Front", halVersion);
            status_t res = mVirtualCameras[mVirtualCameraNum]->Initialize( nullptr, nullptr, nullptr);
            if (res == NO_ERROR)
            {
                // Camera creation and initialization was successful.
                mVirtualCameraNum++;
            }
            else
            {
                ALOGE("%s: Unable to initialize %s camera %d: %s (%d)",
                      __FUNCTION__, backCamera ? "back" : "front",
                      mVirtualCameraNum, strerror(-res), res);
                delete mVirtualCameras[mVirtualCameraNum];
            }
        }
    }

    void VirtualCameraFactory::waitForRemoteSfFakeCameraPropertyAvailable()
    {
        /*
        * Camera service may start running before remote-props sets
        * remote.sf.fake_camera to any of the follwing four values:
        * "none,front,back,both"; so we need to wait.
        *
        * android/camera/camera-service.c
        * bug: 30768229
        */
        int numAttempts = 100;
        char prop[PROPERTY_VALUE_MAX];
        bool timeout = true;
        for (int i = 0; i < numAttempts; ++i)
        {
            if (property_get("remote.sf.fake_camera", prop, nullptr) != 0)
            {
                timeout = false;
                break;
            }
            usleep(5000);
        }
        if (timeout)
        {
            ALOGE("timeout (%dms) waiting for property remote.sf.fake_camera to be set\n", 5 * numAttempts);
        }
    }

    bool VirtualCameraFactory::isFakeCameraEmulationOn(bool backCamera)
    {
        /*
        * Defined by 'remote.sf.fake_camera' boot property. If the property exists,
        * and if it's set to 'both', then fake cameras are used to emulate both
        * sides. If it's set to 'back' or 'front', then a fake camera is used only
        * to emulate the back or front camera, respectively.
        */
        char prop[PROPERTY_VALUE_MAX];
        if ((property_get("remote.sf.fake_camera", prop, nullptr) > 0) &&
            (!strcmp(prop, "both") ||
             !strcmp(prop, backCamera ? "back" : "front")))
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    int VirtualCameraFactory::getCameraHalVersion(bool backCamera)
    {
        /*
        * Defined by 'remote.sf.front_camera_hal_version' and
        * 'remote.sf.back_camera_hal_version' boot properties. If the property
        * doesn't exist, it is assumed we are working with HAL v1.
        */
        char prop[PROPERTY_VALUE_MAX];
        const char *propQuery = backCamera ? "remote.sf.back_camera_hal" : "remote.sf.front_camera_hal";
        if (property_get(propQuery, prop, nullptr) > 0)
        {
            char *propEnd = prop;
            int val = strtol(prop, &propEnd, 10);
            if (*propEnd == '\0')
            {
                return val;
            }
            // Badly formatted property. It should just be a number.
            ALOGE("remote.sf.back_camera_hal is not a number: %s", prop);
        }
        return 3;
    }

    void VirtualCameraFactory::onStatusChanged(int cameraId, int newStatus)
    {
        cameraId = trySwitchRemoteCamera(cameraId);

        VirtualBaseCamera *cam = mVirtualCameras[cameraId];
        if (!cam)
        {
            ALOGE("%s: Invalid camera ID %d", __FUNCTION__, cameraId);
            return;
        }

        /*
        * (Order is important)
        * Send the callback first to framework, THEN close the camera.
        */

        if (newStatus == cam->getHotplugStatus())
        {
            ALOGW("%s: Ignoring transition to the same status", __FUNCTION__);
            return;
        }

        const camera_module_callbacks_t *cb = mCallbacks;
        if (cb != nullptr && cb->camera_device_status_change != nullptr)
        {
            cb->camera_device_status_change(cb, cameraId, newStatus);
        }

        if (newStatus == CAMERA_DEVICE_STATUS_NOT_PRESENT)
        {
            cam->unplugCamera();
        }
        else if (newStatus == CAMERA_DEVICE_STATUS_PRESENT)
        {
            cam->plugCamera();
        }
    }

    /********************************************************************************
     * Initializer for the static member structure.
     *******************************************************************************/

    // Entry point for camera HAL API.
    struct hw_module_methods_t VirtualCameraFactory::mCameraModuleMethods = {
        .open = VirtualCameraFactory::device_open};

}; // end of namespace android
