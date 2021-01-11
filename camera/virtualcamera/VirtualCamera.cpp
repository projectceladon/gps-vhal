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
 * Contains implementation of a class VirtualCamera that encapsulates
 * functionality common to all virtual cameras ("fake", "webcam", "video file",
 * etc.). Instances of this class (for each virtual camera) are created during
 * the construction of the VirtualCameraFactory instance. This class serves as
 * an entry point for all camera API calls that defined by camera_device_ops_t
 * API.
 */

#define LOG_NDEBUG 0
#define LOG_TAG "VirtualCamera_Camera"
#include <log/log.h>
#include <stdio.h>
#include "VirtualCamera.h"
//#include "VirtualFakeCameraDevice.h"
#include "Converters.h"

/* Defines whether we should trace parameter changes. */
#define DEBUG_PARAM 1

namespace android {

static const char *kValidFocusModes[] = {
    CameraParameters::FOCUS_MODE_AUTO,
    CameraParameters::FOCUS_MODE_INFINITY,
    CameraParameters::FOCUS_MODE_MACRO,
    CameraParameters::FOCUS_MODE_FIXED,
    CameraParameters::FOCUS_MODE_EDOF,
    CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO,
    CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE,
};

#if DEBUG_PARAM
/* Calculates and logs parameter changes.
 * Param:
 *  current - Current set of camera parameters.
 *  new_par - String representation of new parameters.
 */
static void PrintParamDiff(const CameraParameters &current, const char *new_par);
#else
#define PrintParamDiff(current, new_par) (void(0))
#endif /* DEBUG_PARAM */

/*
 * Check if a given string |value| equals at least one of the strings in |list|
 */
template <size_t N>
static bool IsValueInList(const char *value, const char *const (&list)[N]) {
    for (size_t i = 0; i < N; ++i) {
        if (strcmp(value, list[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool StringsEqual(const char *str1, const char *str2) {
    if (str1 == nullptr && str2 == nullptr) {
        return true;
    }
    if (str1 == nullptr || str2 == nullptr) {
        return false;
    }
    return strcmp(str1, str2) == 0;
}

static bool GetFourCcFormatFromCameraParam(const char *fmt_str, uint32_t *fmt_val) {
    if (strcmp(fmt_str, CameraParameters::PIXEL_FORMAT_YUV420P) == 0) {
        // Despite the name above this is a YVU format, specifically YV12
        *fmt_val = V4L2_PIX_FMT_YVU420;
        return true;
    } else if (strcmp(fmt_str, CameraParameters::PIXEL_FORMAT_RGBA8888) == 0) {
        *fmt_val = V4L2_PIX_FMT_RGB32;
        return true;
    } else if (strcmp(fmt_str, CameraParameters::PIXEL_FORMAT_YUV420SP) == 0) {
        *fmt_val = V4L2_PIX_FMT_NV21;
        return true;
    }
    return false;
}

VirtualCamera::VirtualCamera(int cameraId, struct hw_module_t *module)
    : VirtualBaseCamera(cameraId, HARDWARE_DEVICE_API_VERSION(1, 0), &common, module),
      mPreviewWindow(),
      mCallbackNotifier() {
    /* camera_device v1 fields. */
    common.close = VirtualCamera::close;
    ops = &mDeviceOps;
    priv = this;
}

VirtualCamera::~VirtualCamera() {}

/****************************************************************************
 * Public API
 ***************************************************************************/

status_t VirtualCamera::Initialize(const char *device_name, const char *frame_dims,
                                   const char *facing_dir) {
    /* Preview formats supported by this HAL. */
    char preview_formats[1024];
    snprintf(preview_formats, sizeof(preview_formats), "%s,%s,%s",
             CameraParameters::PIXEL_FORMAT_YUV420SP, CameraParameters::PIXEL_FORMAT_YUV420P,
             CameraParameters::PIXEL_FORMAT_RGBA8888);

    /*
     * Fake required parameters.
     */

    mParameters.set(CameraParameters::KEY_RECORDING_HINT, CameraParameters::FALSE);
    mParameters.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, "320x240,0x0");

    mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, "320");
    mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, "240");
    mParameters.set(CameraParameters::KEY_JPEG_QUALITY, "90");
    // Camera values for a Logitech B910 HD Webcam
    //     Focal length: 4.90 mm (from specs)
    //     Horizontal view angle: 61 degrees for 4:3 sizes,
    //         70 degrees for 16:9 sizes (empirical)
    //     Vertical view angle: 45.8 degrees (= 61 * 3 / 4)
    // (The Mac has only "4:3" image sizes; the correct angle
    //  is 51.0 degrees. [MacBook Pro (Retina, 15-inch, Mid 2014)])
    mParameters.set(CameraParameters::KEY_FOCAL_LENGTH, "4.90");
    mParameters.set(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, "61.0");
    mParameters.set(CameraParameters::KEY_VERTICAL_VIEW_ANGLE, "45.8");
    mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, "90");

    /* Preview format settings used here are related to panoramic view only. It's
     * not related to the preview window that works only with RGB frames, which
     * is explicitly stated when set_buffers_geometry is called on the preview
     * window object. */
    mParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, preview_formats);
    mParameters.setPreviewFormat(CameraParameters::PIXEL_FORMAT_YUV420SP);

    /* We don't rely on the actual frame rates supported by the camera device,
     * since we will emulate them through timeouts in the virtual camera device
     * worker thread. */
    mParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, "30,24,20,15,10,5");
    mParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(30000,30000)");
    mParameters.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "30000,30000");
    mParameters.setPreviewFrameRate(30);

    /* Only PIXEL_FORMAT_YUV420P is accepted by video framework in emulator! */
    mParameters.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT,
                    CameraParameters::PIXEL_FORMAT_YUV420P);
    mParameters.set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS,
                    CameraParameters::PIXEL_FORMAT_JPEG);
    mParameters.setPictureFormat(CameraParameters::PIXEL_FORMAT_JPEG);

    /* Set exposure compensation. */
    mParameters.set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, "6");
    mParameters.set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, "-6");
    mParameters.set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, "0.5");
    mParameters.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, "0");

    /* Sets the white balance modes and the device-dependent scale factors. */
    char supported_white_balance[1024];
    snprintf(supported_white_balance, sizeof(supported_white_balance), "%s,%s,%s,%s",
             CameraParameters::WHITE_BALANCE_AUTO, CameraParameters::WHITE_BALANCE_INCANDESCENT,
             CameraParameters::WHITE_BALANCE_DAYLIGHT, CameraParameters::WHITE_BALANCE_TWILIGHT);
    mParameters.set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE, supported_white_balance);
    mParameters.set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
    getCameraDevice()->initializeWhiteBalanceModes(CameraParameters::WHITE_BALANCE_AUTO, 1.0f,
                                                   1.0f);
    getCameraDevice()->initializeWhiteBalanceModes(CameraParameters::WHITE_BALANCE_INCANDESCENT,
                                                   1.38f, 0.60f);
    getCameraDevice()->initializeWhiteBalanceModes(CameraParameters::WHITE_BALANCE_DAYLIGHT, 1.09f,
                                                   0.92f);
    getCameraDevice()->initializeWhiteBalanceModes(CameraParameters::WHITE_BALANCE_TWILIGHT, 0.92f,
                                                   1.22f);
    getCameraDevice()->setWhiteBalanceMode(CameraParameters::WHITE_BALANCE_AUTO);

    /* Set suported antibanding values */
    mParameters.set(CameraParameters::KEY_SUPPORTED_ANTIBANDING,
                    CameraParameters::ANTIBANDING_AUTO);
    mParameters.set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_AUTO);

    /* Set control effect mode
     * Bug: 30862244
     * */
    mParameters.set(CameraParameters::KEY_SUPPORTED_EFFECTS, CameraParameters::EFFECT_NONE);
    mParameters.set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_NONE);

    /* Set focus distances for "near,optimal,far" */
    mParameters.set(CameraParameters::KEY_FOCUS_DISTANCES, "Infinity,Infinity,Infinity");

    /* Not supported features
     */
    mParameters.set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES,
                    CameraParameters::FOCUS_MODE_FIXED);
    mParameters.set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_FIXED);

    return NO_ERROR;
}

