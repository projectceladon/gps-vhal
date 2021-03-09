/*
** Copyright 2019 Intel Corporation
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef __CG_PROTOCOL_H__
#define __CG_PROTOCOL_H__

#include <stdint.h>
#include <time.h>
#include <memory>
#include <functional>
#include <cstdlib>
#include <cstring>

/**********************************************************************
 * Android cloud gaming message protocol
 *
 * package format:
 *
 * 1)MESSAGE PACKAGE   (48B + 16B + ...)
 *      acgmsg_header_t --- acgmsg_message_header_t  --- message payload
 *
 * 2)CONFIGURE PACKAGE (48B + 16B + ...)
 *      acgmsg_header_t --- acgmsg_config_header_t   --- config payload
 *
 * 3)CONNECTION PACKAGE (48B + 48B)
 *      acgmsg_header_t --- acgmsg_connection_t
 *
 * 4)HEARTBEAT PACKAGE  (48B + 16B)
 *      acg_msg_header  --- acgmsg_heartbeat_t
 *
 * *********************************************************************/
/*********************************************
 *
 *  Android cloud Gaming message header define
 *
 ********************************************/

#pragma region Android Cloud Gaming header define

#define ACG_MAGIC 0x41434701  // 'ACG rev1'

typedef int32_t acg_enum;

typedef enum _acgmsg_terminal_t {
    ACG_TERMINAL_INPUT_SERVICE = 0x00,
    ACG_TERMINAL_CLIENT = 0x01,
    ACG_TERMINAL_PROXY = 0x02,
    ACG_TERMINAL_MEDIA_SERVICE = 0x03
} acgmsg_terminal_t;

// Enum for terminal type legacy compatible
#define TERMINAL_SERVICE ACG_TERMINAL_INPUT_SERVICE
#define TERMINAL_CLIENT ACG_TERMINAL_CLIENT
#define TERMINAL_PROXY ACG_TERMINAL_PROXY
#define TERMINAL_MEDIA ACG_TERMINAL_MEDIA_SERVICE

typedef enum _acgmsg_header_type_t {
    ACG_TYPE_UNKNOWN = 0x0,
    ACG_TYPE_CONNECTION = 0x10,
    ACG_TYPE_HEARTBEAT = 0x11,
    ACG_TYPE_CONFIG = 0x12,
    ACG_TYPE_MESSAGE = 0x13,
} acgmsg_header_type_t;

// Enum for header type legacy compatible
#define TYPE_UNKNOWN ACG_TYPE_UNKNOWN
#define TYPE_CONNECTION ACG_TYPE_CONNECTION
#define TYPE_HEARTBEAT ACG_TYPE_HEARTBEAT
#define TYPE_CONFIG ACG_TYPE_CONFIG
#define TYPE_MESSAGE ACG_TYPE_MESSAGE

typedef struct _acgmsg_header_t {
    uint32_t msg_magic;
    uint32_t msg_size;         // size of this message including payload
    acg_enum msg_type;         // acgmsg_header_type_t
    acg_enum msg_source;       // acgmsg_terminal_t
    acg_enum msg_destination;  // acgmsg_terminal_t
    int64_t timestamp;
    uint32_t ack_required;
    uint32_t instance_id;

    uint32_t reserved[3];
} acgmsg_header_t;
#pragma endregion

/*********************************************
 *
 *  Android cloud Gaming payload define
 *
 ********************************************/
#pragma region Android Android cloud Gaming payload define
// ACG_TYPE_CONNECTION payload
typedef struct _acgmsg_connection_t {
    uint32_t heartbeat_enabled;
    int32_t port;  // rtsp port for client, audio port for proxy
    uint8_t key[16];
    uint8_t uuid[16];

    uint32_t reserved[2];
} acgmsg_connection_t;

// ACG_TYPE_HEARTBEAT payload
typedef struct _acgmsg_heartbeat_t {
    uint32_t index;
    uint32_t interval;

    uint32_t reserved[2];
} acgmsg_heartbeat_t;

