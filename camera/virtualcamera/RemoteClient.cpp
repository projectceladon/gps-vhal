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
 * Contains implementation of classes that encapsulate connection to camera
 * services in the emulator via remote pipe.
 */

// #define LOG_NDEBUG 0
#define LOG_TAG "VirtualCamera_RemoteClient"
#include <log/log.h>
#include "VirtualCamera.h"
#include "RemoteClient.h"
#include "CameraSocketServerThread.h"

#define LOG_QUERIES 0
#if LOG_QUERIES
#define LOGQ(...) ALOGD(__VA_ARGS__)
#else
#define LOGQ(...) (void(0))

#endif // LOG_QUERIES

#define REMOTE_PIPE_DEBUG LOGQ
#include "qemu_pipe.h"

namespace android
{

    /****************************************************************************
     * Remote query
     ***************************************************************************/

    RemoteQuery::RemoteQuery()
        : mQuery(mQueryPrealloc),
          mQueryDeliveryStatus(NO_ERROR),
          mReplyBuffer(NULL),
          mReplyData(NULL),
          mReplySize(0),
          mReplyDataSize(0),
          mReplyStatus(0)
    {
        *mQuery = '\0';
    }

    RemoteQuery::RemoteQuery(const char *query_string)
        : mQuery(mQueryPrealloc),
          mQueryDeliveryStatus(NO_ERROR),
          mReplyBuffer(NULL),
          mReplyData(NULL),
          mReplySize(0),
          mReplyDataSize(0),
          mReplyStatus(0)
    {
        mQueryDeliveryStatus = RemoteQuery::createQuery(query_string, NULL);
    }

    RemoteQuery::RemoteQuery(const char *query_name, const char *query_param)
        : mQuery(mQueryPrealloc),
          mQueryDeliveryStatus(NO_ERROR),
          mReplyBuffer(NULL),
          mReplyData(NULL),
          mReplySize(0),
          mReplyDataSize(0),
          mReplyStatus(0)
    {
        mQueryDeliveryStatus = RemoteQuery::createQuery(query_name, query_param);
    }

    RemoteQuery::~RemoteQuery()
    {
        RemoteQuery::resetQuery();
    }

    status_t RemoteQuery::createQuery(const char *name, const char *param)
    {
        /* Reset from the previous use. */
        resetQuery();

        /* Query name cannot be NULL or an empty string. */
        if (name == NULL || *name == '\0')
        {
            ALOGE("%s: NULL or an empty string is passed as query name.",
                  __FUNCTION__);
            mQueryDeliveryStatus = EINVAL;
            return EINVAL;
        }

        const size_t name_len = strlen(name);
        const size_t param_len = (param != NULL) ? strlen(param) : 0;
        const size_t required = strlen(name) + (param_len ? (param_len + 2) : 1);

        if (required > sizeof(mQueryPrealloc))
        {
            /* Preallocated buffer was too small. Allocate a bigger query buffer. */
            mQuery = new char[required];
            if (mQuery == NULL)
            {
                ALOGE("%s: Unable to allocate %zu bytes for query buffer",
                      __FUNCTION__, required);
                mQueryDeliveryStatus = ENOMEM;
                return ENOMEM;
            }
        }

        /* At this point mQuery buffer is big enough for the query. */
        if (param_len)
        {
            sprintf(mQuery, "%s %s", name, param);
        }
        else
        {
            memcpy(mQuery, name, name_len + 1);
        }

        return NO_ERROR;
    }