void VirtualCamera::onNextFrameAvailable(nsecs_t timestamp, VirtualCameraDevice *camera_dev) {
    /* Notify the preview window first. */
    mPreviewWindow.onNextFrameAvailable(timestamp, camera_dev);

    /* Notify callback notifier next. */
    mCallbackNotifier.onNextFrameAvailable(timestamp, camera_dev);
}

void VirtualCamera::onCameraDeviceError(int err) {
    /* Errors are reported through the callback notifier */
    mCallbackNotifier.onCameraDeviceError(err);
}

void VirtualCamera::setTakingPicture(bool takingPicture) {
    mCallbackNotifier.setTakingPicture(takingPicture);
}
/****************************************************************************
 * Camera API implementation.
 ***************************************************************************/

status_t VirtualCamera::connectCamera(hw_device_t **device) {
    ALOGV("%s", __FUNCTION__);

    status_t res = EINVAL;
    VirtualCameraDevice *const camera_dev = getCameraDevice();
    ALOGE_IF(camera_dev == NULL, "%s: No camera device instance.", __FUNCTION__);

    if (camera_dev != NULL) {
        /* Connect to the camera device. */
        res = getCameraDevice()->connectDevice();
        if (res == NO_ERROR) {
            *device = &common;
        }
    }

    return -res;
}

