/*
 * Copyright (C) 2010 The Android Open Source Project
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

/* this implements a GPS hardware library for the Android emulator.
 * the following code should be built as a shared library that will be
 * placed into /system/lib/hw/gps.goldfish.so
 *
 * it will be loaded by the code in hardware/libhardware/hardware.c
 * which is itself called from android_location_GpsLocationProvider.cpp
 */

// #define LOG_NDEBUG 0
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <math.h>
#include <time.h>

#define LOG_TAG "gps_virtual"
#include <cutils/log.h>
#include <cutils/sockets.h>
#include <hardware/gps.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <cutils/properties.h>
#include <sys/system_properties.h>

// #define GPS_DEBUG 1

#undef D
#if GPS_DEBUG
#define D(...) ALOGD(__VA_ARGS__)
#else
#define D(...) ((void)0)
#endif

/*****************************************************************/
/*****************************************************************/
/*****                                                       *****/
/*****       N M E A   T O K E N I Z E R                     *****/
/*****                                                       *****/
/*****************************************************************/
/*****************************************************************/

typedef struct
{
    const char *p;
    const char *end;
} Token;

#define MAX_NMEA_TOKENS 16

typedef struct
{
    int count;
    Token tokens[MAX_NMEA_TOKENS];
} NmeaTokenizer;

static int
nmea_tokenizer_init(NmeaTokenizer *t, const char *p, const char *end)
{
    int count = 0;

    // the initial '$' is optional
    if (p < end && p[0] == '$')
        p += 1;

    // remove trailing newline
    if (end > p && end[-1] == '\n')
    {
        end -= 1;
        if (end > p && end[-1] == '\r')
            end -= 1;
    }

    // get rid of checksum at the end of the sentecne
    if (end >= p + 3 && end[-3] == '*')
    {
        end -= 3;
    }

    while (p < end)
    {
        const char *q = p;

        q = memchr(p, ',', end - p);
        if (q == NULL)
            q = end;

        if (count < MAX_NMEA_TOKENS)
        {
            t->tokens[count].p = p;
            t->tokens[count].end = q;
            count += 1;
        }
        if (q < end)
            q += 1;

        p = q;
    }

    t->count = count;
    return count;
}

static Token
nmea_tokenizer_get(NmeaTokenizer *t, int index)
{
    Token tok;
    static const char *dummy = "";

    if (index < 0 || index >= t->count)
    {
        tok.p = tok.end = dummy;
    }
    else
        tok = t->tokens[index];

    return tok;
}

static int
str2int(const char *p, const char *end)
{
    int result = 0;
    int len = end - p;

    for (; len > 0; len--, p++)
    {
        int c;

        if (p >= end)
            goto Fail;

        c = *p - '0';
        if ((unsigned)c >= 10)
            goto Fail;

        result = result * 10 + c;
    }
    return result;

Fail:
    return -1;
}

static double
str2float(const char *p, const char *end)
{
    int len = end - p;
    char temp[16];

    if (len >= (int)sizeof(temp))
        return 0.;

    memcpy(temp, p, len);
    temp[len] = 0;
    return strtod(temp, NULL);
}

/*****************************************************************/
/*****************************************************************/
/*****                                                       *****/
/*****       N M E A   P A R S E R                           *****/
/*****                                                       *****/
/*****************************************************************/
/*****************************************************************/

#define NMEA_MAX_SIZE 83

typedef struct
{
    int pos;
    int overflow;
    int utc_year;
    int utc_mon;
    int utc_day;
    int utc_diff;
    GpsLocation fix;
    gps_location_callback callback;
    char in[NMEA_MAX_SIZE + 1];
} NmeaReader;

static void
nmea_reader_update_utc_diff(NmeaReader *r)
{
    time_t now = time(NULL);
    struct tm tm_local;
    struct tm tm_utc;
    long time_local, time_utc;

    gmtime_r(&now, &tm_utc);
    localtime_r(&now, &tm_local);

    time_local = tm_local.tm_sec +
                 60 * (tm_local.tm_min +
                       60 * (tm_local.tm_hour +
                             24 * (tm_local.tm_yday +
                                   365 * tm_local.tm_year)));

    time_utc = tm_utc.tm_sec +
               60 * (tm_utc.tm_min +
                     60 * (tm_utc.tm_hour +
                           24 * (tm_utc.tm_yday +
                                 365 * tm_utc.tm_year)));

    r->utc_diff = time_utc - time_local;
}