    status_t RemoteQuery::completeQuery(status_t status)
    {
        /* Save query completion status. */
        mQueryDeliveryStatus = status;
        if (mQueryDeliveryStatus != NO_ERROR)
        {
            return mQueryDeliveryStatus;
        }

        /* Make sure reply buffer contains at least 'ok', or 'ko'.
        * Note that 'ok', or 'ko' prefixes are always 3 characters long: in case
        * there are more data in the reply, that data will be separated from 'ok'/'ko'
        * with a ':'. If there is no more data in the reply, the prefix will be
        * zero-terminated, and the terminator will be inculded in the reply. */
        if (mReplyBuffer == NULL || mReplySize < 3)
        {
            ALOGE("%s: Invalid reply to the query", __FUNCTION__);
            mQueryDeliveryStatus = EINVAL;
            return EINVAL;
        }

        /* Lets see the reply status. */
        if (!memcmp(mReplyBuffer, "ok", 2))
        {
            mReplyStatus = 1;
        }
        else if (!memcmp(mReplyBuffer, "ko", 2))
        {
            mReplyStatus = 0;
        }
        else
        {
            ALOGE("%s: Invalid query reply: '%s'", __FUNCTION__, mReplyBuffer);
            mQueryDeliveryStatus = EINVAL;
            return EINVAL;
        }

        /* Lets see if there are reply data that follow. */
        if (mReplySize > 3)
        {
            /* There are extra data. Make sure they are separated from the status
            * with a ':' */
            if (mReplyBuffer[2] != ':')
            {
                ALOGE("%s: Invalid query reply: '%s'", __FUNCTION__, mReplyBuffer);
                mQueryDeliveryStatus = EINVAL;
                return EINVAL;
            }
            mReplyData = mReplyBuffer + 3;
            mReplyDataSize = mReplySize - 3;
        }
        else
        {
            /* Make sure reply buffer containing just 'ok'/'ko' ends with
            * zero-terminator. */
            if (mReplyBuffer[2] != '\0')
            {
                ALOGE("%s: Invalid query reply: '%s'", __FUNCTION__, mReplyBuffer);
                mQueryDeliveryStatus = EINVAL;
                return EINVAL;
            }
        }

        return NO_ERROR;
    }

    void RemoteQuery::resetQuery()
    {
        if (mQuery != NULL && mQuery != mQueryPrealloc)
        {
            delete[] mQuery;
        }
        mQuery = mQueryPrealloc;
        mQueryDeliveryStatus = NO_ERROR;
        if (mReplyBuffer != NULL)
        {
            free(mReplyBuffer);
            mReplyBuffer = NULL;
        }
        mReplyData = NULL;
        mReplySize = mReplyDataSize = 0;
        mReplyStatus = 0;
    }

    /****************************************************************************
     * Remote client base
     ***************************************************************************/

    /* Camera service name. */
    const char RemoteClient::mCameraServiceName[] = "camera";

    RemoteClient::RemoteClient()
        : mPipeFD(-1), mCameraSocketFD(-1)
    {
        ALOGV("%s The default value of mCameraSocketFD is %d", __func__, mCameraSocketFD);
    }

    RemoteClient::~RemoteClient()
    {
        if (mPipeFD >= 0)
        {
            close(mPipeFD);
        }
        if (mCameraSocketFD >= 0)
        {
            mCameraSocketFD = -1;
            ALOGV("%s Set current camera socket to -1 without really close as it "
                  " will be used. mCameraSocketFD = %d",
                  __func__, mCameraSocketFD);
        }
    }

    /****************************************************************************
     * Remote client API
     ***************************************************************************/

    status_t RemoteClient::connectClient(const char *param)
    {
        ALOGV("%s: '%s'", __FUNCTION__, param ? param : "");

        /* Make sure that client is not connected already. */
        if (mCameraSocketFD < 0)
        {
            ALOGE("%s: Camera client is not connected. mCameraSocketFD = %d", __FUNCTION__, mCameraSocketFD);
            return EINVAL;
        }

        char query_str[256];
        memset(query_str, 0, sizeof(query_str));
        snprintf(query_str, sizeof(query_str), "%s", param);
        RemoteQuery query(query_str);
        query.csi.cmd = CMD_NAME;
        query.csi.data_size = strlen(query_str) + 1;
        doQuery(&query);
        const status_t res = query.getCompletionStatus();
        if (res != NO_ERROR)
        {
            ALOGE("%s: Query failed: %s",
                  __FUNCTION__, query.mReplyData ? query.mReplyData : "No error message");
            return res;
        }
        return NO_ERROR;
    }

    void RemoteClient::disconnectClient()
    {
        ALOGV("%s", __FUNCTION__);

        if (mCameraSocketFD >= 0)
        {
            ALOGV("%s Set camera client from %d to -1 than close as the socket "
                  "will be used in future.",
                  __FUNCTION__, mCameraSocketFD);
            mCameraSocketFD = -1;
        }
    }

    status_t RemoteClient::setCameraFD(int socketFd)
    {
        ALOGV("%s", __FUNCTION__);
        mCameraSocketFD = socketFd;
        ALOGV("%s mCameraSocketFD = %d", __FUNCTION__, mCameraSocketFD);
        return NO_ERROR;
    }

    status_t RemoteClient::cleanCameraFD(int socketFd)
    {
        ALOGV("%s clean camera fd. mCameraSocketFD = %d", __FUNCTION__, mCameraSocketFD);
        mCameraSocketFD = -1;
        return NO_ERROR;
    }

