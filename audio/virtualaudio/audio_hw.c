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

#define LOG_TAG "audio_hw_virtual"
// #define LOG_NDEBUG 0

#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <log/log.h>

#include <hardware/audio.h>
#include <hardware/hardware.h>
#include <system/audio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <cutils/properties.h>
#include <cutils/sockets.h>
#include <sys/system_properties.h>

#define STUB_DEFAULT_SAMPLE_RATE 48000
#define STUB_DEFAULT_AUDIO_FORMAT AUDIO_FORMAT_PCM_16_BIT

#define STUB_INPUT_BUFFER_MILLISECONDS 20
#define STUB_INPUT_DEFAULT_CHANNEL_MASK AUDIO_CHANNEL_IN_STEREO

#define STUB_OUTPUT_BUFFER_MILLISECONDS 10
#define STUB_OUTPUT_DEFAULT_CHANNEL_MASK AUDIO_CHANNEL_OUT_STEREO

enum
{
    CMD_OPEN = 0,
    CMD_CLOSE = 1,
    CMD_DATA = 2
};

struct audio_socket_configuration_info
{
    uint32_t sample_rate;
    uint32_t channel_mask;
    uint32_t format;
    uint32_t frame_count;
};

struct audio_socket_info
{
    uint32_t cmd;
    union {
        struct audio_socket_configuration_info asci;
        uint32_t data_size;
    };
};

struct stub_audio_device
{
    struct audio_hw_device device;
};

struct stub_stream_out
{
    struct audio_stream_out stream;
    int64_t last_write_time_us;
    uint32_t sample_rate;
    audio_channel_mask_t channel_mask;
    audio_format_t format;
    size_t frame_count;

    //Audio out socket
    int out_fd;
    pthread_t oss_thread; // out socket server thread
    int oss_exit;         // out socket server thread exit
    int oss_fd;           // out socket server fd
    int out_tcp_port;
};

struct stub_stream_in
{
    struct audio_stream_in stream;
    int64_t last_read_time_us;
    uint32_t sample_rate;
    audio_channel_mask_t channel_mask;
    audio_format_t format;
    size_t frame_count;

    //Audio in socket
    int in_fd;
    pthread_t iss_thread; // in socket server thread
    int iss_exit;         // in socket server thread exit
    int iss_fd;           // iut socket server fd
    int in_tcp_port;
};

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    const struct stub_stream_out *out = (const struct stub_stream_out *)stream;

    ALOGV("out_get_sample_rate: %u", out->sample_rate);
    return out->sample_rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    struct stub_stream_out *out = (struct stub_stream_out *)stream;

    ALOGV("out_set_sample_rate: %d", rate);
    out->sample_rate = rate;
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    const struct stub_stream_out *out = (const struct stub_stream_out *)stream;
    size_t buffer_size = out->frame_count *
                         audio_stream_out_frame_size(&out->stream);

    ALOGV("out_get_buffer_size: %zu", buffer_size);
    return buffer_size;
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream)
{
    const struct stub_stream_out *out = (const struct stub_stream_out *)stream;

    ALOGV("out_get_channels: %x", out->channel_mask);
    return out->channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    const struct stub_stream_out *out = (const struct stub_stream_out *)stream;

    ALOGV("out_get_format: %d", out->format);
    return out->format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    struct stub_stream_out *out = (struct stub_stream_out *)stream;

    ALOGV("out_set_format: %d", format);
    out->format = format;
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    ALOGV("out_standby");
    // out->last_write_time_us = 0; unnecessary as a stale write time has same effect
    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    ALOGV("out_dump");
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    ALOGV("out_set_parameters");
    return 0;
}

static char *out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    ALOGV("out_get_parameters");
    return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    ALOGV("out_get_latency");
    return STUB_OUTPUT_BUFFER_MILLISECONDS;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    ALOGV("out_set_volume: Left:%f Right:%f", left, right);
    return 0;
}