static void
nmea_reader_init(NmeaReader *r)
{
    memset(r, 0, sizeof(*r));

    r->pos = 0;
    r->overflow = 0;
    r->utc_year = -1;
    r->utc_mon = -1;
    r->utc_day = -1;
    r->callback = NULL;
    r->fix.size = sizeof(r->fix);

    nmea_reader_update_utc_diff(r);
}

static void
nmea_reader_set_callback(NmeaReader *r, gps_location_callback cb)
{
    r->callback = cb;
    if (cb != NULL && r->fix.flags != 0)
    {
        D("%s: sending latest fix to new callback", __FUNCTION__);
        r->callback(&r->fix);
    }
}

static int
nmea_reader_update_time(NmeaReader *r, Token tok)
{
    int hour, minute;
    double seconds;
    struct tm tm;
    time_t fix_time;

    if (tok.p + 6 > tok.end)
        return -1;

    if (r->utc_year < 0)
    {
        // no date yet, get current one
        time_t now = time(NULL);
        gmtime_r(&now, &tm);
        r->utc_year = tm.tm_year + 1900;
        r->utc_mon = tm.tm_mon + 1;
        r->utc_day = tm.tm_mday;
    }

    hour = str2int(tok.p, tok.p + 2);
    minute = str2int(tok.p + 2, tok.p + 4);
    seconds = str2float(tok.p + 4, tok.end);

    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = (int)seconds;
    tm.tm_year = r->utc_year - 1900;
    tm.tm_mon = r->utc_mon - 1;
    tm.tm_mday = r->utc_day;
    tm.tm_isdst = -1;

    // This is a little confusing, let's use an example:
    // Suppose now it's 1970-1-1 01:00 GMT, local time is 1970-1-1 00:00 GMT-1
    // Then the utc_diff is 3600.
    // The time string from GPS is 01:00:00, mktime assumes it's a local
    // time. So we are doing mktime for 1970-1-1 01:00 GMT-1. The result of
    // mktime is 7200 (1970-1-1 02:00 GMT) actually. To get the correct
    // timestamp, we have to subtract utc_diff here.
    fix_time = mktime(&tm) - r->utc_diff;
    r->fix.timestamp = (long long)fix_time * 1000;
    return 0;
}

static int
nmea_reader_update_date(NmeaReader *r, Token date, Token time)
{
    Token tok = date;
    int day, mon, year;

    if (tok.p + 6 != tok.end)
    {
        D("date not properly formatted: '%.*s'", tok.end - tok.p, tok.p);
        return -1;
    }
    day = str2int(tok.p, tok.p + 2);
    mon = str2int(tok.p + 2, tok.p + 4);
    year = str2int(tok.p + 4, tok.p + 6) + 2000;

    if ((day | mon | year) < 0)
    {
        D("date not properly formatted: '%.*s'", tok.end - tok.p, tok.p);
        return -1;
    }

    r->utc_year = year;
    r->utc_mon = mon;
    r->utc_day = day;

    return nmea_reader_update_time(r, time);
}

static double
convert_from_hhmm(Token tok)
{
    double val = str2float(tok.p, tok.end);
    int degrees = (int)(floor(val) / 100);
    double minutes = val - degrees * 100.;
    double dcoord = degrees + minutes / 60.0;
    return dcoord;
}

static int
nmea_reader_update_latlong(NmeaReader *r,
                           Token latitude,
                           char latitudeHemi,
                           Token longitude,
                           char longitudeHemi)
{
    double lat, lon;
    Token tok;

    r->fix.flags &= ~GPS_LOCATION_HAS_LAT_LONG;

    tok = latitude;
    if (tok.p + 6 > tok.end)
    {
        D("latitude is too short: '%.*s'", tok.end - tok.p, tok.p);
        return -1;
    }
    lat = convert_from_hhmm(tok);
    if (latitudeHemi == 'S')
        lat = -lat;

    tok = longitude;
    if (tok.p + 6 > tok.end)
    {
        D("longitude is too short: '%.*s'", tok.end - tok.p, tok.p);
        return -1;
    }
    lon = convert_from_hhmm(tok);
    if (longitudeHemi == 'W')
        lon = -lon;

    r->fix.flags |= GPS_LOCATION_HAS_LAT_LONG;
    r->fix.latitude = lat;
    r->fix.longitude = lon;
    return 0;
}

