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
#define LOG_NDEBUG 0
//#define LOG_NNDEBUG 0
#define LOG_TAG "CameraSocketServerThread"
#include <log/log.h>

#ifdef LOG_NNDEBUG
#define ALOGVV(...) ALOGV(__VA_ARGS__)
#else
#define ALOGVV(...) ((void)0)
#endif

#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include "CameraSocketServerThread.h"
#include "VirtualBuffer.h"
#include "VirtualCameraFactory.h"
#define DUMP_FROM_LAPTOP_TO_SERVER(filename, p_addr1, len1, p_addr2, len2) \
  ({                                                                       \
    size_t rc = 0;                                                         \
    FILE *fp = fopen("/ipc/filename.yuv", "w+");                           \
    if (fp) {                                                              \
      rc = fwrite(p_addr1, 1, len1, fp);                                   \
      rc = fwrite(p_addr2, 1, len2, fp);                                   \
      fclose(fp);                                                          \
    } else {                                                               \
      ALOGE("open failed!!!");                                             \
    }                                                                      \
  })

#if 0
	if (i <= 0){
		DUMP_FROM_LAPTOP_TO_SERVER(i, fbuffer, 307200, uv_add, 153600);
		i++;
	}
#endif

android::ClientVideoBuffer *android::ClientVideoBuffer::ic_instance = 0;