typedef enum _acgmsg_config_type_t {
    ACG_CLIENT_CONFIG_SERVER_RTSP = 0x20,
    ACG_SERVER_CONFIG_CLINET_SENSOR = 0x21,
    ACG_SERVER_CONFIG_CLIENT_TOUCH = 0x22,
    ACG_SERVER_CONFIG_CLIENT_AUDIO_OUT = 0x23,
    ACG_SERVER_CONFIG_CLIENT_AUDIO_IN = 0x24,
    ACG_SERVER_CONFIG_CLINET_GPS = 0x25,
    ACG_SERVER_CONFIG_CLIENT_INFO = 0x26,
    ACG_SERVER_CONFIG_CLIENT_MONITOR = 0x27,
    ACG_SERVER_CONFIG_CLIENT_CAMERA_IN = 0x28,
    ACG_CLIENT_CONFIG_SERVER_AUDIO_OUT = 0x30
} acgmsg_config_type_t;

// Enum for config type legacy compatible
#define CONFIG_CLIENT_RTSP ACG_CLIENT_CONFIG_SERVER_RTSP
#define CONFIG_SERVER_SENSOR ACG_SERVER_CONFIG_CLINET_SENSOR
#define CONFIG_SERVER_TOUCH ACG_SERVER_CONFIG_CLIENT_TOUCH
#define CONFIG_SERVER_AUDIO_OUT ACG_SERVER_CONFIG_CLIENT_AUDIO_OUT
#define CONFIG_SERVER_AUDIO_IN ACG_SERVER_CONFIG_CLIENT_AUDIO_IN
#define CONFIG_SERVER_GPS ACG_SERVER_CONFIG_CLINET_GPS
#define CONFIG_SERVER_INFO ACG_SERVER_CONFIG_CLIENT_INFO
#define CONFIG_CLIENT_AUDIO_OUT ACG_CLIENT_CONFIG_SERVER_AUDIO_OUT

// ACG_TYPE_CONFIG payload header
typedef struct _acgmsg_config_header_t {
    uint32_t config_size;
    acg_enum config_type;  // acgmsg_config_type_t

    uint32_t reserved[2];
} acgmsg_config_header_t;

typedef enum _acgmsg_message_type_t {
    ACG_MESSAGE_CLIENT_TOUCH = 0x30,
    ACG_MESSAGE_CLIENT_KEYBOARD = 0x31,
    ACG_MESSAGE_CLIENT_MOUSEKEY = 0x32,
    ACG_MESSAGE_CLIENT_MOUSEMOTION = 0x33,
    ACG_MESSAGE_CLIENT_MOUSEWHEEL = 0x34,
    ACG_MESSAGE_CLIENT_SENSOR = 0x35,
    ACG_MESSAGE_CLIENT_GPS = 0x36,
    ACG_MESSAGE_SERVER_AUDIO_OUT = 0x37,
    ACG_MESSAGE_CLIENT_AUDIO_IN = 0x38,
    ACG_MESSAGE_MEDIA_LATENCY = 0x40,
    ACG_MESSAGE_CLIENT_INFO = 0x41,
    ACG_MESSAGE_CLIENT_GAMEPAD = 0x42,
    ACG_MESSAGE_CLIENT_ACTIVITY = 0x43,
    ACG_MESSAGE_CLIENT_CAMERA_IN = 0x44,
    ACG_MESSAGE_CLIENT_COMMAND = 0x45,
    ACG_MESSAGE_CLIENT_COMMAND_ACK = 0x46
} acgmsg_message_type_t;

// Enum for message type legacy compatible
#define MESSAGE_CLIENT_TOUCH ACG_MESSAGE_CLIENT_TOUCH
#define MESSAGE_CLIENT_KEYBOARD ACG_MESSAGE_CLIENT_KEYBOARD
#define MESSAGE_CLIENT_MOUSEKEY ACG_MESSAGE_CLIENT_MOUSEKEY
#define MESSAGE_CLIENT_MOUSEMOTION ACG_MESSAGE_CLIENT_MOUSEMOTION
#define MESSAGE_CLIENT_MOUSEWHEEL ACG_MESSAGE_CLIENT_MOUSEWHEEL
#define MESSAGE_CLIENT_SENSOR ACG_MESSAGE_CLIENT_SENSOR
#define MESSAGE_CLIENT_GPS ACG_MESSAGE_CLIENT_GPS
#define MESSAGE_SERVER_AUDIO_OUT ACG_MESSAGE_SERVER_AUDIO_OUT
#define MESSAGE_CLIENT_AUDIO_IN ACG_MESSAGE_CLIENT_AUDIO_IN
#define MESSAGE_MEDIA_LATENCY ACG_MESSAGE_MEDIA_LATENCY
#define ACG_MESSAGE_GAMECONTROLLER ACG_MESSAGE_CLIENT_GAMEPAD