static int
nmea_reader_update_altitude(NmeaReader *r,
                            Token altitude,
                            Token __unused units)
{
    Token tok = altitude;

    r->fix.flags &= ~GPS_LOCATION_HAS_ALTITUDE;

    if (tok.p >= tok.end)
        return -1;

    r->fix.flags |= GPS_LOCATION_HAS_ALTITUDE;
    r->fix.altitude = str2float(tok.p, tok.end);
    return 0;
}

static int
nmea_reader_update_bearing(NmeaReader *r,
                           Token bearing)
{
    Token tok = bearing;

    r->fix.flags &= ~GPS_LOCATION_HAS_BEARING;

    if (tok.p >= tok.end)
        return -1;

    r->fix.flags |= GPS_LOCATION_HAS_BEARING;
    r->fix.bearing = str2float(tok.p, tok.end);
    return 0;
}

static int
nmea_reader_update_speed(NmeaReader *r,
                         Token speed)
{
    Token tok = speed;

    r->fix.flags &= ~GPS_LOCATION_HAS_SPEED;

    if (tok.p >= tok.end)
        return -1;

    r->fix.flags |= GPS_LOCATION_HAS_SPEED;
    r->fix.speed = str2float(tok.p, tok.end);
    return 0;
}

static int
nmea_reader_update_accuracy(NmeaReader *r)
{
    // Always return 20m accuracy.
    // Possibly parse it from the NMEA sentence in the future.
    r->fix.flags |= GPS_LOCATION_HAS_ACCURACY;
    r->fix.accuracy = 20;
    return 0;
}