namespace android {

CameraSocketServerThread::CameraSocketServerThread(int containerId,
                                                   VirtualCameraFactory &ecf)
    : Thread(/*canCallJava*/ false) {
  mRunning = true;
  mSocketServerFd = -1;

  char container_id_str[64] = {
      '\0',
  };
  snprintf(container_id_str, sizeof(container_id_str), "/ipc/camera-socket%d",
           containerId);
  const char *pSocketServerFile =
      (getenv("K8S_ENV") != NULL && strcmp(getenv("K8S_ENV"), "true") == 0)
          ? "/conn/camera-socket"
          : container_id_str;
  snprintf(mSocketServerFile, 64, "%s", pSocketServerFile);
  pecf = &ecf;
}

CameraSocketServerThread::~CameraSocketServerThread() {
  if (mClientFd > 0) {
    shutdown(mClientFd, SHUT_RDWR);
    close(mClientFd);
    mClientFd = -1;
    gVirtualCameraFactory.setSocketFd(mClientFd);
  }
  if (mSocketServerFd > 0) {
    close(mSocketServerFd);
    mSocketServerFd = -1;
  }
}

status_t CameraSocketServerThread::requestExitAndWait() {
  ALOGE("%s: Not implemented. Use requestExit + join instead", __FUNCTION__);
  return INVALID_OPERATION;
}
int CameraSocketServerThread::getClientFd() {
  Mutex::Autolock al(mMutex);
  return mClientFd;
}

void CameraSocketServerThread::requestExit() {
  Mutex::Autolock al(mMutex);

  ALOGV("%s: Requesting thread exit", __FUNCTION__);
  mRunning = false;
  ALOGV("%s: Request exit complete.", __FUNCTION__);
}

status_t CameraSocketServerThread::readyToRun() {
  Mutex::Autolock al(mMutex);

  return OK;
}

bool CameraSocketServerThread::threadLoop() {
  int ret = 0;
  int newClientFd = -1;
  int port = 8080;
  int so_reuseaddr = 1;
  struct sockaddr_in addr_ip;
  struct sockaddr_un addr_un;

  mSocktypeIP = (getenv("CAM_SOCK_TYPE") != NULL &&
                 strcmp(getenv("CAM_SOCK_TYPE"), "ip") == 0)
                    ? true
                    : false;
  ALOGI("%s: Constructing camera socket server with socket type %s...",
        __FUNCTION__, getenv("CAM_SOCK_TYPE"));
  if (!mSocktypeIP) {
    mSocketServerFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (mSocketServerFd < 0) {
      ALOGE("%s:%d Fail to construct camera socket with error: %s",
            __FUNCTION__, __LINE__, strerror(errno));
      return false;
    }

    memset(&addr_un, 0, sizeof(addr_un));
    addr_un.sun_family = AF_UNIX;
    strncpy(&addr_un.sun_path[0], mSocketServerFile, strlen(mSocketServerFile));
    if ((access(mSocketServerFile, F_OK)) != -1) {
      ALOGW("%s camera socket server file is %s", __FUNCTION__,
            mSocketServerFile);
      ret = unlink(mSocketServerFile);
      if (ret < 0) {
        ALOGW("%s Failed to unlink %s address %d, %s", __FUNCTION__,
              mSocketServerFile, ret, strerror(errno));
        return false;
      }
    } else {
      ALOGW("%s camera socket server file %s will created. ", __FUNCTION__,
            mSocketServerFile);
    }

    ret = bind(mSocketServerFd, (struct sockaddr *)&addr_un,
               sizeof(sa_family_t) + strlen(mSocketServerFile) + 1);
    if (ret < 0) {
      ALOGE("%s Failed to bind %s address %d, %s", __FUNCTION__,
            mSocketServerFile, ret, strerror(errno));
      return false;
    }

    struct stat st;
    __mode_t mod = S_IRWXU | S_IRWXG | S_IRWXO;
    if (fstat(mSocketServerFd, &st) == 0) {
      mod |= st.st_mode;
    }
    chmod(mSocketServerFile, mod);
    stat(mSocketServerFile, &st);
  } else {
    mSocketServerFd = socket(AF_INET, SOCK_STREAM, 0);
    if (mSocketServerFd < 0) {
      ALOGE("%s:%d Fail to construct camera socket with error: %s",
            __FUNCTION__, __LINE__, strerror(errno));
      return false;
    }
    if (setsockopt(mSocketServerFd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr,
                   sizeof(int)) < 0) {
      ALOGE(LOG_TAG "%s setsockopt(SO_REUSEADDR) failed. : %d\n", __func__,
            mSocketServerFd);
      return false;
    }
    memset(&addr_ip, 0, sizeof(addr_ip));
    addr_ip.sin_family = AF_INET;
    addr_ip.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_ip.sin_port = htons(port);

    ret = bind(mSocketServerFd, (struct sockaddr *)&addr_ip,
               sizeof(struct sockaddr_in));
    if (ret < 0) {
      ALOGE("%s Failed to bind port(%d). ret: %d, %s", __func__, port, ret,
            strerror(errno));
      return false;
    }
  }
  ret = listen(mSocketServerFd, 5);
  if (ret < 0) {
    ALOGE("%s Failed to listen on %s", __FUNCTION__, mSocketServerFile);
    return false;
  }

  while (mRunning) {
    ALOGE("%s: Wait for camera client to connect...", __FUNCTION__);
    if (mSocktypeIP) {
      socklen_t alen = sizeof(struct sockaddr_in);
      newClientFd = accept(mSocketServerFd, (struct sockaddr *)&addr_ip, &alen);
    } else {
      socklen_t alen = sizeof(struct sockaddr_un);
      newClientFd = accept(mSocketServerFd, (struct sockaddr *)&addr_un, &alen);
    }
    ALOGE("%s: Accepted client connect... %d", __FUNCTION__, newClientFd);
    if (newClientFd < 0) {
      ALOGE("%s: Fail to accept client. Error: %s", __FUNCTION__,
            strerror(errno));
      continue;
    }
    mClientFd = newClientFd;

    gVirtualCameraFactory.setSocketFd(mClientFd);

    int size = 0;
    static int i;
    struct pollfd fd;
    int ret;
    int event;

    ClientVideoBuffer *handle = ClientVideoBuffer::getClientInstance();

    fd.fd = mClientFd;  // your socket handler
    fd.events = POLLIN | POLLHUP;

    while (true) {
      char *fbuffer =
          (char *)handle->clientBuf[handle->clientRevCount % 1].buffer;
      char *uv_add = (char *)(fbuffer + 307200 + 1);
      if (mSocktypeIP) {
        if ((size = recv(mClientFd, (char *)fbuffer, 460800, MSG_WAITALL)) >
            0) {
          handle->clientRevCount++;
          ALOGVV(LOG_TAG
                 "%s: Pocket rev %d and "
                 "size %d",
                 __FUNCTION__, handle->clientRevCount, size);
        } else {
          ALOGE(LOG_TAG "%s: Remote client closed connection ", __FUNCTION__);
          shutdown(mClientFd, SHUT_RDWR);
          close(mClientFd);
          mClientFd = -1;
          gVirtualCameraFactory.setSocketFd(mClientFd);
          break;
        }
      } else {
        ret = poll(&fd, 1, 3000);  // 1 second for timeout
        // check if there are any events on fd.
        // if event is POLLHUP, then socket/fd is closed at the other end.
        //   you can close this socket.
        // if event is POLLIN, then data is available in socket/fd.
        //   you can read data from this socket.

        // if (POLLHUP) { ... }
        // else if (POLLIN) { recv()... }
        // else /* timeout */ { continue poll }

        event = fd.revents;  // returned events

        if (event & POLLHUP) {  // connnection disconnected
          ALOGE("%s: POLLHUP: Close camera socket connection", __FUNCTION__);
          close(mClientFd);
          mClientFd = -1;
          gVirtualCameraFactory.setSocketFd(mClientFd);
          break;
        } else if (event & POLLIN) {  // preview / record
          if ((size = recv(mClientFd, (char *)fbuffer, 460800, MSG_WAITALL)) >
              0) {
            handle->clientRevCount++;
            ALOGVV(
                "%s: Pocket rev %d and "
                "size %d",
                __FUNCTION__, handle->clientRevCount, size);
          }
        } else {
          //	ALOGE("%s: continue polling..", __FUNCTION__);
        }
      }
    }
  }
  ALOGE("%s: Quit CameraSocketServerThread... %s(%d)", __FUNCTION__,
        mSocketServerFile, mClientFd);
  close(mClientFd);
  mClientFd = -1;
  gVirtualCameraFactory.setSocketFd(mClientFd);
  close(mSocketServerFd);
  mSocketServerFd = -1;
  return true;
}

}  // namespace android
