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

#ifndef CAMERA_SOCKET_SERVER_H
#define CAMERA_SOCKET_SERVER_H

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <utils/Mutex.h>
#include <utils/String8.h>
#include <utils/Thread.h>
#include <utils/Vector.h>
#include <string>

namespace android {

class VirtualCameraFactory;
class CameraSocketServerThread : public Thread {
public:
    CameraSocketServerThread(int containerId, VirtualCameraFactory &ecf);
    ~CameraSocketServerThread();

    virtual void requestExit();
    virtual status_t requestExitAndWait();
    int getClientFd();
    void clearBuffer(char *fbuffer, int width, int height);

private:
    virtual status_t readyToRun();
    virtual bool threadLoop();

    Mutex mMutex;
    bool mRunning;  // guarding only when it's important
    int mSocketServerFd = -1;
    char mSocketServerFile[64];
    int mClientFd = -1;
    bool mSocktypeIP = false;
    android::VirtualCameraFactory *pecf = nullptr;
};
}  // namespace android

#endif