    status_t RemoteClient::sendMessage(const void *data, size_t data_size)
    {
        if (mCameraSocketFD < 0)
        {
            ALOGE("%s: Remote client is not connected", __FUNCTION__);
            return EINVAL;
        }

        // const size_t written = TEMP_FAILURE_RETRY(write(mCameraSocketFD, data, data_size));
        const size_t written = write_spec_size(mCameraSocketFD, data, data_size);
        if (written == data_size)
        {
            return NO_ERROR;
        }
        else
        {
            ALOGE("%s: Error sending data via socket (%d): '%s'",
                  __FUNCTION__, mCameraSocketFD, strerror(errno));
            return errno ? errno : EIO;
        }
    }

    ssize_t RemoteClient::read_spec_size(int fd, void *buf, size_t count)
    {
        ssize_t ret = 0;
        memset(buf, 0, count);
        ssize_t len = count;
        char *pointer = (char *)buf;
        while (len > 0)
        {
            do
            {
                ret = read(fd, pointer, len);
            } while (ret < 0 && errno == EINTR);
            if (ret == 0)
            {
                ALOGV("%s:%d the camera client(%d) may close. Close this client.\n", __func__, __LINE__, fd);
                shutdown(fd, SHUT_RDWR);
                close(fd);
                fd = -1;
                ALOGV("%s:%d mCameraSocketFD = %d\n", __func__, __LINE__, mCameraSocketFD);
                break;
            }
            if (ret > 0)
            {
                pointer += ret;
                len -= ret;
            }
        }
        if (len == 0)
        {
            return count;
        }
        else
        {
            return ret;
        }
    }

    ssize_t RemoteClient::write_spec_size(int fd, const void *buf, size_t count)
    {
        ssize_t ret = 0;
        ssize_t len = count;
        char *pointer = (char *)buf;
        while (len > 0)
        {
            do
            {
                ret = write(fd, pointer, len);
            } while (ret < 0 && errno == EINTR);
            if (ret == 0)
            {
                ALOGV("%s:%d the camera client(%d) may close. Close this client.\n", __func__, __LINE__, fd);
                shutdown(fd, SHUT_RDWR);
                close(fd);
                fd = -1;
                ALOGV("%s:%d mCameraSocketFD = %d\n", __func__, __LINE__, mCameraSocketFD);
                break;
            }
            if (ret > 0)
            {
                pointer += ret;
                len -= ret;
            }
        }
        if (len == 0)
        {
            return count;
        }
        else
        {
            return ret;
        }
    }

    status_t RemoteClient::receiveMessage(void **data, size_t *data_size)
    {
        *data = NULL;
        *data_size = 0;

        if (mCameraSocketFD < 0)
        {
            ALOGE("%s: Remote client is not connected", __FUNCTION__);
            return EINVAL;
        }

        /* The way the service replies to a query, it sends payload size first, and
        * then it sends the payload itself. Note that payload size is sent as a
        * string, containing 8 characters representing a hexadecimal payload size
        * value. Note also, that the string doesn't contain zero-terminator. */
        size_t payload_size;
        char payload_size_str[9];
        // int rd_res = TEMP_FAILURE_RETRY(read(mCameraSocketFD, payload_size_str, 8));
        int rd_res = read_spec_size(mCameraSocketFD, payload_size_str, 8);

        /* Convert payload size. */
        errno = 0;
        payload_size_str[8] = '\0';
        payload_size = strtol(payload_size_str, NULL, 16);
        if (errno)
        {
            ALOGE("%s: Invalid payload size '%s'", __FUNCTION__, payload_size_str);
            return EIO;
        }

        /* Allocate payload data buffer, and read the payload there. */
        *data = malloc(payload_size);
        if (*data == NULL)
        {
            ALOGE("%s: Unable to allocate %zu bytes payload buffer",
                  __FUNCTION__, payload_size);
            return ENOMEM;
        }
        // rd_res = TEMP_FAILURE_RETRY(read(mCameraSocketFD, *data, payload_size));
        rd_res = read_spec_size(mCameraSocketFD, *data, payload_size);
        if (static_cast<size_t>(rd_res) == payload_size)
        {
            *data_size = payload_size;
            return NO_ERROR;
        }
        else
        {
            ALOGE("%s: Read size %d doesnt match expected payload size %zu: %s",
                  __FUNCTION__, rd_res, payload_size, strerror(errno));
            free(*data);
            *data = NULL;
            return errno ? errno : EIO;
        }
    }