static ssize_t out_write(struct audio_stream_out *stream, const void *buffer,
                         size_t bytes)
{
    ALOGV("out_write: bytes: %zu", bytes);

    struct stub_stream_out *out = (struct stub_stream_out *)stream;
    ssize_t ret = bytes;
    if (bytes > 0)
    {
        if (out->out_fd > 0)
        {
            struct audio_socket_info asi;
            asi.cmd = CMD_DATA;
            asi.data_size = bytes;
            ALOGV("%s asi.data_size: %d\n", __func__, asi.data_size);
            do
            {
                ret = write(out->out_fd, &asi, sizeof(struct audio_socket_info));
            } while (ret < 0 && errno == EINTR);
            if (ret != sizeof(struct audio_socket_info))
                ALOGE("%s: could not notify the audio out client(%d) to receive: ret=%zd: %s.",
                      __FUNCTION__, out->out_fd, ret, strerror(errno));
            else
                ALOGV("%s Notify the audio out client(%d) to receive.", __func__, out->out_fd);

            ret = write(out->out_fd, buffer, bytes);
            if (ret < 0)
            {
                ALOGE("out_write: Fail to write to audio out client(%d) with error(%s)", out->out_fd, strerror(errno));
            }
            else if (ret == 0)
            {
                ALOGW("out_write: audio out client(%d) is closed.", out->out_fd);
            }
            else
            {
                ALOGV("out_write: Write to audio out client. out->out_fd: %d bytes: %zu", out->out_fd, bytes);
                if (bytes != (size_t)ret)
                {
                    ALOGW("out_write: (!^!) ret(%zd) data is written. But bytes(%zu) is expected.", ret, bytes);
                }
            }
        }
        else
        {
            ALOGV("out_write: (->v->) Audio out client is not connected. port(%d) out->out_fd(%d). Return bytes(%zu) directly.", out->out_tcp_port, out->out_fd, bytes);
        }
    }
    /* XXX: fake timing for audio output */
    struct timespec t = {.tv_sec = 0, .tv_nsec = 0};
    clock_gettime(CLOCK_MONOTONIC, &t);
    const int64_t now = (t.tv_sec * 1000000000LL + t.tv_nsec) / 1000;
    const int64_t elapsed_time_since_last_write = now - out->last_write_time_us;
    int64_t sleep_time = bytes * 1000000LL / audio_stream_out_frame_size(stream) /
                             out_get_sample_rate(&stream->common) -
                         elapsed_time_since_last_write;

    if (sleep_time > 0)
    {
        usleep(sleep_time);
    }
    else
    {
        // we don't sleep when we exit standby (this is typical for a real alsa buffer).
        sleep_time = 0;
    }
    out->last_write_time_us = now + sleep_time;
    // last_write_time_us is an approximation of when the (simulated) alsa
    // buffer is believed completely full. The usleep above waits for more space
    // in the buffer, but by the end of the sleep the buffer is considered
    // topped-off.
    //
    // On the subsequent out_write(), we measure the elapsed time spent in
    // the mixer. This is subtracted from the sleep estimate based on frames,
    // thereby accounting for drain in the alsa buffer during mixing.
    // This is a crude approximation; we don't handle underruns precisely.
    return ret;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    *dsp_frames = 0;
    ALOGV("out_get_render_position: dsp_frames: %p", dsp_frames);
    return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    ALOGV("out_add_audio_effect: %p", effect);
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    ALOGV("out_remove_audio_effect: %p", effect);
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
                                        int64_t *timestamp)
{
    *timestamp = 0;
    ALOGV("out_get_next_write_timestamp: %ld", (long int)(*timestamp));
    return -EINVAL;
}

/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    const struct stub_stream_in *in = (const struct stub_stream_in *)stream;

    ALOGV("in_get_sample_rate: %u", in->sample_rate);
    return in->sample_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    struct stub_stream_in *in = (struct stub_stream_in *)stream;

    ALOGV("in_set_sample_rate: %u", rate);
    in->sample_rate = rate;
    return 0;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    const struct stub_stream_in *in = (const struct stub_stream_in *)stream;
    size_t buffer_size = in->frame_count *
                         audio_stream_in_frame_size(&in->stream);

    ALOGV("in_get_buffer_size: %zu", buffer_size);
    return buffer_size;
}

static audio_channel_mask_t in_get_channels(const struct audio_stream *stream)
{
    const struct stub_stream_in *in = (const struct stub_stream_in *)stream;

    ALOGV("in_get_channels: %x", in->channel_mask);
    return in->channel_mask;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    const struct stub_stream_in *in = (const struct stub_stream_in *)stream;

    ALOGV("in_get_format: %d", in->format);
    return in->format;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    struct stub_stream_in *in = (struct stub_stream_in *)stream;

    ALOGV("in_set_format: %d", format);
    in->format = format;
    return 0;
}

