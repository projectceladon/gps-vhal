ifeq ($(USE_SENSOR_VHAL), true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := sensors.$(TARGET_PRODUCT)

LOCAL_PROPRIETARY_MODULE := true

LOCAL_MODULE_RELATIVE_PATH := hw

#LOCAL_CFLAGS += -Denable_debug_logs
LOCAL_CFLAGS += -DLOG_TAG=\"sensorHal\"
LOCAL_CFLAGS += -Wno-error
LOCAL_CFLAGS += -D_POSIX_C_SOURCE=200809

LOCAL_SRC_FILES := sensor_hal.cpp \
                   remote_sensors.cpp

LOCAL_C_INCLUDES := sensor_interfaces.h

LOCAL_SHARED_LIBRARIES := liblog libc libdl libxml2 libcutils libutils
LOCAL_HEADER_LIBRARIES += libutils_headers libhardware_headers
LOCAL_CFLAGS += -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function
include $(BUILD_SHARED_LIBRARY)
endif