status_t VirtualCamera::closeCamera() {
    ALOGV("%s", __FUNCTION__);

    return cleanupCamera();
}

status_t VirtualCamera::getCameraInfo(struct camera_info *info) {
    ALOGV("%s", __FUNCTION__);

    const char *valstr = NULL;

    valstr = mParameters.get(VirtualCamera::FACING_KEY);
    if (valstr != NULL) {
        if (strcmp(valstr, VirtualCamera::FACING_FRONT) == 0) {
            info->facing = CAMERA_FACING_FRONT;
        } else if (strcmp(valstr, VirtualCamera::FACING_BACK) == 0) {
            info->facing = CAMERA_FACING_BACK;
        }
    } else {
        info->facing = CAMERA_FACING_BACK;
    }

    valstr = mParameters.get(VirtualCamera::ORIENTATION_KEY);
    if (valstr != NULL) {
        info->orientation = atoi(valstr);
    } else {
        info->orientation = 0;
    }

    return VirtualBaseCamera::getCameraInfo(info);
}

void VirtualCamera::autoFocusComplete() { mCallbackNotifier.autoFocusComplete(); }

status_t VirtualCamera::setPreviewWindow(struct preview_stream_ops *window) {
    /* Callback should return a negative errno. */
    return -mPreviewWindow.setPreviewWindow(window, mParameters.getPreviewFrameRate());
}

void VirtualCamera::setCallbacks(camera_notify_callback notify_cb, camera_data_callback data_cb,
                                 camera_data_timestamp_callback data_cb_timestamp,
                                 camera_request_memory get_memory, void *user) {
    mCallbackNotifier.setCallbacks(notify_cb, data_cb, data_cb_timestamp, get_memory, user);
}

void VirtualCamera::enableMsgType(int32_t msg_type) { mCallbackNotifier.enableMessage(msg_type); }

void VirtualCamera::disableMsgType(int32_t msg_type) { mCallbackNotifier.disableMessage(msg_type); }

int VirtualCamera::isMsgTypeEnabled(int32_t msg_type) {
    return mCallbackNotifier.isMessageEnabled(msg_type);
}

status_t VirtualCamera::startPreview() {
    /* Callback should return a negative errno. */
    return -doStartPreview();
}

void VirtualCamera::stopPreview() {
    /* The camera client will not pass on calls to set the preview window to
     * NULL if the preview is not enabled. If preview is not enabled the camera
     * client will instead simply destroy the preview window without notifying
     * the HAL. Later on when preview is enabled again that means the HAL will
     * attempt to use the old, destroyed window which will cause a crash.
     * Instead we need to clear the preview window here, the client will set
     * a preview window when needed. The preview window is cleared here instead
     * of inside doStopPreview to prevent the window from being cleared when
     * restarting the preview because of a parameter change. */
    mPreviewWindow.setPreviewWindow(nullptr, 0);

    doStopPreview();
}

int VirtualCamera::isPreviewEnabled() { return mPreviewWindow.isPreviewEnabled(); }

status_t VirtualCamera::storeMetaDataInBuffers(int enable) {
    /* Callback should return a negative errno. */
    return mCallbackNotifier.storeMetaDataInBuffers(enable);
}

status_t VirtualCamera::startRecording() {
    /* This callback should return a negative errno, hence all the negations */
    if (!mPreviewWindow.isPreviewEnabled()) {
        ALOGE("%s: start recording without preview enabled", __FUNCTION__);
        return INVALID_OPERATION;
    }
    int frameRate = mParameters.getPreviewFrameRate();
    status_t res = mCallbackNotifier.enableVideoRecording(frameRate);
    if (res != NO_ERROR) {
        ALOGE("%s: CallbackNotifier failed to enable video recording", __FUNCTION__);
        stopRecording();
        return -res;
    }
    VirtualCameraDevice *const camera_dev = getCameraDevice();
    if (camera_dev == nullptr || !camera_dev->isStarted()) {
        // No need for restarts, the next preview start will use correct params
        return NO_ERROR;
    }

    // If the camera is running we might have to restart it to accomodate
    // whatever pixel format and frame size the caller wants.
    uint32_t conf_fmt = 0;
    res = getConfiguredPixelFormat(&conf_fmt);
    if (res != NO_ERROR) {
        stopRecording();
        return -res;
    }
    uint32_t cur_fmt = camera_dev->getOriginalPixelFormat();
    int conf_width = -1, conf_height = -1;
    res = getConfiguredFrameSize(&conf_width, &conf_height);
    if (res != NO_ERROR) {
        stopRecording();
        return -res;
    }
    int cur_width = camera_dev->getFrameWidth();
    int cur_height = camera_dev->getFrameHeight();

    if (cur_fmt != conf_fmt || cur_width != conf_width || cur_height != conf_height) {
        // We need to perform a restart to use the new format or size and it
        // has to be an asynchronous restart or this might block if the camera
        // thread is currently delivering a frame.
        if (!camera_dev->requestRestart(conf_width, conf_height, conf_fmt,
                                        false /* takingPicture */, false /* oneBurst */)) {
            ALOGE("%s: Could not restart preview with new pixel format", __FUNCTION__);
            stopRecording();
            return -EINVAL;
        }
    }
    ALOGD("go all the way to the end");
    return NO_ERROR;
}

