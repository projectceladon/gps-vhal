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

#ifeq ($(TARGET_USE_CAMERA_VHAL), true)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MULTILIB := 64

# Emulator camera module########################################################

emulator_camera_module_relative_path := hw
emulator_camera_cflags := -fno-short-enums -DREMOTE_HARDWARE
emulator_camera_cflags += -Wno-unused-parameter -Wno-missing-field-initializers
emulator_camera_clang_flags := -Wno-c++11-narrowing -Werror -Wno-unknown-pragmas

ifeq ($(BOARD_USES_GRALLOC1), true)
emulator_camera_cflags += -DUSE_GRALLOC1
endif

emulator_camera_shared_libraries := \
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

emulator_camera_static_libraries := \
	android.hardware.camera.common@1.0-helper \
	libyuv_static

emulator_camera_c_includes := external/libjpeg-turbo \
	external/libexif \
	external/libyuv/files/include \
	frameworks/native/include/media/hardware \
	device/generic/goldfish/include \
	device/generic/goldfish-opengl/system/OpenglSystemCommon \
	hardware/libhardware/modules/gralloc \
	$(LOCAL_PATH)/cg-codec/include \
	$(LOCAL_PATH)/cg-codec/prebuilts/ffmpeg-4.1.5/android-x86_64/include \
	$(call include-path-for, camera)

emulator_camera_src := \
	VirtualCameraHal.cpp \
	VirtualCameraFactory.cpp \
	VirtualBaseCamera.cpp \
	VirtualCamera.cpp \
	VirtualCameraDevice.cpp \
	VirtualRemoteCamera.cpp \
	VirtualRemoteCameraDevice.cpp \
	VirtualFakeCamera.cpp \
	VirtualFakeCameraDevice.cpp \
	VirtualFakeRotatingCameraDevice.cpp \
	Converters.cpp \
	PreviewWindow.cpp \
	CallbackNotifier.cpp \
	RemoteClient.cpp \
	JpegCompressor.cpp \
	VirtualCamera2.cpp \
	VirtualFakeCamera2.cpp \
	VirtualRemoteCamera2.cpp \
	fake-pipeline2/Scene.cpp \
	fake-pipeline2/Sensor.cpp \
	fake-pipeline2/JpegCompressor.cpp \
	VirtualCamera3.cpp \
	VirtualFakeCamera3.cpp \
	VirtualRemoteCamera3.cpp \
	remote-pipeline3/RemoteSensor.cpp \
	Exif.cpp \
	Thumbnail.cpp \
	WorkerThread.cpp \
	CameraSocketServerThread.cpp \
	CameraSocketCommand.cpp \
	cg-codec/src/cg_codec.cpp \
	cg-codec/src/cg_protocol.cpp \
	cg-codec/src/cg_timelog.cpp

# Virtual camera - goldfish / vbox_x86 build###################################

LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := ${emulator_camera_module_relative_path}
LOCAL_CFLAGS := ${emulator_camera_cflags}
LOCAL_CPPFLAGS += -std=c++17
LOCAL_CLANG_CFLAGS += ${emulator_camera_clang_flags}

LOCAL_SHARED_LIBRARIES := ${emulator_camera_shared_libraries}
LOCAL_STATIC_LIBRARIES := ${emulator_camera_static_libraries}
LOCAL_C_INCLUDES += ${emulator_camera_c_includes}
LOCAL_SRC_FILES := ${emulator_camera_src}

LOCAL_EXPORT_C_INCLUDES := \
	$(LOCAL_PATH)/cg-codec/include \
	$(LOCAL_PATH)/cg-codec/prebuilts/ffmpeg-4.1.5/android-x86_64/include \

# to support platfrom build system
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_EXPORT_C_INCLUDES)

LOCAL_MODULE := camera.$(TARGET_PRODUCT)

include $(BUILD_SHARED_LIBRARY)

# Build all subdirectories #####################################################
#include $(call all-makefiles-under,$(LOCAL_PATH))
#endif # TARGET_USE_CAMERA_VHAL