static void
nmea_reader_parse(NmeaReader *r)
{
    /* we received a complete sentence, now parse it to generate
    * a new GPS fix...
    */
    NmeaTokenizer tzer[1];
    Token tok;

    D("Received: '%.*s'", r->pos, r->in);
    if (r->pos < 9)
    {
        D("Too short. discarded.");
        return;
    }

    nmea_tokenizer_init(tzer, r->in, r->in + r->pos);
#if GPS_DEBUG
    {
        int n;
        D("Found %d tokens", tzer->count);
        for (n = 0; n < tzer->count; n++)
        {
            Token tok = nmea_tokenizer_get(tzer, n);
            D("%2d: '%.*s'", n, tok.end - tok.p, tok.p);
        }
    }
#endif

    tok = nmea_tokenizer_get(tzer, 0);
    if (tok.p + 5 > tok.end)
    {
        D("sentence id '%.*s' too short, ignored.", tok.end - tok.p, tok.p);
        return;
    }

    // ignore first two characters.
    tok.p += 2;
    if (!memcmp(tok.p, "GGA", 3))
    {
        // GPS fix
        Token tok_time = nmea_tokenizer_get(tzer, 1);
        Token tok_latitude = nmea_tokenizer_get(tzer, 2);
        Token tok_latitudeHemi = nmea_tokenizer_get(tzer, 3);
        Token tok_longitude = nmea_tokenizer_get(tzer, 4);
        Token tok_longitudeHemi = nmea_tokenizer_get(tzer, 5);
        Token tok_altitude = nmea_tokenizer_get(tzer, 9);
        Token tok_altitudeUnits = nmea_tokenizer_get(tzer, 10);

        nmea_reader_update_time(r, tok_time);
        nmea_reader_update_latlong(r, tok_latitude,
                                   tok_latitudeHemi.p[0],
                                   tok_longitude,
                                   tok_longitudeHemi.p[0]);
        nmea_reader_update_altitude(r, tok_altitude, tok_altitudeUnits);
    }
    else if (!memcmp(tok.p, "GSA", 3))
    {
        // do something ?
    }
    else if (!memcmp(tok.p, "RMC", 3))
    {
        Token tok_time = nmea_tokenizer_get(tzer, 1);
        Token tok_fixStatus = nmea_tokenizer_get(tzer, 2);
        Token tok_latitude = nmea_tokenizer_get(tzer, 3);
        Token tok_latitudeHemi = nmea_tokenizer_get(tzer, 4);
        Token tok_longitude = nmea_tokenizer_get(tzer, 5);
        Token tok_longitudeHemi = nmea_tokenizer_get(tzer, 6);
        Token tok_speed = nmea_tokenizer_get(tzer, 7);
        Token tok_bearing = nmea_tokenizer_get(tzer, 8);
        Token tok_date = nmea_tokenizer_get(tzer, 9);

        D("in RMC, fixStatus=%c", tok_fixStatus.p[0]);
        if (tok_fixStatus.p[0] == 'A')
        {
            nmea_reader_update_date(r, tok_date, tok_time);

            nmea_reader_update_latlong(r, tok_latitude,
                                       tok_latitudeHemi.p[0],
                                       tok_longitude,
                                       tok_longitudeHemi.p[0]);

            nmea_reader_update_bearing(r, tok_bearing);
            nmea_reader_update_speed(r, tok_speed);
        }
    }
    else
    {
        tok.p -= 2;
        D("unknown sentence '%.*s", tok.end - tok.p, tok.p);
    }

    // Always update accuracy
    nmea_reader_update_accuracy(r);

    if (r->fix.flags != 0)
    {
#if GPS_DEBUG
        char temp[256];
        char *p = temp;
        char *end = p + sizeof(temp);
        struct tm utc;

        p += snprintf(p, end - p, "sending fix");
        if (r->fix.flags & GPS_LOCATION_HAS_LAT_LONG)
        {
            p += snprintf(p, end - p, " lat=%g lon=%g", r->fix.latitude, r->fix.longitude);
        }
        if (r->fix.flags & GPS_LOCATION_HAS_ALTITUDE)
        {
            p += snprintf(p, end - p, " altitude=%g", r->fix.altitude);
        }
        if (r->fix.flags & GPS_LOCATION_HAS_SPEED)
        {
            p += snprintf(p, end - p, " speed=%g", r->fix.speed);
        }
        if (r->fix.flags & GPS_LOCATION_HAS_BEARING)
        {
            p += snprintf(p, end - p, " bearing=%g", r->fix.bearing);
        }
        if (r->fix.flags & GPS_LOCATION_HAS_ACCURACY)
        {
            p += snprintf(p, end - p, " accuracy=%g", r->fix.accuracy);
        }
        gmtime_r((time_t *)&r->fix.timestamp, &utc);
        p += snprintf(p, end - p, " time=%s", asctime(&utc));
        D(temp);
#endif
        if (r->callback)
        {
            r->callback(&r->fix);
        }
        else
        {
            D("no callback, keeping data until needed !");
        }
    }
}

static void
nmea_reader_addc(NmeaReader *r, int c)
{
    if (r->overflow)
    {
        r->overflow = (c != '\n');
        return;
    }

    if (r->pos >= (int)sizeof(r->in) - 1)
    {
        r->overflow = 1;
        r->pos = 0;
        return;
    }

    r->in[r->pos] = (char)c;
    r->pos += 1;

    if (c == '\n')
    {
        nmea_reader_parse(r);
        r->pos = 0;
    }
}

/*****************************************************************/
/*****************************************************************/
/*****                                                       *****/
/*****       C O N N E C T I O N   S T A T E                 *****/
/*****                                                       *****/
/*****************************************************************/
/*****************************************************************/

/* commands sent to the gps thread */
enum
{
    CMD_QUIT = 0,
    CMD_START = 1,
    CMD_STOP = 2
};

/* this is the state of our connection to the qemu_gpsd daemon */
typedef struct
{
    int init;
    int fd;
    GpsCallbacks callbacks;
    pthread_t thread;
    int control[2];
    pthread_t gps_socket_server_tid;
    int gps_socket_server_fd;
    bool gsst_loop_exit; // gps socket server thread loop exit
    int epoll_fd;
    int tcp_port; // virtual gps tcp port
    int need_notify_client_start;
} GpsState;

