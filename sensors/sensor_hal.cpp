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

static android::RemoteSensors *sensorInterface = new android::RemoteSensors();

/*******************************************
 * Android SensorHAL wrapper functions   *
 *******************************************/

static int get_sensors_list(struct sensors_module_t* module,
        struct sensor_t const** list) {
    return sensorInterface->get_sensors_list(module, list);
}

static int poll(struct sensors_poll_device_t* dev,
        sensors_event_t* data, int count) {
    return sensorInterface->poll(dev, data, count);
}

static int activate(struct sensors_poll_device_t *dev,
        int handle, int enabled) {
    sensorInterface->activate(dev, handle, enabled);
    return 0;
}

static int setDelay(struct sensors_poll_device_t *dev, int handle, int64_t ns) {
    sensorInterface->setDelay(dev, handle, ns);
    return 0;
}

static int batch(struct sensors_poll_device_1* dev,
        int sensor_handle, int flags,
        int64_t sampling_period_ns, int64_t max_report_latency_ns) {
    sensorInterface->batch(dev, sensor_handle, flags,
            sampling_period_ns, max_report_latency_ns);
    return 0;
}

static int flush(struct sensors_poll_device_1* dev, int handle) {
    sensorInterface->flush(dev, handle);
    return 0;
}

/* Nothing to be cleared on close */
static int close(struct hw_device_t *dev) {
    sensorInterface->close(dev);
    return 0;
}

static int open_sensors(const struct hw_module_t* module,
        const char* id, struct hw_device_t** device) {
    UNUSED(id);
    static struct sensors_poll_device_1 dev;

    dev.common.tag = HARDWARE_DEVICE_TAG;
    dev.common.version = SENSORS_DEVICE_API_VERSION_1_3;
    dev.common.module = const_cast<hw_module_t*>(module);
    dev.common.close = close;

    dev.activate = activate;
    dev.poll = poll;
    dev.batch = batch;
    dev.flush = flush;

    *device = &dev.common;
    return 0;
}

static struct hw_module_methods_t sensors_module_methods = {
    .open = open_sensors
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 3,
        .id = SENSORS_HARDWARE_MODULE_ID,
        .name = "Intel sensorhal module",
        .author = "Intel India",
        .methods = &sensors_module_methods,
        .dso = 0,
        .reserved = {},
    },
    .get_sensors_list = get_sensors_list,
};