    status_t RemoteClient::doQuery(RemoteQuery *query)
    {
        /* Make sure that query has been successfuly constructed. */
        if (query->mQueryDeliveryStatus != NO_ERROR)
        {
            ALOGE("%s: Query is invalid", __FUNCTION__);
            return query->mQueryDeliveryStatus;
        }

        LOGQ("Send query '%s'.", query->mQuery);
        /** Send the header. */
        status_t res = sendMessage(&query->csi, sizeof(struct camera_socket_info));
        if (res != NO_ERROR)
        {
            ALOGE("%s: Send the header of query '%s' failed: %s",
                  __FUNCTION__, query->mQuery, strerror(res));
            return res;
        }
        if (query->csi.data_size > 0)
        {
            /* Send the query. */
            res = sendMessage(query->mQuery, strlen(query->mQuery) + 1);
            if (res != NO_ERROR)
            {
                ALOGE("%s: Send query '%s' failed: %s",
                      __FUNCTION__, query->mQuery, strerror(res));
                return res;
            }
        }

        /* Read the response. */
        res = receiveMessage(reinterpret_cast<void **>(&query->mReplyBuffer),
                             &query->mReplySize);
        if (res == NO_ERROR)
        {
            LOGQ("Response to query '%s': Status = '%.2s', %d bytes in response",
                 query->mQuery, query->mReplyBuffer, query->mReplySize);
        }
        else
        {
            ALOGE("%s Response to query '%s' has failed: %s",
                  __FUNCTION__, query->mQuery, strerror(res));
        }

        /* Complete the query, and return its completion handling status. */
        const status_t res1 = query->completeQuery(res);
        ALOGE_IF(res1 != NO_ERROR && res1 != res,
                 "%s: Error %d in query '%s' completion",
                 __FUNCTION__, res1, query->mQuery);
        return res1;
    }

    /****************************************************************************
     * Remote client for the 'factory' service.
     ***************************************************************************/

    /*
    * Factory service queries.
    */

    /* Queries list of cameras connected to the host. */
    const char FactoryRemoteClient::mQueryList[] = "list";

    FactoryRemoteClient::FactoryRemoteClient()
        : RemoteClient()
    {
    }

    FactoryRemoteClient::~FactoryRemoteClient()
    {
    }

    status_t FactoryRemoteClient::listCameras(char **list)
    {
        ALOGV("%s", __FUNCTION__);

        RemoteQuery query(mQueryList);
        query.csi.cmd = CMD_LIST;
        query.csi.data_size = 0;
        if (doQuery(&query) || !query.isQuerySucceeded())
        {
            ALOGE("%s: List cameras query failed: %s", __FUNCTION__,
                  query.mReplyData ? query.mReplyData : "No error message");
            return query.getCompletionStatus();
        }

        /* Make sure there is a list returned. */
        if (query.mReplyDataSize == 0)
        {
            ALOGE("%s: No camera list is returned.", __FUNCTION__);
            return EINVAL;
        }

        /* Copy the list over. */
        *list = (char *)malloc(query.mReplyDataSize);
        if (*list != NULL)
        {
            memcpy(*list, query.mReplyData, query.mReplyDataSize);
            ALOGD("Virtual camera list: %s", *list);
            return NO_ERROR;
        }
        else
        {
            ALOGE("%s: Unable to allocate %zu bytes",
                  __FUNCTION__, query.mReplyDataSize);
            return ENOMEM;
        }
    }

    /****************************************************************************
     * Remote client for an 'virtual camera' service.
     ***************************************************************************/

    /*
    * Virtual camera queries
    */

    /* Connect to the camera device. */
    const char CameraRemoteClient::mQueryConnect[] = "connect";
    /* Disconect from the camera device. */
    const char CameraRemoteClient::mQueryDisconnect[] = "disconnect";
    /* Start capturing video from the camera device. */
    const char CameraRemoteClient::mQueryStart[] = "start";
    /* Stop capturing video from the camera device. */
    const char CameraRemoteClient::mQueryStop[] = "stop";
    /* Get next video frame from the camera device. */
    const char CameraRemoteClient::mQueryFrame[] = "frame";

    CameraRemoteClient::CameraRemoteClient()
        : RemoteClient()
    {
    }

    CameraRemoteClient::~CameraRemoteClient()
    {
    }

    status_t CameraRemoteClient::queryConnect()
    {
        ALOGV("%s", __FUNCTION__);

        RemoteQuery query(mQueryConnect);
        query.csi.cmd = CMD_CONNECT;
        query.csi.data_size = 0;
        doQuery(&query);
        const status_t res = query.getCompletionStatus();
        ALOGE_IF(res != NO_ERROR, "%s: Query failed: %s",
                 __FUNCTION__, query.mReplyData ? query.mReplyData : "No error message");
        return res;
    }