static int in_standby(struct audio_stream *stream)
{
    struct stub_stream_in *in = (struct stub_stream_in *)stream;
    in->last_read_time_us = 0;
    return 0;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    return 0;
}

static char *in_get_parameters(const struct audio_stream *stream,
                               const char *keys)
{
    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void *buffer,
                       size_t bytes)
{
    ALOGV("in_read: bytes %zu", bytes);

    struct stub_stream_in *in = (struct stub_stream_in *)stream;
    ssize_t ret = bytes;
    if (bytes > 0)
    {
        if (in->in_fd > 0)
        {
            ret = read(in->in_fd, buffer, bytes);
            if (ret < 0)
            {
                ALOGE("in_read: Fail to read from audio in client(%d) with error (%s)", in->in_fd, strerror(errno));
            }
            else if (ret == 0)
            {
                ALOGE("in_read: Audio in client(%d) is closed.", in->in_fd);
            }
            else
            {
                ALOGV("in_read: Read from port %d in->in_fd %d bytes %zu, ret: %zd", in->in_tcp_port, in->in_fd, bytes, ret);
                if (bytes != (size_t)ret)
                {
                    ALOGW("in_read: (!^!) ret(%zd) data is read. But bytes(%zu) is expected.", ret, bytes);
                }
            }
        }
        else
        {
            ALOGV("in_read: (->v->) Audio in client is not connected. port(%d) in->in_fd(%d). Memset data to 0. Return bytes(%zu) directly.", in->in_tcp_port, in->in_fd, bytes);
            memset(buffer, 0, bytes);
        }
    }
    /* XXX: fake timing for audio input */
    struct timespec t = {.tv_sec = 0, .tv_nsec = 0};
    clock_gettime(CLOCK_MONOTONIC, &t);
    const int64_t now = (t.tv_sec * 1000000000LL + t.tv_nsec) / 1000;

    // we do a full sleep when exiting standby.
    const bool standby = in->last_read_time_us == 0;
    int64_t elapsed_time_since_last_read = 0;
    if (in->in_fd > 0)
    {
        elapsed_time_since_last_read = now - in->last_read_time_us;
    }
    else
    {
        elapsed_time_since_last_read = standby ? 0 : now - in->last_read_time_us;
    }
    int64_t sleep_time = bytes * 1000000LL / audio_stream_in_frame_size(stream) /
                             in_get_sample_rate(&stream->common) -
                         elapsed_time_since_last_read;

    if (sleep_time > 0)
    {
        usleep(sleep_time);
    }
    else
    {
        sleep_time = 0;
    }
    in->last_read_time_us = now + sleep_time;
    // last_read_time_us is an approximation of when the (simulated) alsa
    // buffer is drained by the read, and is empty.
    //
    // On the subsequent in_read(), we measure the elapsed time spent in
    // the recording thread. This is subtracted from the sleep estimate based on frames,
    // thereby accounting for fill in the alsa buffer during the interim.
    return ret;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static size_t samples_per_milliseconds(size_t milliseconds,
                                       uint32_t sample_rate,
                                       size_t channel_count)
{
    return milliseconds * sample_rate * channel_count / 1000;
}

static void *out_socket_sever_thread(void *args)
{
    struct stub_stream_out *out = (struct stub_stream_out *)args;
    int ret = 0;
    int so_reuseaddr = 1;

    ALOGV("%s Constructing audio out socket server...", __func__);
    out->oss_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (out->oss_fd < 0)
    {
        ALOGE("%s:%d Fail to construct audio out socket with error: %s",
              __func__, __LINE__, strerror(errno));
        return NULL;
    }
    if (setsockopt(out->oss_fd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(int)) < 0)
    {
        ALOGE("%s setsockopt(SO_REUSEADDR) failed. out->oss_fd: %d\n", __func__, out->oss_fd);
        return NULL;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(out->out_tcp_port);

    ret = bind(out->oss_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    if (ret < 0)
    {
        ALOGE("%s Failed to bind port(%d). ret: %d %s", __func__, out->out_tcp_port, ret, strerror(errno));
        return NULL;
    }

    ret = listen(out->oss_fd, 5);
    if (ret < 0)
    {
        ALOGE("%s Failed to listen on port %d", __func__, out->out_tcp_port);
        return NULL;
    }

    while (!out->oss_exit)
    {
        socklen_t alen = sizeof(struct sockaddr_in);

        ALOGW("%s Wait a audio out client to connect...", __func__);
        out->out_fd = accept(out->oss_fd, (struct sockaddr *)&addr, &alen);
        if (out->out_fd < 0)
        {
            ALOGE("%s The audio in socket server maybe shutdown as quit command is got. "
                  "Or else, error happen. port: %d %s",
                  __func__, out->out_tcp_port, strerror(errno));
        }
        else
        {
            ALOGW("%s A new audio out client connected to server. out->out_fd = %d", __func__, out->out_fd);
            struct audio_socket_info asi;
            asi.cmd = CMD_OPEN;
            asi.asci.sample_rate = out->sample_rate;
            asi.asci.channel_mask = out->channel_mask;
            asi.asci.format = out->format;
            asi.asci.frame_count = out->frame_count;
            ALOGV("%s asi.asci.sample_rate: %d asi.asci.channel_mask: %d asi.asci.format: %d asi.asci.frame_count: %d\n",
                  __func__, asi.asci.sample_rate, asi.asci.channel_mask, asi.asci.format, asi.asci.frame_count);
            do
            {
                ret = write(out->out_fd, &asi, sizeof(struct audio_socket_info));
            } while (ret < 0 && errno == EINTR);
            if (ret != sizeof(struct audio_socket_info))
                ALOGE("%s: could not notify the audio out client(%d) to open: ret=%d: %s.",
                      __FUNCTION__, out->out_fd, ret, strerror(errno));
            else
                ALOGV("%s Notify the audio out client(%d) to open.", __func__, out->out_fd);
        }
    }
    ALOGW("%s Quit. port %d(%d)", __func__, out->out_tcp_port, out->out_fd);
    close(out->out_fd);
    out->out_fd = -1;
    close(out->oss_fd);
    out->oss_fd = -1;
    return NULL;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out,
                                   const char *address __unused)
{
    ALOGV("adev_open_output_stream...");

    *stream_out = NULL;
    struct stub_stream_out *out =
        (struct stub_stream_out *)calloc(1, sizeof(struct stub_stream_out));
    if (!out)
        return -ENOMEM;

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;
    out->sample_rate = config->sample_rate;
    if (out->sample_rate == 0)
        out->sample_rate = STUB_DEFAULT_SAMPLE_RATE;
    out->channel_mask = config->channel_mask;
    if (out->channel_mask == AUDIO_CHANNEL_NONE)
        out->channel_mask = STUB_OUTPUT_DEFAULT_CHANNEL_MASK;
    out->format = config->format;
    if (out->format == AUDIO_FORMAT_DEFAULT)
        out->format = STUB_DEFAULT_AUDIO_FORMAT;
    out->frame_count = samples_per_milliseconds(
        STUB_OUTPUT_BUFFER_MILLISECONDS,
        out->sample_rate, 1);
    ALOGV("%s:%d Init out->out_fd to %d", __func__, __LINE__, out->out_fd);
    out->out_fd = -1;
    out->oss_fd = -1;
    out->oss_exit = 0;
    out->out_tcp_port = 8768;
    char buf[PROPERTY_VALUE_MAX] = {
        '\0',
    };

    if (property_get("virtual.audio.out.tcp.por", buf, "") > 0)
    {
        out->out_tcp_port = atoi(buf);
    }
    pthread_create(&out->oss_thread, NULL, out_socket_sever_thread, out);

    ALOGV("adev_open_output_stream: sample_rate: %u, channels: %x, format: %d,"
          " frames: %zu",
          out->sample_rate, out->channel_mask, out->format,
          out->frame_count);
    *stream_out = &out->stream;
    return 0;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct stub_stream_out *out = (struct stub_stream_out *)stream;
    if (out->out_fd > 0)
    {
        int ret = 0;
        struct audio_socket_info asi;
        asi.cmd = CMD_CLOSE;
        asi.data_size = 0;
        do
        {
            ret = write(out->out_fd, &asi, sizeof(struct audio_socket_info));
        } while (ret < 0 && errno == EINTR);
        if (ret != sizeof(struct audio_socket_info))
            ALOGE("%s: could not notify the audio out client(%d) to close: ret=%d: %s.",
                  __FUNCTION__, out->out_fd, ret, strerror(errno));
        else
            ALOGV("%s Notify the audio out client(%d) to close.", __func__, out->out_fd);
    }
    out->oss_exit = 1;
    if (out->oss_fd > 0)
    {
        ALOGV("Audio out socket server shutdown...");
        shutdown(out->oss_fd, SHUT_RD);
        close(out->oss_fd);
        out->oss_fd = -1;
    }
    ALOGV("adev_close_output_stream...");
    free(stream);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    ALOGV("adev_set_parameters");
    return -ENOSYS;
}

static char *adev_get_parameters(const struct audio_hw_device *dev,
                                 const char *keys)
{
    ALOGV("adev_get_parameters");
    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    ALOGV("adev_init_check");
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    ALOGV("adev_set_voice_volume: %f", volume);
    return -ENOSYS;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    ALOGV("adev_set_master_volume: %f", volume);
    return -ENOSYS;
}

static int adev_get_master_volume(struct audio_hw_device *dev, float *volume)
{
    ALOGV("adev_get_master_volume: %f", *volume);
    return -ENOSYS;
}

static int adev_set_master_mute(struct audio_hw_device *dev, bool muted)
{
    ALOGV("adev_set_master_mute: %d", muted);
    return -ENOSYS;
}

static int adev_get_master_mute(struct audio_hw_device *dev, bool *muted)
{
    ALOGV("adev_get_master_mute: %d", *muted);
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    ALOGV("adev_set_mode: %d", mode);
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    ALOGV("adev_set_mic_mute: %d", state);
    return -ENOSYS;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    ALOGV("adev_get_mic_mute");
    return -ENOSYS;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
    size_t buffer_size = samples_per_milliseconds(
        STUB_INPUT_BUFFER_MILLISECONDS,
        config->sample_rate,
        audio_channel_count_from_in_mask(
            config->channel_mask));

    if (!audio_has_proportional_frames(config->format))
    {
        // Since the audio data is not proportional choose an arbitrary size for
        // the buffer.
        buffer_size *= 4;
    }
    else
    {
        buffer_size *= audio_bytes_per_sample(config->format);
    }
    ALOGV("adev_get_input_buffer_size: %zu", buffer_size);
    return buffer_size;
}

static void *in_socket_sever_thread(void *args)
{
    struct stub_stream_in *in = (struct stub_stream_in *)args;
    int ret = 0;
    int so_reuseaddr = 1;

    ALOGV("%s Constructing audio in socket server...", __func__);
    in->iss_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (in->iss_fd < 0)
    {
        ALOGE("%s:%d Fail to construct audio in socket with error: %s",
              __func__, __LINE__, strerror(errno));
        return NULL;
    }
    if (setsockopt(in->iss_fd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(int)) < 0)
    {
        ALOGE("%s setsockopt(SO_REUSEADDR) failed. in->iss_fd: %d\n", __func__, in->iss_fd);
        return NULL;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(in->in_tcp_port);

    ret = bind(in->iss_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    if (ret < 0)
    {
        ALOGE("%s Failed to bind port(%d). ret: %d, %s", __func__, in->in_tcp_port, ret, strerror(errno));
        return NULL;
    }

    ret = listen(in->iss_fd, 5);
    if (ret < 0)
    {
        ALOGE("%s Failed to listen on port %d", __func__, in->in_tcp_port);
        return NULL;
    }

    while (!in->iss_exit)
    {
        socklen_t alen = sizeof(struct sockaddr_in);

        ALOGW("%s Wait a audio in client to connect...", __func__);
        in->in_fd = accept(in->iss_fd, (struct sockaddr *)&addr, &alen);
        if (in->in_fd < 0)
        {
            ALOGE("%s The audio in socket server maybe shutdown as quit command is got. "
                  "Or else, error happen. port: %d %s",
                  __func__, in->in_tcp_port, strerror(errno));
        }
        else
        {
            ALOGW("%s A new audio in client connected to server. in->in_fd = %d", __func__, in->in_fd);
            struct audio_socket_info asi;
            asi.cmd = CMD_OPEN;
            asi.asci.sample_rate = in->sample_rate;
            asi.asci.channel_mask = in->channel_mask;
            asi.asci.format = in->format;
            asi.asci.frame_count = in->frame_count;
            do
            {
                ret = write(in->in_fd, &asi, sizeof(struct audio_socket_info));
            } while (ret < 0 && errno == EINTR);
            if (ret != sizeof(struct audio_socket_info))
                ALOGE("%s: could not notify the audio in client(%d) to open: ret=%d: %s.",
                      __FUNCTION__, in->in_fd, ret, strerror(errno));
            else
                ALOGV("%s Notify the audio in client(%d) to open.", __func__, in->in_fd);
        }
    }
    ALOGW("%s Quit. prot %d(%d)", __func__, in->in_tcp_port, in->in_fd);
    close(in->in_fd);
    in->in_fd = -1;
    close(in->iss_fd);
    in->iss_fd = -1;
    return NULL;
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in,
                                  audio_input_flags_t flags __unused,
                                  const char *address __unused,
                                  audio_source_t source __unused)
{
    ALOGV("adev_open_input_stream...");

    *stream_in = NULL;
    struct stub_stream_in *in = (struct stub_stream_in *)calloc(1, sizeof(struct stub_stream_in));
    if (!in)
        return -ENOMEM;

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;
    in->sample_rate = config->sample_rate;
    if (in->sample_rate == 0)
        in->sample_rate = STUB_DEFAULT_SAMPLE_RATE;
    in->channel_mask = config->channel_mask;
    if (in->channel_mask == AUDIO_CHANNEL_NONE)
        in->channel_mask = STUB_INPUT_DEFAULT_CHANNEL_MASK;
    in->format = config->format;
    if (in->format == AUDIO_FORMAT_DEFAULT)
        in->format = STUB_DEFAULT_AUDIO_FORMAT;
    in->frame_count = samples_per_milliseconds(
        STUB_INPUT_BUFFER_MILLISECONDS, in->sample_rate, 1);
    in->in_fd = -1;
    in->iss_fd = -1;
    in->iss_exit = 0;
    in->in_tcp_port = 8767;
    char buf[PROPERTY_VALUE_MAX] = {
        '\0',
    };

    if (property_get("virtual.audio.in.tcp.port", buf, "") > 0)
    {
        in->in_tcp_port = atoi(buf);
    }
    pthread_create(&in->iss_thread, NULL, in_socket_sever_thread, in);

    ALOGV("adev_open_input_stream: sample_rate: %u, channels: %x, format: %d,"
          "frames: %zu",
          in->sample_rate, in->channel_mask, in->format,
          in->frame_count);
    *stream_in = &in->stream;
    return 0;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                    struct audio_stream_in *stream)
{
    struct stub_stream_in *in = (struct stub_stream_in *)stream;

    if (in->in_fd > 0)
    {
        int ret = 0;
        struct audio_socket_info asi;
        asi.cmd = CMD_CLOSE;
        asi.data_size = 0;
        do
        {
            ret = write(in->in_fd, &asi, sizeof(struct audio_socket_info));
        } while (ret < 0 && errno == EINTR);
        if (ret != sizeof(struct audio_socket_info))
            ALOGE("%s: could not notify the audio in client(%d) to close: ret=%d: %s.",
                  __FUNCTION__, in->in_fd, ret, strerror(errno));
        else
            ALOGV("%s Notify the audio in client(%d) to close.", __func__, in->in_fd);
    }
    in->iss_exit = 1;
    if (in->iss_fd > 0)
    {
        ALOGV("Audio in socket server shutdown...");
        shutdown(in->iss_fd, SHUT_RD);
        close(in->iss_fd);
        in->iss_fd = -1;
    }
    ALOGV("adev_close_input_stream...");
    return;
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    ALOGV("adev_dump");
    return 0;
}

static int adev_close(hw_device_t *device)
{
    ALOGV("adev_close");
    free(device);
    return 0;
}

static int adev_open(const hw_module_t *module, const char *name,
                     hw_device_t **device)
{
    ALOGV("adev_open: %s", name);

    //It will generate SIGPIPE when write to a closed socket, which will kill
    //the process. Ignore the signal SIGPIPE to avoid the process crash.
    signal(SIGPIPE, SIG_IGN);

    struct stub_audio_device *adev;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    adev = calloc(1, sizeof(struct stub_audio_device));
    if (!adev)
        return -ENOMEM;

    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->device.common.module = (struct hw_module_t *)module;
    adev->device.common.close = adev_close;

    adev->device.init_check = adev_init_check;
    adev->device.set_voice_volume = adev_set_voice_volume;
    adev->device.set_master_volume = adev_set_master_volume;
    adev->device.get_master_volume = adev_get_master_volume;
    adev->device.set_master_mute = adev_set_master_mute;
    adev->device.get_master_mute = adev_get_master_mute;
    adev->device.set_mode = adev_set_mode;
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;
    adev->device.get_parameters = adev_get_parameters;
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

    *device = &adev->device.common;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "Virtal audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