static GpsState _gps_state[1];

static void
gps_state_done(GpsState *s)
{
    // tell the thread to quit, and wait for it
    char cmd = CMD_QUIT;
    void *dummy;
    write(s->control[0], &cmd, 1);
    pthread_join(s->thread, &dummy);

    int ret = 0;
    if (s->fd > 0)
    {
        do
        {
            ret = write(s->fd, &cmd, 1);
        } while (ret < 0 && errno == EINTR);
        if (ret != 1)
            D("%s: could not notify client(%d) to quit: ret=%d: %s",
              __FUNCTION__, s->fd, ret, strerror(errno));
        else
            ALOGV("%s Notify client(%d) to quit", __func__, s->fd);
    }
    s->gsst_loop_exit = 1;
    shutdown(s->gps_socket_server_fd, SHUT_RDWR);
    pthread_join(s->gps_socket_server_tid, &dummy);

    // close the control socket pair
    close(s->control[0]);
    s->control[0] = -1;
    close(s->control[1]);
    s->control[1] = -1;

    // close connection to the GPS daemon
    shutdown(s->fd, SHUT_RDWR);
    close(s->fd);
    s->fd = -1;
    s->init = 0;
}

static void
gps_state_start(GpsState *s)
{
    char cmd = CMD_START;
    int ret;
    if (s->fd > 0)
    {
        do
        {
            ret = write(s->fd, &cmd, 1);
        } while (ret < 0 && errno == EINTR);
        if (ret != 1)
            D("%s: could not notify client(%d) to start: ret=%d: %s",
              __FUNCTION__, s->fd, ret, strerror(errno));
        else
            ALOGV("%s Notify client(%d) to start", __func__, s->fd);
    }
    s->need_notify_client_start = 1;

    do
    {
        ret = write(s->control[0], &cmd, 1);
    } while (ret < 0 && errno == EINTR);

    if (ret != 1)
        D("%s: could not send CMD_START command: ret=%d: %s",
          __FUNCTION__, ret, strerror(errno));
}

static void
gps_state_stop(GpsState *s)
{
    char cmd = CMD_STOP;
    int ret;

    if (s->fd > 0)
    {
        do
        {
            ret = write(s->fd, &cmd, 1);
        } while (ret < 0 && errno == EINTR);

        if (ret != 1)
            D("%s: could not notify client(%d) to stop: ret=%d: %s",
              __FUNCTION__, s->fd, ret, strerror(errno));
        else
            ALOGV("%s Notify client(%d) to stop", __func__, s->fd);
    }
    s->need_notify_client_start = 0;

    do
    {
        ret = write(s->control[0], &cmd, 1);
    } while (ret < 0 && errno == EINTR);

    if (ret != 1)
        D("%s: could not send CMD_STOP command: ret=%d: %s",
          __FUNCTION__, ret, strerror(errno));
}

static int
epoll_register(int epoll_fd, int fd)
{
    struct epoll_event ev;
    int ret, flags;

    /* important: make the fd non-blocking */
    flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    ev.events = EPOLLIN;
    ev.data.fd = fd;
    do
    {
        ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    } while (ret < 0 && errno == EINTR);
    return ret;
}

static int
epoll_deregister(int epoll_fd, int fd)
{
    int ret;
    do
    {
        ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    } while (ret < 0 && errno == EINTR);
    return ret;
}

/* this is the main thread, it waits for commands from gps_state_start/stop and,
 * when started, messages from the GPS daemon. these are simple NMEA sentences
 * that must be parsed to be converted into GPS fixes sent to the framework
 */