void VirtualCamera::stopRecording() { mCallbackNotifier.disableVideoRecording(); }

int VirtualCamera::isRecordingEnabled() { return mCallbackNotifier.isVideoRecordingEnabled(); }

void VirtualCamera::releaseRecordingFrame(const void *opaque) {
    mCallbackNotifier.releaseRecordingFrame(opaque);
}

status_t VirtualCamera::setAutoFocus() {
    // Make sure to check that a preview is in progress. Otherwise this will
    // silently fail because no callback will be called until the preview starts
    // which might be never.
    if (!isPreviewEnabled()) {
        return EINVAL;
    }
    VirtualCameraDevice *const camera_dev = getCameraDevice();
    if (camera_dev && camera_dev->isStarted()) {
        return camera_dev->setAutoFocus();
    }
    return EINVAL;
}

status_t VirtualCamera::cancelAutoFocus() {
    // In this case we don't check if a preview is in progress or not. Unlike
    // setAutoFocus this call will not silently fail without the check. If an
    // auto-focus request is somehow pending without having preview enabled this
    // will correctly cancel that pending auto-focus which seems reasonable.
    VirtualCameraDevice *const camera_dev = getCameraDevice();
    if (camera_dev && camera_dev->isStarted()) {
        return camera_dev->cancelAutoFocus();
    }
    return EINVAL;
}

status_t VirtualCamera::takePicture() {
    ALOGV("%s", __FUNCTION__);

    int width, height;
    uint32_t org_fmt;

    /* Collect frame info for the picture. */
    mParameters.getPictureSize(&width, &height);
    const char *pix_fmt = mParameters.getPictureFormat();
    if (!GetFourCcFormatFromCameraParam(pix_fmt, &org_fmt)) {
        // Also check for JPEG here, the function above does not do this since
        // this is very specific to this use case.
        if (strcmp(pix_fmt, CameraParameters::PIXEL_FORMAT_JPEG) == 0) {
            /* We only have JPEG converted for NV21 format. */
            org_fmt = V4L2_PIX_FMT_NV21;
        } else {
            ALOGE("%s: Unsupported pixel format %s", __FUNCTION__, pix_fmt);
            return EINVAL;
        }
    }

    /* Get JPEG quality. */
    int jpeg_quality = mParameters.getInt(CameraParameters::KEY_JPEG_QUALITY);
    if (jpeg_quality <= 0) {
        jpeg_quality = 90; /* Fall back to default. */
    }

    /*
     * Make sure preview is not running, and device is stopped before taking
     * picture.
     */

    VirtualCameraDevice *const camera_dev = getCameraDevice();
    mCallbackNotifier.setJpegQuality(jpeg_quality);
    mCallbackNotifier.setCameraParameters(mParameters);

    ALOGD("Starting camera for picture: %.4s(%s)[%dx%d]", reinterpret_cast<const char *>(&org_fmt),
          pix_fmt, width, height);
    if (mPreviewWindow.isPreviewEnabled()) {
        mPreviewWindow.stopPreview();
        /* If the camera preview is enabled we need to perform an asynchronous
         * restart. A blocking restart could deadlock this thread as it's
         * currently holding the camera client lock and the frame delivery could
         * be stuck on waiting for that lock. If this was synchronous then this
         * thread would in turn get stuck on waiting for the delivery thread. */
        if (!camera_dev->requestRestart(width, height, org_fmt, true /* takingPicture */,
                                        true /* oneBurst */)) {
            return UNKNOWN_ERROR;
        }
        return NO_ERROR;
    } else {
        ALOGE("%s: preview has not been enabled", __FUNCTION__);
        return EINVAL;
    }
}

status_t VirtualCamera::cancelPicture() {
    ALOGV("%s", __FUNCTION__);
    return NO_ERROR;
}