    status_t CameraRemoteClient::queryDisconnect()
    {
        ALOGV("%s", __FUNCTION__);

        RemoteQuery query(mQueryDisconnect);
        query.csi.cmd = CMD_DISCONNECT;
        query.csi.data_size = 0;
        doQuery(&query);
        const status_t res = query.getCompletionStatus();
        ALOGE_IF(res != NO_ERROR, "%s: Query failed: %s",
                 __FUNCTION__, query.mReplyData ? query.mReplyData : "No error message");
        return res;
    }

    status_t CameraRemoteClient::queryStart(uint32_t pixel_format,
                                            int width,
                                            int height)
    {
        ALOGV("%s", __FUNCTION__);

        char query_str[256];
        snprintf(query_str, sizeof(query_str), "%s dim=%dx%d pix=%d",
                 mQueryStart, width, height, pixel_format);
        RemoteQuery query(query_str);
        query.csi.cmd = CMD_START;
        query.csi.data_size = strlen(query_str) + 1;
        doQuery(&query);
        const status_t res = query.getCompletionStatus();
        ALOGE_IF(res != NO_ERROR, "%s: Query failed: %s",
                 __FUNCTION__, query.mReplyData ? query.mReplyData : "No error message");
        return res;
    }

    status_t CameraRemoteClient::queryStop()
    {
        ALOGV("%s", __FUNCTION__);

        RemoteQuery query(mQueryStop);
        query.csi.cmd = CMD_STOP;
        query.csi.data_size = 0;
        doQuery(&query);
        const status_t res = query.getCompletionStatus();
        ALOGE_IF(res != NO_ERROR, "%s: Query failed: %s",
                 __FUNCTION__, query.mReplyData ? query.mReplyData : "No error message");
        return res;
    }

    status_t CameraRemoteClient::queryFrame(void *vframe,
                                            void *pframe,
                                            size_t vframe_size,
                                            size_t pframe_size,
                                            float r_scale,
                                            float g_scale,
                                            float b_scale,
                                            float exposure_comp,
                                            int64_t *frame_time)
    {
        ALOGV("%s", __FUNCTION__);

        char query_str[256];
        memset(query_str, 0, sizeof(query_str));
        snprintf(query_str, sizeof(query_str), "%s video=%zu preview=%zu whiteb=%g,%g,%g expcomp=%g time=%d",
                 mQueryFrame, (vframe && vframe_size) ? vframe_size : 0,
                 (pframe && pframe_size) ? pframe_size : 0, r_scale, g_scale, b_scale,
                 exposure_comp, frame_time != nullptr ? 1 : 0);
        RemoteQuery query(query_str);
        query.csi.cmd = CMD_FRAME;
        query.csi.data_size = strlen(query_str) + 1;
        doQuery(&query);
        const status_t res = query.getCompletionStatus();
        if (res != NO_ERROR)
        {
            ALOGE("%s: Query failed: %s",
                  __FUNCTION__, query.mReplyData ? query.mReplyData : "No error message");
            return res;
        }

        /* Copy requested frames. */
        size_t cur_offset = 0;
        const uint8_t *frame = reinterpret_cast<const uint8_t *>(query.mReplyData);
        /* Video frame is always first. */
        if (vframe != NULL && vframe_size != 0)
        {
            /* Make sure that video frame is in. */
            if ((query.mReplyDataSize - cur_offset) >= vframe_size)
            {
                memcpy(vframe, frame, vframe_size);
                cur_offset += vframe_size;
            }
            else
            {
                ALOGE("%s: Reply %zu bytes is to small to contain %zu bytes video frame",
                      __FUNCTION__, query.mReplyDataSize - cur_offset, vframe_size);
                return EINVAL;
            }
        }
        if (pframe != NULL && pframe_size != 0)
        {
            /* Make sure that preview frame is in. */
            if ((query.mReplyDataSize - cur_offset) >= pframe_size)
            {
                memcpy(pframe, frame + cur_offset, pframe_size);
                cur_offset += pframe_size;
            }
            else
            {
                ALOGE("%s: Reply %zu bytes is to small to contain %zu bytes preview frame",
                      __FUNCTION__, query.mReplyDataSize - cur_offset, pframe_size);
                return EINVAL;
            }
        }
        if (frame_time != nullptr)
        {
            if (query.mReplyDataSize - cur_offset >= 8)
            {
                *frame_time = *reinterpret_cast<const int64_t *>(frame + cur_offset);
                cur_offset += 8;
            }
            else
            {
                *frame_time = 0L;
            }
        }

        return NO_ERROR;
    }

}; /* namespace android */
