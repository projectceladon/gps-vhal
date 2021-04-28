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
#ifndef SENSOR_INTERFACES_H_
#define SENSOR_INTERFACES_H_

#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/limits.h>
#include <utils/Log.h>
#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/sensors.h>

#include <utils/RefBase.h>
#include <utils/threads.h>
#include <log/log.h>
#include <mutex>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif

#ifndef UNUSED
#define UNUSED(x)   (void)(x)
#endif

namespace android {

enum sensor_id {ACCEL, GYRO, MAGN, MAX_SENSOR};

struct sensor_info {
    uint32_t type;
    int32_t  data[3];
    uint32_t time_stamp;
    uint32_t frame_count;
};

class SensorInterface {
 public:
    virtual ~SensorInterface() {}

    virtual int get_sensors_list(struct sensors_module_t*,
            struct sensor_t const**) = 0;

    virtual int poll(struct sensors_poll_device_t*,
            sensors_event_t*, int) = 0;

    virtual int activate(struct sensors_poll_device_t*,
            int, int) = 0;

    virtual int setDelay(struct sensors_poll_device_t*,
            int, int64_t) = 0;

    virtual int batch(struct sensors_poll_device_1*,
            int, int, int64_t, int64_t) = 0;

    virtual int flush(struct sensors_poll_device_1*,
            int) = 0;

    virtual int close(struct hw_device_t*) = 0;
};

class SensorSocketServerThread : public Thread {
 private:
    bool mRunning;
    int mClientFd;
    int mSocketServerFd;
    char mSocketServerFile[64];

 public:
    explicit SensorSocketServerThread();
    ~SensorSocketServerThread();
    int OnAccept();
    int OnReceiveData(struct sensor_info*, int);

    virtual bool threadLoop();
};

class RemoteSensors : public SensorInterface {
 public:
    bool is_meta_data_pending;
    sp<SensorSocketServerThread> mSensorInterface;

 private:
    const struct sensor_t sSensorList[MAX_SENSOR] = {
        {"Accelerometer",
            "Intel",
            1,
            ACCEL,
            SENSOR_TYPE_ACCELEROMETER,
            1000,
            0.1,
            0.1,
            1,
            1,
            0,
            "android.sensor.accelerometer",
            "",
            1000,
            SENSOR_FLAG_CONTINUOUS_MODE,
            {},
        },
        {"gyro_3d",
            "Intel",
            1,
            GYRO,
            SENSOR_TYPE_GYROSCOPE,
            10000,
            0.1,
            0.5,
            0,
            1,
            10,
            "android.sensor.gyro_3d",
            "",
            1000,
            SENSOR_FLAG_CONTINUOUS_MODE,
            {},
        },
        {"magn_3d",
            "Intel",
            1,
            MAGN,
            SENSOR_TYPE_MAGNETIC_FIELD,
            12000,
            0.1,
            0.5,
            0,
            0,
            0,
            "android.sensor.magn_3d",
            "",
            10000,
            SENSOR_FLAG_CONTINUOUS_MODE,
            {},
        },
    };

 public:
    explicit RemoteSensors();

    virtual ~RemoteSensors() {}

    virtual int get_sensors_list(struct sensors_module_t*,
            struct sensor_t const**);

    virtual int poll(struct sensors_poll_device_t* dev,
            sensors_event_t*, int);

    virtual int activate(struct sensors_poll_device_t*,
            int, int);

    virtual int setDelay(struct sensors_poll_device_t*,
            int, int64_t);

    virtual int batch(struct sensors_poll_device_1*,
            int, int, int64_t, int64_t);

    virtual int flush(struct sensors_poll_device_1*, int);

    virtual int close(struct hw_device_t*);

    int64_t getTimeStamp(clockid_t);
};

}  // namespace android

#endif  // SENSOR_INTERFACES_H_