status_t VirtualCamera::setParameters(const char *parms) {
    ALOGV("%s", __FUNCTION__);
    PrintParamDiff(mParameters, parms);

    CameraParameters new_param;
    String8 str8_param(parms);
    new_param.unflatten(str8_param);
    bool restartPreview = false;

    /*
     * Check for new exposure compensation parameter.
     */
    int new_exposure_compensation = new_param.getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION);
    const int min_exposure_compensation =
        new_param.getInt(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION);
    const int max_exposure_compensation =
        new_param.getInt(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION);

    // Checks if the exposure compensation change is supported.
    if ((min_exposure_compensation != 0) || (max_exposure_compensation != 0)) {
        if (new_exposure_compensation > max_exposure_compensation) {
            new_exposure_compensation = max_exposure_compensation;
        }
        if (new_exposure_compensation < min_exposure_compensation) {
            new_exposure_compensation = min_exposure_compensation;
        }

        const int current_exposure_compensation =
            mParameters.getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION);
        if (current_exposure_compensation != new_exposure_compensation) {
            const float exposure_value =
                new_exposure_compensation *
                new_param.getFloat(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP);

            getCameraDevice()->setExposureCompensation(exposure_value);
        }
    }

    const char *new_white_balance = new_param.get(CameraParameters::KEY_WHITE_BALANCE);
    const char *supported_white_balance =
        new_param.get(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE);

    if ((supported_white_balance != NULL) && (new_white_balance != NULL) &&
        (strstr(supported_white_balance, new_white_balance) != NULL)) {
        const char *current_white_balance = mParameters.get(CameraParameters::KEY_WHITE_BALANCE);
        if ((current_white_balance == NULL) ||
            (strcmp(current_white_balance, new_white_balance) != 0)) {
            ALOGV("Setting white balance to %s", new_white_balance);
            getCameraDevice()->setWhiteBalanceMode(new_white_balance);
        }
    }
    int old_frame_rate = mParameters.getPreviewFrameRate();
    int new_frame_rate = new_param.getPreviewFrameRate();
    if (old_frame_rate != new_frame_rate) {
        getCameraDevice()->setPreviewFrameRate(new_frame_rate);
    }

    // Validate focus mode
    const char *focus_mode = new_param.get(CameraParameters::KEY_FOCUS_MODE);
    if (focus_mode && !IsValueInList(focus_mode, kValidFocusModes)) {
        return BAD_VALUE;
    }

    // Validate preview size, if there is no preview size the initial values of
    // the integers below will be preserved thus intentionally failing the test
    int new_preview_width = -1, new_preview_height = -1;
    new_param.getPreviewSize(&new_preview_width, &new_preview_height);
    if (new_preview_width < 0 || new_preview_height < 0) {
        return BAD_VALUE;
    }
    // If the preview size has changed we have to restart the preview to make
    // sure we provide frames of the correct size. The receiver assumes the
    // frame size is correct and will copy all data provided into a buffer whose
    // size is determined by the preview size without checks, potentially
    // causing buffer overruns or underruns if there is a size mismatch.
    int old_preview_width = -1, old_preview_height = -1;
    mParameters.getPreviewSize(&old_preview_width, &old_preview_height);
    if (old_preview_width != new_preview_width || old_preview_height != new_preview_height) {
        restartPreview = true;
    }

    // For the same reasons as with the preview size we have to look for changes
    // in video size and restart the preview if the size has changed.
    int old_video_width = -1, old_video_height = -1;
    int new_video_width = -1, new_video_height = -1;
    mParameters.getVideoSize(&old_video_width, &old_video_height);
    new_param.getVideoSize(&new_video_width, &new_video_height);
    if (old_video_width != new_video_width || old_video_height != new_video_height) {
        restartPreview = true;
    }
    // Restart the preview if the pixel format changes to make sure we serve
    // the selected encoding to the client.
    const char *old_format = mParameters.getPreviewFormat();
    const char *new_format = new_param.getPreviewFormat();
    if (!StringsEqual(old_format, new_format)) {
        restartPreview = true;
    }

    const char *old_hint = mParameters.get(CameraParameters::KEY_RECORDING_HINT);
    const char *new_hint = new_param.get(CameraParameters::KEY_RECORDING_HINT);
    if (!StringsEqual(old_hint, new_hint)) {
        // The recording hint changed, this indicates we transitioned from
        // recording to non-recording or the other way around. We need to look
        // at a new pixel format for this and that requires a restart.
        restartPreview = true;
    }

    mParameters = new_param;

    // Now that the parameters have been assigned check if the preview needs to
    // be restarted. If necessary this will then use the new parameters to set
    // up the preview as requested by the caller.
    if (restartPreview && isPreviewEnabled()) {
        status_t status = doStopPreview();
        if (status != NO_ERROR) {
            ALOGE("%s: Stopping preview failed: %d", __FUNCTION__, status);
            return status;
        }
        status = doStartPreview();
        if (status != NO_ERROR) {
            ALOGE("%s: Starting preview failed: %d", __FUNCTION__, status);
            return status;
        }
    }
    return NO_ERROR;
}