// ACG_TYPE_MESSAGE payload header
typedef struct _acgmsg_message_header_t {
    uint32_t message_size;
    acg_enum message_type;  // acgmsg_message_type_t

    uint32_t reserved[2];
} acgmsg_message_header_t;
#pragma endregion

/*********************************************
 *
 *  rtsp package define
 *
 ********************************************/
#pragma region rtsp define
typedef struct _acgmsg_client_config_rtsp_t {
    uint8_t rtsp_url[512];
    int32_t rtsp_port;
    int32_t status;
    int32_t rtpOverTCP;

    uint32_t reserved[5];
} acgmsg_client_config_rtsp_t;
#pragma endregion

/*********************************************
 *
 *  sensor package define and workflow
 *
 *  1) Server Android send ACG_SERVER_CONFIG_CLINET_SENSOR to client APP to start sensor
 *  2) Client APP send ASensorEvent(android/sensor.h) data to Server Android
 *  3) Server Android send ACG_SERVER_CONFIG_CLINET_SENSOR to client APP to stop sensor
 *
 ********************************************/
#pragma region sensor define
typedef enum _acgmsg_sensor_type_t {
    ACG_SENSOR_TYPE_INVALID = -1,
    ACG_SENSOR_TYPE_ACCELEROMETER = 1,
    ACG_SENSOR_TYPE_MAGNETIC_FIELD = 2,
    ACG_SENSOR_TYPE_GYROSCOPE = 4,
    ACG_SENSOR_TYPE_LIGHT = 5,
    ACG_SENSOR_TYPE_PRESSURE = 6,
    ACG_SENSOR_TYPE_PROXIMITY = 8,
    ACG_SENSOR_TYPE_GRAVITY = 9,
    ACG_SENSOR_TYPE_LINEAR_ACCELERATION = 10,
    ACG_SENSOR_TYPE_ROTATION_VECTOR = 11,
    ACG_SENSOR_TYPE_RELATIVE_HUMIDITY = 12,
    ACG_SENSOR_TYPE_AMBIENT_TEMPERATURE = 13,
    ACG_SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED = 14,
    ACG_SENSOR_TYPE_GAME_ROTATION_VECTOR = 15,
    ACG_SENSOR_TYPE_GYROSCOPE_UNCALIBRATED = 16,
    ACG_SENSOR_TYPE_SIGNIFICANT_MOTION = 17,
    ACG_SENSOR_TYPE_STEP_DETECTOR = 18,
    ACG_SENSOR_TYPE_STEP_COUNTER = 19,
    ACG_SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR = 20,
    ACG_SENSOR_TYPE_HEART_RATE = 21,
    ACG_SENSOR_TYPE_POSE_6DOF = 28,
    ACG_SENSOR_TYPE_STATIONARY_DETECT = 29,
    ACG_SENSOR_TYPE_MOTION_DETECT = 30,
    ACG_SENSOR_TYPE_HEART_BEAT = 31,
    ACG_SENSOR_TYPE_ADDITIONAL_INFO = 33,
    ACG_SENSOR_TYPE_LOW_LATENCY_OFFBODY_DETECT = 34,
    ACG_SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED = 35
} acgmsg_sensor_type_t;

typedef enum _acgmsg_sensor_mode_t {
    ACG_SENSOR_MODE_INVALID = -1,
    ACG_SENSOR_MODE_CONTINUOUS = 0,
    ACG_SENSOR_MODE_ON_CHANGE = 1,
    ACG_SENSOR_MODE_ONE_SHOT = 2,
    ACG_SENSOR_MODE_SPECIAL_TRIGGER = 3
} acgmsg_sensor_mode_t;

