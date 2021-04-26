/*
 * Copyright (C) 2017 The Android Open Source Project
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
 *
 */

#include "./sensor_interfaces.h"

namespace android {

namespace shared {
    static struct sensor_info sensorData[MAX_SENSOR];
    static std::mutex sensorLock;
    static bool isClientConnected;
    static bool status[MAX_SENSOR];
}

RemoteSensors::RemoteSensors() {
    mSensorInterface = new SensorSocketServerThread();
    mSensorInterface->run("SensorSocketServerThread");
}

int RemoteSensors::get_sensors_list(struct sensors_module_t* module,
        struct sensor_t const** list) {
    UNUSED(module);
    *list = this->sSensorList;
    ALOGD("%s: numberOfSensors found = %d", __func__, MAX_SENSOR);
    return MAX_SENSOR;
}

int RemoteSensors::poll(struct sensors_poll_device_t* dev,
        sensors_event_t* data, int count) {
    UNUSED(dev);
    UNUSED(count);

    /* recomended to block until valid connection */
    while (shared::isClientConnected == false)
        sleep(2);

    shared::sensorLock.lock();
    int ev_count = 0;
    for (int handle = 0; handle < MAX_SENSOR ; handle++) {
        if (is_meta_data_pending) {
            is_meta_data_pending = false;
            data[handle].version = META_DATA_VERSION;
            data[handle].type = SENSOR_TYPE_META_DATA;
            data[handle].reserved0 = 0;
            data[handle].timestamp = 0;
            data[handle].meta_data.sensor = 0;
            data[handle].meta_data.what = META_DATA_FLUSH_COMPLETE;
            ev_count++;
        } else {
            if (shared::status[handle] == true) {
                data[ev_count].version = sizeof(sensors_event_t);
                data[ev_count].timestamp = getTimeStamp(CLOCK_MONOTONIC);
                data[ev_count].sensor = handle;
                data[ev_count].type = sSensorList[handle].type;
                data[ev_count].data[0] =
                    static_cast<float> (shared::sensorData[handle].data[0]);
                data[ev_count].data[1] =
                    static_cast<float> (shared::sensorData[handle].data[1]);
                data[ev_count].data[2] =
                    static_cast<float> (shared::sensorData[handle].data[2]);
                shared::sensorLock.unlock();

                switch (handle) {
                    case ACCEL:
                        ALOGD("OnSensor-poll accel: (%f, %f, %f)",
                                data[ev_count].data[0],
                                data[ev_count].data[1],
                                data[ev_count].data[2]);
                        break;
                    case GYRO:
                        ALOGD("OnSensor-poll gyro: (%f, %f, %f)",
                                data[ev_count].data[0],
                                data[ev_count].data[1],
                                data[ev_count].data[2]);
                        break;
                    case MAGN:
                        ALOGD("OnSsensor-poll magn: (%f, %f, %f)",
                                data[ev_count].data[0],
                                data[ev_count].data[1],
                                data[ev_count].data[2]);
                        break;
                }
                ev_count++;
            }
        }
    }
    shared::sensorLock.unlock();
    return ev_count;
}

int RemoteSensors::activate(struct sensors_poll_device_t *dev,
        int handle, int enabled) {
    UNUSED(dev);
    UNUSED(handle);
    UNUSED(enabled);
    shared::status[handle] = enabled;
    return 0;
}

int RemoteSensors::setDelay(struct sensors_poll_device_t *dev,
        int handle, int64_t ns) {
    UNUSED(dev);
    UNUSED(handle);
    UNUSED(ns);
    return 0;
}
int64_t RemoteSensors::getTimeStamp(clockid_t clock_id) {
    struct timespec ts = {0, 0};

    if (!clock_gettime(clock_id, &ts))
        return 1000000000LL * ts.tv_sec + ts.tv_nsec;
    else
        return -1;
}
int RemoteSensors::batch(struct sensors_poll_device_1* dev,
        int handle, int flags,
        int64_t sampling_period_ns,
        int64_t max_report_latency_ns) {
    UNUSED(dev);
    UNUSED(handle);
    UNUSED(flags);
    UNUSED(sampling_period_ns);
    UNUSED(max_report_latency_ns);
    return 0;
}

int RemoteSensors::flush(struct sensors_poll_device_1* dev,
        int handle) {
    UNUSED(dev);
    UNUSED(handle);
    is_meta_data_pending = true;
    return 0;
}

/* Nothing to be cleared on close */
int RemoteSensors::close(struct hw_device_t *dev) {
    UNUSED(dev);
    return 0;
}

/*******************************************
 *   SocketServerImplemntation             *
 *******************************************/
SensorSocketServerThread::SensorSocketServerThread() : Thread(false) {
    mRunning = true;
    mSocketServerFd = -1;
    mClientFd = -1;

    char build_id_buf[PROPERTY_VALUE_MAX] = {'\0'};
    property_get("ro.build.id", build_id_buf, "");
    ALOGD("Server thread for remote sensors - started! (%s)", build_id_buf);

    char container_id_buf[PROPERTY_VALUE_MAX] = {'\0'};
    int containerId = 0;
    if (property_get("ro.boot.container.id", container_id_buf, "") > 0) {
        containerId = atoi(container_id_buf);
    }

    char container_id_str[64] = {'\0'};
    snprintf(container_id_str, sizeof(container_id_str),
            "/ipc/sensor-sock%d", containerId);

    const char *pSocketServerFile =
        (getenv("K8S_ENV") != NULL && strcmp(getenv("K8S_ENV"), "true") == 0)
        ? "/conn/sensor-sock"
        : container_id_str;

    snprintf(mSocketServerFile,
            sizeof(mSocketServerFile), "%s", pSocketServerFile);
}

SensorSocketServerThread::~SensorSocketServerThread() {
    if (mClientFd > 0) {
        close(mClientFd);
        mClientFd = -1;
    }
    if (mSocketServerFd > 0) {
        close(mSocketServerFd);
        mSocketServerFd = -1;
    }
}

bool SensorSocketServerThread::threadLoop() {
    int ret = 0;
    int newClientFd = -1;

    ALOGD("Creating server-socket for sensor...");
    mSocketServerFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (mSocketServerFd < 0) {
        ALOGE("Fail to construct sensor socket with error: %s",
                strerror(errno));
        return false;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[0], mSocketServerFile, strlen(mSocketServerFile));
    if ((access(mSocketServerFile, F_OK)) != -1) {
        ALOGV("Sensor socket server file %s already exist", mSocketServerFile);
        ret = unlink(mSocketServerFile);
        if (ret < 0) {
            ALOGE("Failed to unlink %s address %d, %s",
                    mSocketServerFile, ret, strerror(errno));
            return false;
        }
    } else {
        ALOGD("SensorSocketfile %s created.", mSocketServerFile);
    }

    ret = bind(mSocketServerFd, (struct sockaddr *)&addr,
            sizeof(sa_family_t) + strlen(mSocketServerFile) + 1);
    if (ret < 0) {
        ALOGE("Failed to bind %s address %d, %s",
                mSocketServerFile, ret, strerror(errno));
        return false;
    }
    ret = listen(mSocketServerFd, 5);
    if (ret < 0) {
        ALOGE("Failed to listen sensor-server on %s", mSocketServerFile);
        return false;
    }

    while (mRunning) {
        socklen_t alen = sizeof(struct sockaddr_un);
        ALOGD("Waiting for a sensor-client to connect...");
        newClientFd = accept(mSocketServerFd, (struct sockaddr *)&addr, &alen);
        if (newClientFd < 0) {
            ALOGE("Failed to accept new sensor-client. error: %s",
                    strerror(errno));
        } else {
            if (mClientFd > 0) {
                ALOGE("Close previours sensor-client(%d)", mClientFd);
                close(mClientFd);
                mClientFd = -1;
            }
            mClientFd = newClientFd;
            ALOGD("New sensor-client(%d) connected to server.", mClientFd);
            shared::isClientConnected = true;
            OnAccept();
            shared::isClientConnected = false;
        }
    }
    ALOGD("Exited: %s, %s(%d)", __func__, mSocketServerFile, mClientFd);
    close(mClientFd);
    mClientFd = -1;
    close(mSocketServerFd);
    mSocketServerFd = -1;
    return true;
}


int SensorSocketServerThread::OnAccept() {
    int32_t rlen = 0;
    struct sensor_info rData;
    while (true) {
        memset(&rData, 0, sizeof(struct sensor_info));

        shared::sensorLock.lock();
        rlen = recv(mClientFd, reinterpret_cast<void *> (&rData),
                sizeof(struct sensor_info), 0);
        if (rlen > 0) {
            OnReceiveData(&rData, rlen);
        }
        shared::sensorLock.unlock();

        if (rlen == 0) {
            ALOGE("Client disconnected, ret = %d", rlen);
            break;
        }
    }
    return -1;
}

int SensorSocketServerThread::OnReceiveData(struct sensor_info *pData,
        int rcvLen) {
    int info_size = sizeof(struct sensor_info);
    if (rcvLen != info_size) {
        ALOGE("receved data size mismatch (%d, %d), retrying..",
                rcvLen, info_size);
        return -1;
    }

    /* discard data if sensor not activated */
    if (shared::status[pData->type] == false)
        return -1;

    switch (pData->type) {
        case ACCEL:
            ALOGD("OnReceive accel: (%d, %d, %d)",
                    pData->data[0], pData->data[1], pData->data[2]);
            break;
        case GYRO:
            ALOGD("OnReceive gyro: (%d, %d, %d)",
                    pData->data[0], pData->data[1], pData->data[2]);
            break;
        case MAGN:
            ALOGD("OnReceive magn: (%d, %d, %d)",
                    pData->data[0], pData->data[1], pData->data[2]);
            break;
        default:
            ALOGD("OnReceive sensortype(%d) not supported. (%d, %d, %d)",
                    pData->type,
                    pData->data[0], pData->data[1], pData->data[2]);
            return -1;
    }
    memcpy(&shared::sensorData[pData->type], pData, info_size);
    return rcvLen;
}

}  // namespace android
