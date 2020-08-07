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

# Emulator camera module########################################################

emulator_camera_module_relative_path := hw
emulator_camera_cflags := -fno-short-enums -DREMOTE_HARDWARE
emulator_camera_cflags += -Wno-unused-parameter -Wno-missing-field-initializers 
emulator_camera_clang_flags := -Wno-c++11-narrowing -Werror  
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
    libhardware

emulator_camera_static_libraries := \
	android.hardware.camera.common@1.0-helper \
	libyuv_static

emulator_camera_c_includes := external/libjpeg-turbo \
	external/libexif \
	external/libyuv/files/include \
	frameworks/native/include/media/hardware \
	$(LOCAL_PATH)/../../../device/generic/goldfish/include \
	$(LOCAL_PATH)/../../../device/generic/goldfish-opengl/system/OpenglSystemCommon \
	$(LOCAL_PATH)/../../../hardware/libhardware/modules/gralloc \
	$(call include-path-for, camera)

emulator_camera_src := \
	VirtualCameraHal.cpp \
	VirtualCameraFactory.cpp \
	VirtualCameraHotplugThread.cpp \
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

# Virtual camera - goldfish / vbox_x86 build###################################

LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := ${emulator_camera_module_relative_path}
LOCAL_CFLAGS := ${emulator_camera_cflags}
LOCAL_CLANG_CFLAGS += ${emulator_camera_clang_flags}

LOCAL_SHARED_LIBRARIES := ${emulator_camera_shared_libraries}
LOCAL_STATIC_LIBRARIES := ${emulator_camera_static_libraries}
LOCAL_C_INCLUDES += ${emulator_camera_c_includes}
LOCAL_SRC_FILES := ${emulator_camera_src}

LOCAL_MODULE := camera.$(TARGET_PRODUCT)

include $(BUILD_SHARED_LIBRARY)

# Build all subdirectories #####################################################
include $(call all-makefiles-under,$(LOCAL_PATH))
endif # TARGET_USE_CAMERA_VHAL