/* A dumb variable indicating "no params" / error on the exit from
 * VirtualCamera::getParameters(). */
static char lNoParam = '\0';
char *VirtualCamera::getParameters() {
    // Read the image size and set the camera's Field of View.
    // These values are valid for a Logitech B910 HD Webcam.
    int width = 0, height = 0;
    mParameters.getPictureSize(&width, &height);
    if (height > 0) {
        if (((double)width / height) < 1.55) {
            // Closer to 4:3 (1.33), set the FOV to 61.0 degrees
            mParameters.set(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, "61.0");
        } else {
            // Closer to 16:9 (1.77), set the FOV to 70.0 degrees
            mParameters.set(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, "70.0");
        }
    }

    String8 params(mParameters.flatten());
    char *ret_str = reinterpret_cast<char *>(malloc(sizeof(char) * (params.length() + 1)));
    memset(ret_str, 0, params.length() + 1);
    if (ret_str != NULL) {
        strncpy(ret_str, params.string(), params.length() + 1);
        return ret_str;
    } else {
        ALOGE("%s: Unable to allocate string for %s", __FUNCTION__, params.string());
        /* Apparently, we can't return NULL fron this routine. */
        return &lNoParam;
    }
}

void VirtualCamera::putParameters(char *params) {
    /* This method simply frees parameters allocated in getParameters(). */
    if (params != NULL && params != &lNoParam) {
        free(params);
    }
}

status_t VirtualCamera::sendCommand(int32_t cmd, int32_t arg1, int32_t arg2) {
    ALOGV("%s: cmd = %d, arg1 = %d, arg2 = %d", __FUNCTION__, cmd, arg1, arg2);

    switch (cmd) {
        case CAMERA_CMD_START_FACE_DETECTION:
        case CAMERA_CMD_STOP_FACE_DETECTION:
            // We do not support hardware face detection so we need to indicate
            // that any attempt to start/stop face detection is invalid
            return BAD_VALUE;
    }
    /* TODO: Future enhancements. */
    return 0;
}

void VirtualCamera::releaseCamera() {
    ALOGV("%s", __FUNCTION__);

    cleanupCamera();
}

status_t VirtualCamera::dumpCamera(int fd) {
    ALOGV("%s", __FUNCTION__);

    /* TODO: Future enhancements. */
    dprintf(fd, "dump camera unimplemented\n");
    return 0;
}

status_t VirtualCamera::getConfiguredPixelFormat(uint32_t *pixelFormat) const {
    const char *pix_fmt = nullptr;
    const char *recordingHint = mParameters.get(CameraParameters::KEY_RECORDING_HINT);
    bool recordingHintOn = recordingHint && strcmp(recordingHint, CameraParameters::TRUE) == 0;
    bool recordingEnabled = mCallbackNotifier.isVideoRecordingEnabled();
    if (recordingHintOn || recordingEnabled) {
        // We're recording a video, use the video pixel format
        pix_fmt = mParameters.get(CameraParameters::KEY_VIDEO_FRAME_FORMAT);
    }
    if (pix_fmt == nullptr) {
        pix_fmt = mParameters.getPreviewFormat();
    }
    if (pix_fmt == nullptr) {
        ALOGE("%s: Unable to obtain configured pixel format", __FUNCTION__);
        return EINVAL;
    }
    /* Convert framework's pixel format to the FOURCC one. */
    if (!GetFourCcFormatFromCameraParam(pix_fmt, pixelFormat)) {
        ALOGE("%s: Unsupported pixel format %s", __FUNCTION__, pix_fmt);
        return EINVAL;
    }
    return NO_ERROR;
}

status_t VirtualCamera::getConfiguredFrameSize(int *outWidth, int *outHeight) const {
    int width = -1, height = -1;
    if (mParameters.get(CameraParameters::KEY_VIDEO_SIZE) != nullptr) {
        mParameters.getVideoSize(&width, &height);
    } else {
        mParameters.getPreviewSize(&width, &height);
    }
    if (width < 0 || height < 0) {
        ALOGE("%s: No frame size configured for camera", __FUNCTION__);
        return EINVAL;
    }
    // Only modify the out parameters once we know we succeeded
    *outWidth = width;
    *outHeight = height;
    return NO_ERROR;
}

/****************************************************************************
 * Preview management.
 ***************************************************************************/

