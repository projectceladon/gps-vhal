# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifeq ($(TARGET_USE_CAMERA_VHAL), true)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MULTILIB := 64

camera_vhal_module_relative_path := hw
camera_vhal_cflags := -fno-short-enums -DREMOTE_HARDWARE
camera_vhal_cflags += -Wno-unused-parameter -Wno-missing-field-initializers
camera_vhal_clang_flags := -Wno-c++11-narrowing -Werror -Wno-unknown-pragmas

ifeq ($(BOARD_USES_GRALLOC1), true)
camera_vhal_cflags += -DUSE_GRALLOC1
endif

camera_vhal_shared_libraries := \
    libbinder \
    libexif \
    liblog \
    libutils \
    libcutils \
    libEGL \
    libGLESv1_CM \
    libGLESv2 \
    libui \
    libdl \
    libjpeg \
    libcamera_metadata \
    libhardware \
    libsync \
    libavcodec    \
    libavdevice   \
    libavfilter   \
    libavformat   \
    libavutil     \
    libswresample \
    libswscale

camera_vhal_static_libraries := \
	android.hardware.camera.common@1.0-helper \
	libyuv_static

camera_vhal_c_includes := external/libjpeg-turbo \
	external/libexif \
	external/libyuv/files/include \
	frameworks/native/include/media/hardware \
	device/generic/goldfish/include \
	device/generic/goldfish-opengl/system/OpenglSystemCommon \
	hardware/libhardware/modules/gralloc \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/prebuilts/ffmpeg-4.1.5/android-x86_64/include \
	$(call include-path-for, camera)

camera_vhal_src := \
	src/VirtualCameraHal.cpp \
	src/VirtualCameraFactory.cpp \
	src/VirtualBaseCamera.cpp \
	src/Converters.cpp \
	src/NV21JpegCompressor.cpp \
	src/fake-pipeline2/Scene.cpp \
	src/fake-pipeline2/Sensor.cpp \
	src/fake-pipeline2/JpegCompressor.cpp \
	src/VirtualCamera3.cpp \
	src/VirtualFakeCamera3.cpp \
	src/Exif.cpp \
	src/Thumbnail.cpp \
	src/CameraSocketServerThread.cpp \
	src/CameraSocketCommand.cpp \
	src/CGCodec.cpp

LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := ${camera_vhal_module_relative_path}
LOCAL_CFLAGS := ${camera_vhal_cflags}
LOCAL_CPPFLAGS += -std=c++17
LOCAL_CLANG_CFLAGS += ${camera_vhal_clang_flags}

LOCAL_SHARED_LIBRARIES := ${camera_vhal_shared_libraries}
LOCAL_STATIC_LIBRARIES := ${camera_vhal_static_libraries}
LOCAL_C_INCLUDES += ${camera_vhal_c_includes}
LOCAL_SRC_FILES := ${camera_vhal_src}

LOCAL_EXPORT_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/prebuilts/ffmpeg-4.1.5/android-x86_64/include \

# to support platfrom build system
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_EXPORT_C_INCLUDES)

LOCAL_MODULE := camera.$(TARGET_PRODUCT)

include $(BUILD_SHARED_LIBRARY)

# Build all subdirectories #####################################################
include $(call all-makefiles-under,$(LOCAL_PATH))
endif # TARGET_USE_CAMERA_VHAL