typedef enum _acgmsg_sensor_accuracy_t {
    ACG_SENSOR_ACCURACY_NO_CONTACT = -1,
    ACG_SENSOR_ACCURACY_UNRELIABLE = 0,
    ACG_SENSOR_ACCURACY_LOW = 1,
    ACG_SENSOR_ACCURACY_MEDIUM = 2,
    ACG_SENSOR_ACCURACY_HIGH = 3
} acgmsg_sensor_accuracy_t;

typedef enum _acgmsg_sensor_status_t {
    ACG_SENSOR_STATUS_INVALID = -1,
    ACG_SENSOR_STATUS_OFF = 0,
    ACG_SENSOR_STATUS_ON = 1,
    ACG_SENSOR_STATUS_TRIGGER = 2
} acgmsg_sensor_status_t;

// ACG_SERVER_CONFIG_CLINET_SENSOR
typedef struct _acgmsg_client_config_sensor_t {
    acg_enum type;      // acgmsg_sensor_type_t
    acg_enum accuracy;  // acgmsg_sensor_accuracy_t
    acg_enum mode;      // acgmsg_sensor_mode_t
    acg_enum status;    // acgmsg_sensor_status_t

    uint32_t reserved[4];
} acgmsg_client_config_sensor_t;

// ACG_MESSAGE_CLIENT_SENSOR payload
typedef struct _acgmsg_client_message_sensor_t {
    acg_enum type;  // acgmsg_sensor_type_t
    int32_t status;
    int32_t sensor_id;
    int32_t payload_size;

    uint32_t reserved[4];
} acgmsg_client_message_sensor_t;
#pragma endregion

/*********************************************
 *
 *  key input: keyboard, mouse and gamepad package define
 *  please refer root/include/uapi/linux/input-event-codes.h for detailed message
 *
 ********************************************/
typedef struct _acgmsg_client_message_key_input_t {
    int32_t type;   // key event type: EV_KEY, EV_ABS, EV_REL
    int32_t code;   // key scancode or axis value
    int32_t value;  // keyup, keydown or axis
    int32_t timestamp;
    uint32_t reserved[4];
} acgmsg_client_message_key_input_t;

// message struct for legacy compatible, will be deprecated in next release
#define acgmsg_client_message_gamecontroller_t acgmsg_client_message_key_input_t

// ACG_MESSAGE_CLIENT_KEYBOARD
typedef struct _acgmsg_client_message_keyboard_t {
    int32_t scancode;
    int32_t keycode;
    acg_enum mode;  // acgmsg_keyboard_mode_t
    uint32_t state;

    uint32_t reserved[4];
} acgmsg_client_message_keyboard_t;

#pragma endregion

/*********************************************
 *
 *  touch package define and workflow
 *
 *  1) When connection is completed, Server Android sends ACG_SERVER_CONFIG_CLIENT_TOUCH to client
 *APP 2) Client APP sends ACG_MESSAGE_CLIENT_TOUCH to Server Android
 *
 ********************************************/
#pragma region touch define
// ACG_SERVER_CONFIG_CLIENT_TOUCH
typedef struct _acgmsg_aic_config_touch_t {
    int32_t max_x;  // max touch x coordinates value, default 32768
    int32_t max_y;  // max touch y coordinates value, default 32768

    uint32_t reserved[2];
} acgmsg_aic_config_touch_t;

// ACG_MESSAGE_CLIENT_TOUCH
typedef struct _acgmsg_client_message_touch_t {
    int32_t touch_id;    //  device id, set to 0 if only one touch device
    int32_t finger_id;   //  finger id, started from 0
    int32_t event_type;  //  0 – touch down,  1 – touch move,  2 – touch up
    int32_t x;  //  touch x coordinates mapping to previous [0, max_x) range, default [0, 32768)
    int32_t y;  //  touch y coordinates mapping to previous [0, max_x) range, default [0, 32768)
    int32_t pressure;   //  touch pressure
    int64_t timestamp;  //  timestamp of touch message generation
} acgmsg_client_message_touch_t;
#pragma endregion