static void
gps_state_thread(void *arg)
{
    GpsState *state = (GpsState *)arg;
    NmeaReader reader[1];
    int epoll_fd = epoll_create(2);
    if (epoll_fd < 0)
    {
        ALOGE("%s Fail to create epoll fd.", __func__);
    }
    else
    {
        state->epoll_fd = epoll_fd;
        ALOGD("%s Set state->epoll_fd = %d.", __func__, state->epoll_fd);
    }
    int started = 0;
    int control_fd = state->control[1];
    GpsStatus gps_status;
    gps_status.size = sizeof(gps_status);
    GpsSvStatus gps_sv_status;
    memset(&gps_sv_status, 0, sizeof(gps_sv_status));
    gps_sv_status.size = sizeof(gps_sv_status);
    gps_sv_status.num_svs = 1;
    gps_sv_status.sv_list[0].size = sizeof(gps_sv_status.sv_list[0]);
    gps_sv_status.sv_list[0].prn = 17;
    gps_sv_status.sv_list[0].snr = 60.0;
    gps_sv_status.sv_list[0].elevation = 30.0;
    gps_sv_status.sv_list[0].azimuth = 30.0;

    nmea_reader_init(reader);

    // register control file descriptors for polling
    epoll_register(epoll_fd, control_fd);
    if (state->fd > 0)
    {
        ALOGE("%s Register state->fd(%d) to epoll_fd(%d) epoll fd.", __func__, state->fd, epoll_fd);
        epoll_register(epoll_fd, state->fd);
    }

    D("gps thread running");

    // now loop
    for (;;)
    {
        struct epoll_event events[2];
        int ne, nevents;

        int timeout = -1;
        if (gps_status.status == GPS_STATUS_SESSION_BEGIN)
        {
            timeout = 10 * 1000; // 10 seconds
        }
        nevents = epoll_wait(epoll_fd, events, 2, timeout);
        if (state->callbacks.sv_status_cb)
        {
            state->callbacks.sv_status_cb(&gps_sv_status);
        }
        // update satilite info
        if (nevents < 0)
        {
            if (errno != EINTR)
                ALOGE("epoll_wait() unexpected error: %s", strerror(errno));
            continue;
        }
        D("gps thread received %d events", nevents);
        for (ne = 0; ne < nevents; ne++)
        {
            if ((events[ne].events & (EPOLLERR | EPOLLHUP)) != 0)
            {
                ALOGE("EPOLLERR or EPOLLHUP after epoll_wait() !?");
                int fd = events[ne].data.fd;
                if (fd == state->fd)
                {
                    ALOGE("%s GPS socket client may close. Deregister s->fd and set it to -1 and let client to reconnect.", __func__);
                    epoll_deregister(state->epoll_fd, state->fd);
                    state->fd = -1;
                    continue;
                }
                else if (fd == control_fd)
                {
                    ALOGE("%s control_fd closed. return. ", __func__);
                    return;
                }
            }
            if ((events[ne].events & EPOLLIN) != 0)
            {
                int fd = events[ne].data.fd;

                if (fd == control_fd)
                {
                    char cmd = 0xFF;
                    int ret;
                    D("gps control fd event");
                    do
                    {
                        ret = read(fd, &cmd, 1);
                    } while (ret < 0 && errno == EINTR);

                    if (cmd == CMD_QUIT)
                    {
                        D("gps thread quitting on demand");
                        return;
                    }
                    else if (cmd == CMD_START)
                    {
                        if (!started)
                        {
                            D("gps thread starting  location_cb=%p", state->callbacks.location_cb);
                            started = 1;
                            nmea_reader_set_callback(reader, state->callbacks.location_cb);
                            gps_status.status = GPS_STATUS_SESSION_BEGIN;
                            if (state->callbacks.status_cb)
                            {
                                state->callbacks.status_cb(&gps_status);
                            }
                        }
                    }
                    else if (cmd == CMD_STOP)
                    {
                        if (started)
                        {
                            D("gps thread stopping");
                            started = 0;
                            nmea_reader_set_callback(reader, NULL);
                            gps_status.status = GPS_STATUS_SESSION_END;
                            if (state->callbacks.status_cb)
                            {
                                state->callbacks.status_cb(&gps_status);
                            }
                        }
                    }
                }
                else if (fd == state->fd)
                {
                    char buff[32];
                    D("gps fd event");
                    for (;;)
                    {
                        int nn, ret;

                        ret = read(fd, buff, sizeof(buff));
                        if (ret < 0)
                        {
                            if (errno == EINTR)
                                continue;
                            if (errno != EWOULDBLOCK)
                                ALOGE("error while reading from gps daemon socket: %s:", strerror(errno));
                            break;
                        }
                        if (ret == 0)
                        {
                            ALOGE("%s:%d GPS socket client may close. Deregister s->fd and set it to -1 and let client to reconnect.", __func__, __LINE__);
                            epoll_deregister(state->epoll_fd, state->fd);
                            state->fd = -1;
                            break;
                        }
                        D("received %d bytes: %.*s", ret, ret, buff);
                        for (nn = 0; nn < ret; nn++)
                            nmea_reader_addc(reader, buff[nn]);
                    }
                    D("gps fd event end");
                }
                else
                {
                    ALOGE("epoll_wait() returned unkown fd %d ?", fd);
                }
            }
        }
    }
}