status_t VirtualCamera::doStartPreview() {
    ALOGV("%s", __FUNCTION__);

    VirtualCameraDevice *camera_dev = getCameraDevice();
    if (camera_dev->isStarted()) {
        camera_dev->stopDeliveringFrames();
        camera_dev->stopDevice();
    }

    status_t res = mPreviewWindow.startPreview();
    if (res != NO_ERROR) {
        return res;
    }

    /* Make sure camera device is connected. */
    if (!camera_dev->isConnected()) {
        res = camera_dev->connectDevice();
        if (res != NO_ERROR) {
            mPreviewWindow.stopPreview();
            return res;
        }
    }

    /* Lets see what should we use for frame width, and height. */
    int width, height;
    res = getConfiguredFrameSize(&width, &height);
    if (res != NO_ERROR) {
        mPreviewWindow.stopPreview();
        return res;
    }

    uint32_t org_fmt = 0;
    res = getConfiguredPixelFormat(&org_fmt);
    if (res != NO_ERROR) {
        mPreviewWindow.stopPreview();
        return res;
    }

    camera_dev->setPreviewFrameRate(mParameters.getPreviewFrameRate());
    ALOGD("Starting camera: %dx%d -> %.4s", width, height,
          reinterpret_cast<const char *>(&org_fmt));
    res = camera_dev->startDevice(width, height, org_fmt);
    if (res != NO_ERROR) {
        mPreviewWindow.stopPreview();
        return res;
    }

    res = camera_dev->startDeliveringFrames(false);
    if (res != NO_ERROR) {
        camera_dev->stopDevice();
        mPreviewWindow.stopPreview();
    }

    return res;
}

status_t VirtualCamera::doStopPreview() {
    ALOGV("%s", __FUNCTION__);

    status_t res = NO_ERROR;
    if (mPreviewWindow.isPreviewEnabled()) {
        /* Stop the camera. */
        if (getCameraDevice()->isStarted()) {
            getCameraDevice()->stopDeliveringFrames();
            res = getCameraDevice()->stopDevice();
        }

        if (res == NO_ERROR) {
            /* Disable preview as well. */
            mPreviewWindow.stopPreview();
        }
    }

    return NO_ERROR;
}

/****************************************************************************
 * Private API.
 ***************************************************************************/

status_t VirtualCamera::cleanupCamera() {
    status_t res = NO_ERROR;

    /* If preview is running - stop it. */
    res = doStopPreview();
    if (res != NO_ERROR) {
        return -res;
    }

    /* Stop and disconnect the camera device. */
    VirtualCameraDevice *const camera_dev = getCameraDevice();
    if (camera_dev != NULL) {
        if (camera_dev->isStarted()) {
            camera_dev->stopDeliveringFrames();
            res = camera_dev->stopDevice();
            if (res != NO_ERROR) {
                return -res;
            }
        }
        if (camera_dev->isConnected()) {
            res = camera_dev->disconnectDevice();
            if (res != NO_ERROR) {
                return -res;
            }
        }
    }

    mCallbackNotifier.cleanupCBNotifier();

    /* Re-init the camera settings in case settings were changed */
    Initialize(nullptr, nullptr, nullptr);

    return NO_ERROR;
}

/****************************************************************************
 * Camera API callbacks as defined by camera_device_ops structure.
 *
 * Callbacks here simply dispatch the calls to an appropriate method inside
 * VirtualCamera instance, defined by the 'dev' parameter.
 ***************************************************************************/

int VirtualCamera::set_preview_window(struct camera_device *dev,
                                      struct preview_stream_ops *window) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->setPreviewWindow(window);
}

void VirtualCamera::set_callbacks(struct camera_device *dev, camera_notify_callback notify_cb,
                                  camera_data_callback data_cb,
                                  camera_data_timestamp_callback data_cb_timestamp,
                                  camera_request_memory get_memory, void *user) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->setCallbacks(notify_cb, data_cb, data_cb_timestamp, get_memory, user);
}

void VirtualCamera::enable_msg_type(struct camera_device *dev, int32_t msg_type) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->enableMsgType(msg_type);
}

void VirtualCamera::disable_msg_type(struct camera_device *dev, int32_t msg_type) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->disableMsgType(msg_type);
}

int VirtualCamera::msg_type_enabled(struct camera_device *dev, int32_t msg_type) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->isMsgTypeEnabled(msg_type);
}

int VirtualCamera::start_preview(struct camera_device *dev) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->startPreview();
}

void VirtualCamera::stop_preview(struct camera_device *dev) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->stopPreview();
}

int VirtualCamera::preview_enabled(struct camera_device *dev) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->isPreviewEnabled();
}

int VirtualCamera::store_meta_data_in_buffers(struct camera_device *dev, int enable) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->storeMetaDataInBuffers(enable);
}