/*********************************************
 *
 *  Audio package define and workflow
 *  Audio OUT:
 *  1) Server Android send ACG_SERVER_CONFIG_CLIENT_AUDIO_OUT to client APP
 *  2) (OPTION) client app send ACG_CLIENT_CONFIG_SERVER_AUDIO_OUT to Server Android, select pcm or
 *opus encode 3) Server Android send ACG_MESSAGE_SERVER_AUDIO_OUT (pcm or opus) data to client app
 *
 *  Audio IN:
 *  1) Server Android send ACG_SERVER_CONFIG_CLIENT_AUDIO_IN to client APP to start record
 *  2) client APP send pcm data to client app
 *  3) Server Android send ACG_SERVER_CONFIG_CLIENT_AUDIO_IN to client APP to stop record
 *
 ********************************************/
#pragma region audio define

typedef enum _audio_state_t { AUDIO_CLOSE = 0, AUDIO_OPEN = 1 } audio_state_t;

typedef enum _audio_channel_t { MONO = 1, STEREO = 2 } audio_channel_t;

typedef enum _audio_encode_t { PCM = 0, OPUS = 1 } audio_encode_t;

// ACG_SERVER_CONFIG_CLIENT_AUDIO_OUT ACG_CLIENT_CONFIG_SERVER_AUDIO_OUT
// ACG_SERVER_CONFIG_CLIENT_AUDIO_IN
typedef struct _acgmsg_audio_config_t {
    acg_enum state;        // audio_state_t
    uint32_t sample_rate;  // only support 48000
    acg_enum channel;      // audio_channel_t
    acg_enum encode;       // audio_encode_t
    uint32_t frame_count;  // only support 480

    uint32_t reserved[3];
} acgmsg_audio_config_t;

/*********************************************
 *
 *  server info package define and workflow
 *  1) Android Server send ACG_SERVER_CONFIG_CLIENT_INFO to client APP to notify, like
 *      a) Server screen orientation -  client App can adjust its UI according to server screen
 *orientation
 *********************************************/
typedef struct _acgmsg_info_config_t {
    int32_t orientation;
    uint32_t reserved[3];
} acgmsg_info_config_t;

#pragma endregion

/*********************************************
 *
 *  GPS package define
 *  1) Server Android send ACG_SERVER_CONFIG_CLINET_SENSOR to client APP to start gps
 *  2) client APP send MNEA gps data to AIC
 *  3) Server Android send ACG_SERVER_CONFIG_CLINET_SENSOR to client APP to stop gps
 *********************************************/
#pragma region gps define

// ACG_SERVER_CONFIG_CLINET_GPS
typedef enum _gps_state_t { GPS_STOP = 0, GPS_START = 1 } gps_state_t;

#pragma endregion

/*********************************************
 * Camera package define and workflow
 *********************************************/
#pragma region camera define

typedef enum _camera_state_t {
    CAMERA_CLOSE = 0,
    CAMERA_OPEN,
} camera_state_t;

typedef enum _camera_video_codec_t {
    CAMERA_VIDEO_H264 = 0,
    CAMERA_VIDEO_JPEG,
} camera_video_codec_t;

typedef enum _camera_video_resolution_t {
    CAMERA_RESOLUTION_VGA = 0,  // 640x480
    CAMERA_RESOLUTION_720P,     // 1280x720
    CAMERA_RESOLUTION_1080P,    // 1920x1080
} camera_video_resolution_t;

typedef enum _camera_video_rotation_t {
    CAMERA_ROTATION_0 = 0,  // 0°
    CAMERA_ROTATION_90,     // 90°
    CAMERA_ROTATION_180,    // 180°
    CAMERA_ROTATION_270,    // 270°
} camera_video_rotation_t;

typedef enum _camera_video_mirror_t {
    CAMEAR_MIRROR_NO = 0,  // no mirror
    CAMEAR_MIRROR_X_FLIP,  // horizontal flip
    CAMEAR_MIRROR_Y_FLIP,  // vertial flip
} camera_video_mirror_t;

typedef struct _acgmsg_camera_config_t {
    acg_enum state;       // camera_state_t
    acg_enum codec_type;  // video codec type
    acg_enum resolution;  // raw video resolution
    acg_enum rotation;    // rotation angle
    acg_enum mirror;      // mirror type

    uint32_t reserved[3];
} acgmsg_camera_config_t;

#pragma endregion

/*********************************************
 *
 *  ACG_MESSAGE_CLIENT_COMMAND define
 *
 *********************************************/
