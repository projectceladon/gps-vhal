/*
 * Copyright (C) 2013 The Android Open Source Project
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
// #define LOG_NDEBUG 0
#define LOG_TAG "CameraSocketServerThread"
#include <log/log.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/inotify.h>

#include "CameraSocketServerThread.h"
#include "VirtualCameraFactory.h"

namespace android
{

    CameraSocketServerThread::CameraSocketServerThread(int containerId, VirtualCameraFactory &ecf) : Thread(/*canCallJava*/ false)
    {
        mRunning = true;
        mSocketServerFd = -1;

        char container_id_str[64] = {
            '\0',
        };
        snprintf(container_id_str, sizeof(container_id_str), "/ipc/camera-socket%d", containerId);
        const char *pSocketServerFile = (getenv("K8S_ENV") != NULL && strcmp(getenv("K8S_ENV"), "true") == 0)
                                       ? "/conn/camera-socket"
                                       : container_id_str;
        snprintf(mSocketServerFile, 64, "%s", pSocketServerFile);
        pecf = &ecf;
    }

    CameraSocketServerThread::~CameraSocketServerThread()
    {
        if (mClientFd > 0)
        {
            close(mClientFd);
            mClientFd = -1;
        }
        if (mSocketServerFd > 0)
        {
            close(mSocketServerFd);
            mSocketServerFd = -1;
        }
    }

    status_t CameraSocketServerThread::requestExitAndWait()
    {
        ALOGE("%s: Not implemented. Use requestExit + join instead",
              __FUNCTION__);
        return INVALID_OPERATION;
    }
    int CameraSocketServerThread::getClientFd()
    {
        Mutex::Autolock al(mMutex);
        return mClientFd;
    }

    void CameraSocketServerThread::requestExit()
    {
        Mutex::Autolock al(mMutex);

        ALOGV("%s: Requesting thread exit", __FUNCTION__);
        mRunning = false;
        ALOGV("%s: Request exit complete.", __FUNCTION__);
    }

    status_t CameraSocketServerThread::readyToRun()
    {
        Mutex::Autolock al(mMutex);

        return OK;
    }

    bool CameraSocketServerThread::threadLoop()
    {
        int ret = 0;
        int newClientFd = -1;

        ALOGV("%s Constructing camera socket server...", __FUNCTION__);
        mSocketServerFd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (mSocketServerFd < 0)
        {
            ALOGE("%s:%d Fail to construct camera socket with error: %s",
                  __FUNCTION__, __LINE__, strerror(errno));
            return false;
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(&addr.sun_path[0], mSocketServerFile, strlen(mSocketServerFile));
        if ((access(mSocketServerFile, F_OK)) != -1)
        {
            ALOGW("%s camera socket server file is %s", __FUNCTION__, mSocketServerFile);
            ret = unlink(mSocketServerFile);
            if (ret < 0)
            {
                ALOGW("%s Failed to unlink %s address %d, %s", __FUNCTION__, mSocketServerFile, ret, strerror(errno));
                return false;
            }
        }
        else
        {
            ALOGW("%s camera socket server file %s will created. ", __FUNCTION__, mSocketServerFile);
        }

        ret = bind(mSocketServerFd, (struct sockaddr *)&addr, sizeof(sa_family_t) + strlen(mSocketServerFile) + 1);
        if (ret < 0)
        {
            ALOGE("%s Failed to bind %s address %d, %s", __FUNCTION__, mSocketServerFile, ret, strerror(errno));
            return false;
        }

        struct stat st;
        __mode_t mod = S_IRWXU | S_IRWXG | S_IRWXO;
        if (fstat(mSocketServerFd, &st) == 0)
        {
            mod |= st.st_mode;
        }
        chmod(mSocketServerFile, mod);
        stat(mSocketServerFile, &st);

        ret = listen(mSocketServerFd, 5);
        if (ret < 0)
        {
            ALOGE("%s Failed to listen on %s", __FUNCTION__, mSocketServerFile);
            return false;
        }

        while (mRunning)
        {
            socklen_t alen = sizeof(struct sockaddr_un);
            ALOGV("%s Wait a camera client to connect...", __FUNCTION__);
            newClientFd = accept(mSocketServerFd, (struct sockaddr *)&addr, &alen);
            if (newClientFd < 0)
            {
                ALOGE("%s Fail to accept client. Error: %s",
                      __func__, strerror(errno));
            }
            else
            {
                if (mClientFd > 0)
                {
                    if (pecf != nullptr)
                    {
                        pecf->cameraClientDisconnect(mClientFd);
                    }
                    ALOGV("%s Close previours camera client(mClientFd = %d)", __FUNCTION__, mClientFd);
                    close(mClientFd);
                    mClientFd = -1;
                }
                mClientFd = newClientFd;
                ALOGV("%s A camera client connected to server. mClientFd = %d", __FUNCTION__, mClientFd);
                if (pecf != nullptr)
                {
                    pecf->cameraClientConnect(mClientFd);
                }
            }
        }
        ALOGV("%s Quit. %s(%d)", __FUNCTION__, mSocketServerFile, mClientFd);
        close(mClientFd);
        mClientFd = -1;
        close(mSocketServerFd);
        mSocketServerFd = -1;
        return true;
    }

} //namespace android