int VirtualCamera::start_recording(struct camera_device *dev) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->startRecording();
}

void VirtualCamera::stop_recording(struct camera_device *dev) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->stopRecording();
}

int VirtualCamera::recording_enabled(struct camera_device *dev) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->isRecordingEnabled();
}

void VirtualCamera::release_recording_frame(struct camera_device *dev, const void *opaque) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->releaseRecordingFrame(opaque);
}

int VirtualCamera::auto_focus(struct camera_device *dev) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->setAutoFocus();
}

int VirtualCamera::cancel_auto_focus(struct camera_device *dev) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->cancelAutoFocus();
}

int VirtualCamera::take_picture(struct camera_device *dev) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->takePicture();
}

int VirtualCamera::cancel_picture(struct camera_device *dev) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->cancelPicture();
}

int VirtualCamera::set_parameters(struct camera_device *dev, const char *parms) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->setParameters(parms);
}

char *VirtualCamera::get_parameters(struct camera_device *dev) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return NULL;
    }
    return ec->getParameters();
}

void VirtualCamera::put_parameters(struct camera_device *dev, char *params) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->putParameters(params);
}

int VirtualCamera::send_command(struct camera_device *dev, int32_t cmd, int32_t arg1,
                                int32_t arg2) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->sendCommand(cmd, arg1, arg2);
}

void VirtualCamera::release(struct camera_device *dev) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->releaseCamera();
}

int VirtualCamera::dump(struct camera_device *dev, int fd) {
    VirtualCamera *ec = reinterpret_cast<VirtualCamera *>(dev->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->dumpCamera(fd);
}

int VirtualCamera::close(struct hw_device_t *device) {
    VirtualCamera *ec =
        reinterpret_cast<VirtualCamera *>(reinterpret_cast<struct camera_device *>(device)->priv);
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->closeCamera();
}

/****************************************************************************
 * Static initializer for the camera callback API
 ****************************************************************************/

camera_device_ops_t VirtualCamera::mDeviceOps = {VirtualCamera::set_preview_window,
                                                 VirtualCamera::set_callbacks,
                                                 VirtualCamera::enable_msg_type,
                                                 VirtualCamera::disable_msg_type,
                                                 VirtualCamera::msg_type_enabled,
                                                 VirtualCamera::start_preview,
                                                 VirtualCamera::stop_preview,
                                                 VirtualCamera::preview_enabled,
                                                 VirtualCamera::store_meta_data_in_buffers,
                                                 VirtualCamera::start_recording,
                                                 VirtualCamera::stop_recording,
                                                 VirtualCamera::recording_enabled,
                                                 VirtualCamera::release_recording_frame,
                                                 VirtualCamera::auto_focus,
                                                 VirtualCamera::cancel_auto_focus,
                                                 VirtualCamera::take_picture,
                                                 VirtualCamera::cancel_picture,
                                                 VirtualCamera::set_parameters,
                                                 VirtualCamera::get_parameters,
                                                 VirtualCamera::put_parameters,
                                                 VirtualCamera::send_command,
                                                 VirtualCamera::release,
                                                 VirtualCamera::dump};

/****************************************************************************
 * Common keys
 ***************************************************************************/

const char VirtualCamera::FACING_KEY[] = "prop-facing";
const char VirtualCamera::ORIENTATION_KEY[] = "prop-orientation";
const char VirtualCamera::RECORDING_HINT_KEY[] = "recording-hint";

/****************************************************************************
 * Common string values
 ***************************************************************************/

const char VirtualCamera::FACING_BACK[] = "back";
const char VirtualCamera::FACING_FRONT[] = "front";

/****************************************************************************
 * Parameter debugging helpers
 ***************************************************************************/

#if DEBUG_PARAM
static void PrintParamDiff(const CameraParameters &current, const char *new_par) {
    char tmp[2048];
    const char *wrk = new_par;

    /* Divided with ';' */
    const char *next = strchr(wrk, ';');
    while (next != NULL) {
        snprintf(tmp, sizeof(tmp), "%.*s", (int)(intptr_t)(next - wrk), wrk);
        /* in the form key=value */
        char *val = strchr(tmp, '=');
        if (val != NULL) {
            *val = '\0';
            val++;
            const char *in_current = current.get(tmp);
            if (in_current != NULL) {
                if (strcmp(in_current, val)) {
                    ALOGD("=== Value changed: %s: %s -> %s", tmp, in_current, val);
                }
            } else {
                ALOGD("+++ New parameter: %s=%s", tmp, val);
            }
        } else {
            ALOGW("No value separator in %s", tmp);
        }
        wrk = next + 1;
        next = strchr(wrk, ';');
    }
}
#endif /* DEBUG_PARAM */

}; /* namespace android */