#pragma region command channel
const uint32_t MAX_COMMAND_LENGTH = 256 - 1;
const uint32_t MAX_COMMAND_AKC_LENGTH = (64 << 10) - 1;  // 64K

typedef struct _acgmsg_command_t {
    // uint32_t length; //not include the '\0'
    char cmd[MAX_COMMAND_LENGTH + 1];
} acgmsg_command_t;

#pragma endregion

/*********************************************
 *
 *  ACG_MESSAGE_CLIENT_ACTIVITY define
 *
 *********************************************/
#pragma region app activity
const uint32_t MAX_ACTIVITY_LENGTH = 256 - 1;

typedef struct _acgmsg_activity_t {
    char activity[MAX_ACTIVITY_LENGTH + 1];
} acgmsg_activity_t;

#pragma endregion

/*********************************************
 *
 *  ACG_MESSAGE_MEDIA_LATENCY payload define
 *
 *********************************************/
typedef struct acgmsg_media_message_latency_t {
    int32_t channel_id;
    int32_t port;
    int32_t pkt_size;
    int32_t pkt_sizeChange;
    int32_t queue_size;
    int32_t media_type;
    int64_t timestamp;
    struct timeval ps_timestamp;

    uint32_t reserved[4];
} acgmsg_media_message_latency_t;

enum acgmsg_package_length_t {
    LENGTH_MAIN_HEADER = sizeof(acgmsg_header_t),
    LENGTH_PAYLOAD_HEADER = sizeof(acgmsg_message_header_t),
    // LENGTH_PAYLOAD_HEADER  =   sizeof(acgmsg_config_header_t),
    LENGTH_FULL_HEADER = LENGTH_MAIN_HEADER + LENGTH_PAYLOAD_HEADER
};

using namespace std;

class CGBuffer {
public:
    CGBuffer(int length) {
        m_length = length;
        m_ptr = (char*)malloc(length);
    }
    ~CGBuffer() { free(m_ptr); }
    char* m_ptr;
    int m_length;

private:
    CGBuffer(const CGBuffer& cg_buffer);
    CGBuffer& operator=(const CGBuffer&) { return *this; }
};

class CGProtocol {
public:
    CGProtocol(acgmsg_terminal_t source, acgmsg_terminal_t destinate, uint32_t instance_id);
    ~CGProtocol();

    void set_instance_id(int id);
    acgmsg_header_t* get_acgmsg_header();

    unique_ptr<CGBuffer> build_message_packet(acgmsg_message_type_t type, const void* payload_ptr,
                                              int payload_len);
    unique_ptr<CGBuffer> build_config_packet(acgmsg_config_type_t type, const void* payload_ptr,
                                             int payload_len);
    unique_ptr<CGBuffer> build_acg_packet(acgmsg_header_type_t type, const void* payload_ptr,
                                          int payload_len);

    void parse_packet(const void* packet_ptr, int packet_len);

    typedef std::function<void(acgmsg_connection_t* connection_info_ptr)>
        handle_connection_callback_t;
    typedef std::function<void(acgmsg_message_type_t message_ptr, char* ptr, int size)>
        handle_message_callback_t;
    typedef std::function<void(acgmsg_config_type_t config_ptr, char* ptr, int size)>
        handle_config_callback_t;

    void register_handle_connection_callback(handle_connection_callback_t callback);
    void register_handle_message_callback(handle_message_callback_t callback);
    void register_handle_config_callback(handle_config_callback_t callback);

private:
    acg_enum m_source = TERMINAL_PROXY;
    acg_enum m_destinate = TERMINAL_SERVICE;
    uint32_t m_instance_id = 0;
    acgmsg_header_t* m_acgmsg_header = 0;

    handle_connection_callback_t m_connection_callback;
    handle_message_callback_t m_message_callback;
    handle_config_callback_t m_config_callback;

private:
    CGProtocol(const CGProtocol& cg_protocol);
    CGProtocol& operator=(const CGProtocol&) { return *this; }
    int set_acgmsg_header(void* buff_p, acgmsg_header_type_t msg_type, int payload_len);
    void parse_message_packet(const void* data_ptr);
    void parse_config_packet(const void* data_ptr);
};

#endif  //__CG_PROTOCOL_H__