static void gps_socket_server_thread(void *arg)
{
    GpsState *state = (GpsState *)arg;
    int ret = 0;
    ALOGV("Constructing GPS socket server...");
    state->gps_socket_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (state->gps_socket_server_fd < 0)
    {
        ALOGE("%s:%d Fail to construct tcp socket with error: %s",
              __func__, __LINE__, strerror(errno));
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(state->tcp_port);

    ret = bind(state->gps_socket_server_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    if (ret < 0)
    {
        ALOGE("%s Failed to bind server socket address %d, %s", __func__, ret, strerror(errno));
        return;
    }

    ret = listen(state->gps_socket_server_fd, 5);
    if (ret < 0)
    {
        ALOGE("%s Failed to listen on server socket", __func__);
        return;
    }

    while (!state->gsst_loop_exit)
    {
        socklen_t alen = sizeof(struct sockaddr_in);
        ALOGV("%s Wait a GPS client to connect...", __func__);
        state->fd = accept(state->gps_socket_server_fd, (struct sockaddr *)&addr, &alen);
        if (state->fd != -1)
        {
            ALOGV("%s A GPS client connected to server. state->fd = %d", __func__, state->fd);
            if (state->epoll_fd > 0)
            {
                ALOGV("%s register state->fd(%d) to state->epoll_fd(%d)", __func__, state->fd, state->epoll_fd);
                epoll_register(state->epoll_fd, state->fd);
            }

            //Android already triggered start command. Notify client to start when it connect to server.
            if (state->need_notify_client_start)
            {
                ALOGV("%s Android already triggered start command. Notify client to start when it connect to server.", __func__);
                char cmd = CMD_START;
                do
                {
                    ret = write(state->fd, &cmd, 1);
                } while (ret < 0 && errno == EINTR);
                if (ret != 1)
                    D("%s: could not notify client(%d) to start: ret=%d: %s",
                      __FUNCTION__, state->fd, ret, strerror(errno));
                else
                    ALOGV("%s Notify client(%d) to start", __func__, state->fd);
            }
        }
        else
        {
            ALOGV("%s GPS socket server maybe shutdown as quit command is got. "
                  "Or else, error happen. state->fd = %d %s.",
                  __func__, state->fd, strerror(errno));
        }
    }
    close(state->fd);
    state->fd = -1;
    close(state->gps_socket_server_fd);
    state->gps_socket_server_fd = -1;
    ALOGV("%s Quit", __func__);
    return;
}

static void
gps_state_init(GpsState *state, GpsCallbacks *callbacks)
{
    state->init = 1;
    state->control[0] = -1;
    state->control[1] = -1;
    state->fd = -1;
    state->gsst_loop_exit = 0;
    state->gps_socket_server_fd = -1;

    state->tcp_port = 8766;
    char buf[PROPERTY_VALUE_MAX] = {
        '\0',
    };
    if (property_get("virtual.gps.tcp.port", buf, "") > 0)
    {
        state->tcp_port = atoi(buf);
    }
    state->need_notify_client_start = 0;

    state->gps_socket_server_tid = callbacks->create_thread_cb("gps_socket_server_thread", gps_socket_server_thread, state);

    if (!state->gps_socket_server_tid)
    {
        ALOGE("could not create gps socket server thread: %s", strerror(errno));
        goto Fail;
    }

    D("Virtual gps will read with port '%d'", state->tcp_port);

    if (socketpair(AF_LOCAL, SOCK_STREAM, 0, state->control) < 0)
    {
        ALOGE("could not create thread control socket pair: %s", strerror(errno));
        goto Fail;
    }

    state->thread = callbacks->create_thread_cb("gps_state_thread", gps_state_thread, state);

    if (!state->thread)
    {
        ALOGE("could not create gps thread: %s", strerror(errno));
        goto Fail;
    }

    state->callbacks = *callbacks;

    // Explicitly initialize capabilities
    state->callbacks.set_capabilities_cb(0);

    // Setup system info, we are pre 2016 hardware.
    GnssSystemInfo sysinfo;
    sysinfo.size = sizeof(GnssSystemInfo);
    sysinfo.year_of_hw = 2015;
    state->callbacks.set_system_info_cb(&sysinfo);

    D("gps state initialized");
    return;

Fail:
    gps_state_done(state);
}

/*****************************************************************/
/*****************************************************************/
/*****                                                       *****/
/*****       I N T E R F A C E                               *****/
/*****                                                       *****/
/*****************************************************************/
/*****************************************************************/

static int
virtual_gps_init(GpsCallbacks *callbacks)
{
    GpsState *s = _gps_state;

    if (!s->init)
        gps_state_init(s, callbacks);

    if (s->fd < 0)
    {
        ALOGV("%s: s->fd should be available when a client connect to GPS socket server.\n", __func__);
    }

    return 0;
}

static void
virtual_gps_cleanup(void)
{
    GpsState *s = _gps_state;

    if (s->init)
        gps_state_done(s);
}

static int
virtual_gps_start()
{
    GpsState *s = _gps_state;

    if (!s->init)
    {
        D("%s: called with uninitialized state !!", __FUNCTION__);
        return -1;
    }

    D("%s: called", __FUNCTION__);
    gps_state_start(s);
    return 0;
}

static int
virtual_gps_stop()
{
    GpsState *s = _gps_state;

    if (!s->init)
    {
        D("%s: called with uninitialized state !!", __FUNCTION__);
        return -1;
    }

    D("%s: called", __FUNCTION__);
    gps_state_stop(s);
    return 0;
}

static int
virtual_gps_inject_time(GpsUtcTime __unused time,
                        int64_t __unused timeReference,
                        int __unused uncertainty)
{
    return 0;
}

static int
virtual_gps_inject_location(double __unused latitude,
                            double __unused longitude,
                            float __unused accuracy)
{
    return 0;
}

static void
virtual_gps_delete_aiding_data(GpsAidingData __unused flags)
{
}

static int virtual_gps_set_position_mode(GpsPositionMode __unused mode,
                                         GpsPositionRecurrence __unused recurrence,
                                         uint32_t __unused min_interval,
                                         uint32_t __unused preferred_accuracy,
                                         uint32_t __unused preferred_time)
{
    // FIXME - support fix_frequency
    return 0;
}

static const void *
virtual_gps_get_extension(const char *__unused name)
{
    // no extensions supported
    return NULL;
}

static const GpsInterface qemuGpsInterface = {
    sizeof(GpsInterface),
    virtual_gps_init,
    virtual_gps_start,
    virtual_gps_stop,
    virtual_gps_cleanup,
    virtual_gps_inject_time,
    virtual_gps_inject_location,
    virtual_gps_delete_aiding_data,
    virtual_gps_set_position_mode,
    virtual_gps_get_extension,
};

const GpsInterface *gps__get_gps_interface(struct gps_device_t *__unused dev)
{
    return &qemuGpsInterface;
}

static int open_gps(const struct hw_module_t *module,
                    char const *__unused name,
                    struct hw_device_t **device)
{
    struct gps_device_t *dev = malloc(sizeof(struct gps_device_t));
    memset(dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t *)module;
    //    dev->common.close = (int (*)(struct hw_device_t*))close_lights;
    dev->get_gps_interface = gps__get_gps_interface;

    *device = (struct hw_device_t *)dev;
    return 0;
}

static struct hw_module_methods_t gps_module_methods = {
    .open = open_gps};

struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = GPS_HARDWARE_MODULE_ID,
    .name = "Goldfish GPS Module",
    .author = "The Android Open Source Project",
    .methods = &gps_module_methods,
};