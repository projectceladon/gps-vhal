/* Copyright (C) 2007-2008 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/
/*
 *  Android emulator control console
 *
 *  this console is enabled automatically at emulator startup, on _gps_socket_info->port 5554 by default,
 *  unless some other emulator is already running. See (android_emulation_start in android_sdl.c
 *  for details)
 *
 *  you can telnet to the console, then use commands like 'help' or others to dynamically
 *  change emulator settings.
 *
 */
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>

#define S_SIZE 1024

enum
{
    CMD_QUIT = 0,
    CMD_START = 1,
    CMD_STOP = 2
};

typedef struct
{
    int sock_client_fd;
    int epoll_fd;
    int loop_quit;                 // Control injection_gps_data_thread
    pthread_t injection_thread_id; // injection_gps_data_thread
    char server_ip[20];
    int port;
    double gps_lat;
    double gps_long;
    double gps_alt;
    int loop_exit; // Control receive_server_command_thread
} GpsSocketInfo;

static GpsSocketInfo _gps_socket_info[1];

void *receive_server_command_thread(void *args);
static int connect_gps_server(GpsSocketInfo *gsi);
void *injection_gps_data_thread(void *args);
static int do_geo_fix(int fd, char *args);
static int do_geo_nmea(int fd, char *args);
static int send_message(int fd, const char *sentence, size_t n);

char *const short_options = "l:o:a:hs:p:";
struct option long_options[] = {
    {"lat", 0, NULL, 'l'},
    {"long", 1, NULL, 'o'},
    {"alt", 1, NULL, 'a'},
    {"help", 1, NULL, 'h'},
    {"server-ip", 1, NULL, 's'},
    {"port", 1, NULL, 'p'},
    {0, 0, 0, 0},
};

int main(int argc, char *argv[])
{
    int c;
    int index = 0;
    char *p_opt_arg = NULL;

    printf("Set default value:\n");
    strncpy(_gps_socket_info->server_ip, "172.100.0.2", 20);
    _gps_socket_info->port = 8766;
    _gps_socket_info->gps_lat = 121.38215;
    _gps_socket_info->gps_long = 31.07147;
    _gps_socket_info->gps_alt = 4;
    _gps_socket_info->loop_exit = 0;

    while ((c = getopt_long(argc, argv, short_options, long_options, &index)) != -1)
    {
        switch (c)
        {
        case 'l':
            p_opt_arg = optarg;
            _gps_socket_info->gps_lat = strtod(p_opt_arg, NULL);
            printf("_gps_socket_info->gps_lat = %lf\n", _gps_socket_info->gps_lat);
            break;
        case 'o':
            p_opt_arg = optarg;
            _gps_socket_info->gps_long = strtod(p_opt_arg, NULL);
            printf("_gps_socket_info->gps_long = %lf\n", _gps_socket_info->gps_long);
            break;
        case 'a':
            p_opt_arg = optarg;
            _gps_socket_info->gps_alt = strtod(p_opt_arg, NULL);
            printf("_gps_socket_info->gps_alt = %lf\n", _gps_socket_info->gps_alt);
            break;
        case 's':
            p_opt_arg = optarg;
            strncpy(_gps_socket_info->server_ip, p_opt_arg, sizeof(_gps_socket_info->server_ip));
            printf("Set _gps_socket_info->server_ip to %s\n", _gps_socket_info->server_ip);
            break;
        case 'p':
            p_opt_arg = optarg;
            _gps_socket_info->port = atoi(p_opt_arg);
            printf("Set _gps_socket_info->port to %d\n", _gps_socket_info->port);
            break;
        case 'h':
            printf("%s\n"
                   "\t-l, --lat lat\n"
                   "\t-o, --long long\n"
                   "\t-a, --alt alt\n"
                   "\t-c, --count count\n"
                   "\t-h, --help help\n"
                   "\t-s, --server-ip server-ip\n"
                   "\t-p, --port\n",
                   argv[0]);
            break;
        default:
            printf("Enock: c = %c, index =%d \n", c, index);
        }
    }

    printf("_gps_socket_info->server_ip = %s _gps_socket_info->port = %d\n", _gps_socket_info->server_ip, _gps_socket_info->port);

    char str[1024];
    int flag = 1;
    pthread_t accept_thread_id; //accept server command thread id
    int accept_thread_flag = 0;

    while (flag)
    {
        memset(str, 0, sizeof(str));
        printf("%s Please input comand('quit' for quit):", __func__);
        scanf("%s", str);
        if (strcmp(str, "quit") == 0)
        {
            printf("%s quit\n", __func__);
            flag = 0;
            break;
        }
        printf("%s The command is : %s\n\n", __func__, str);
        if (strcmp(str, "start") == 0)
        {
            printf("%s start\n", __func__);
            if (!accept_thread_flag)
            {
                _gps_socket_info->loop_exit = 0;
                _gps_socket_info->loop_quit = 0;
                pthread_create(&accept_thread_id, NULL, receive_server_command_thread, _gps_socket_info);
                accept_thread_flag = 1;
            }
            else
            {
                printf("%s Accept server command thread is already running\n", __func__);
            }
        }
        if (strcmp(str, "stop") == 0)
        {
            printf("%s stop\n", __func__);
            if (accept_thread_flag)
            {
                _gps_socket_info->loop_exit = 1;
                _gps_socket_info->loop_quit = 1;
                pthread_join(accept_thread_id, NULL);
                accept_thread_flag = 0;
            };
        }
    }

    printf("%s Quit\n", __func__);
    return 0;
}

void *receive_server_command_thread(void *args)
{
    GpsSocketInfo *gsi = (GpsSocketInfo *)args;
    gsi->sock_client_fd = -1;
    int injection_thread_flag = 0;
    while (gsi->sock_client_fd < 0)
    {
        printf("%s Try to connect GPS socket server...\n", __func__);
        gsi->sock_client_fd = connect_gps_server(gsi);
        if (gsi->sock_client_fd < 0 && gsi->loop_exit == 0)
        {
            printf("Fail to connect GPS server(%s:%d).\n", _gps_socket_info->server_ip, _gps_socket_info->port);
            continue;
        }
    }

    printf("Connected to GPS socket server(%d)\n", gsi->sock_client_fd);
    int epoll_fd = epoll_create(1);
    if (epoll_fd < 0)
    {
        printf("%s Fail to create epoll fd.", __func__);
        goto Fail;
    }

    struct epoll_event ev;
    int ret, flags;

    /* important: make the fd non-blocking */
    flags = fcntl(gsi->sock_client_fd, F_GETFL);
    fcntl(gsi->sock_client_fd, F_SETFL, flags | O_NONBLOCK);

    ev.events = EPOLLIN;
    ev.data.fd = gsi->sock_client_fd;
    do
    {
        ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, gsi->sock_client_fd, &ev);
    } while (ret < 0 && errno == EINTR);

    gsi->epoll_fd = epoll_fd;

    for (;;)
    {
        if (gsi->loop_exit)
        {
            printf("%s Loop exit\n", __func__);
            break;
        }
        struct epoll_event events[1];
        int ne, nevents;

        int timeout = 10 * 1000; // 10 seconds

        nevents = epoll_wait(gsi->epoll_fd, events, 1, timeout);
        if (nevents < 0)
        {
            if (errno != EINTR)
                printf("%s epoll_wait() unexpected error: %s\n", __func__, strerror(errno));
            continue;
        }
        printf("%s received %d events\n", __func__, nevents);
        for (ne = 0; ne < nevents; ne++)
        {
            if ((events[ne].events & (EPOLLERR | EPOLLHUP)) != 0)
            {
                printf("%s EPOLLERR or EPOLLHUP after epoll_wait() !?\n", __func__);
                int fd = events[ne].data.fd;
                if (fd == gsi->sock_client_fd)
                {
                    printf("%s GPS socket server may close.\n", __func__);
                    int ret;
                    do
                    {
                        ret = epoll_ctl(gsi->epoll_fd, EPOLL_CTL_DEL, gsi->sock_client_fd, NULL);
                    } while (ret < 0 && errno == EINTR);

                    gsi->sock_client_fd = -1;
                    gsi->loop_exit = 1;
                    gsi->loop_quit = 1;
                    break;
                }
            }
            if ((events[ne].events & EPOLLIN) != 0)
            {
                int fd = events[ne].data.fd;

                if (fd == gsi->sock_client_fd)
                {
                    char cmd = 0xFF;
                    int ret;
                    printf("%s gps control fd event\n", __func__);
                    do
                    {
                        ret = read(gsi->sock_client_fd, &cmd, 1);
                    } while (ret < 0 && errno == EINTR);

                    if (ret == 0)
                    {
                        printf("%s:%d GPS socket server may close.\n", __func__, __LINE__);
                        int ret;
                        do
                        {
                            ret = epoll_ctl(gsi->epoll_fd, EPOLL_CTL_DEL, gsi->sock_client_fd, NULL);
                        } while (ret < 0 && errno == EINTR);

                        gsi->sock_client_fd = -1;
                        gsi->loop_exit = 1;
                        gsi->loop_quit = 1;
                        break;
                    }

                    if (cmd == CMD_QUIT)
                    {
                        printf("%s gps thread quitting on demand\n", __func__);
                        return NULL;
                    }
                    else if (cmd == CMD_START)
                    {
                        if (!injection_thread_flag)
                        {
                            printf("%s gps thread starting on demand\n", __func__);
                            injection_thread_flag = 1;
                            gsi->loop_quit = 0;
                            pthread_create(&gsi->injection_thread_id, NULL, injection_gps_data_thread, gsi);
                        }
                    }
                    else if (cmd == CMD_STOP)
                    {
                        if (injection_thread_flag)
                        {
                            printf("%s gps thread stopping on demand\n", __func__);
                            gsi->loop_quit = 1;
                            injection_thread_flag = 0;
                            pthread_join(gsi->injection_thread_id, NULL);
                        }
                    }
                    else
                    {
                        printf("%s Unknown command.\n", __func__);
                    }
                }
            }
        }
    }

Fail:
    shutdown(gsi->sock_client_fd, SHUT_RDWR);
    close(gsi->sock_client_fd);
    gsi->sock_client_fd = -1;
    close(gsi->epoll_fd);
    gsi->epoll_fd = -1;
    pthread_join(gsi->injection_thread_id, NULL);
    printf("%s Quit\n", __func__);
    return NULL;
}

static int connect_gps_server(GpsSocketInfo *gsi)
{
    struct sockaddr_in addr;
    gsi->sock_client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (gsi->sock_client_fd < 0)
    {
        printf("Can't create socket\n");
        return -1;
    }

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(gsi->port);
    inet_pton(AF_INET, gsi->server_ip, &addr.sin_addr);

    if (connect(gsi->sock_client_fd, (struct sockaddr *)&addr,
                sizeof(struct sockaddr_in)) < 0)
    {
        printf("Failed to connect to server socket %s:%d error: %s\n", gsi->server_ip, gsi->port, strerror(errno));
        close(gsi->sock_client_fd);
        gsi->sock_client_fd = -1;
        return -1;
    }
    printf("Connect GPS server successfully(%s:%d).\n", gsi->server_ip, gsi->port);
    return gsi->sock_client_fd;
}

void *injection_gps_data_thread(void *args)
{
    GpsSocketInfo *g = (GpsSocketInfo *)args;
    char lat_long[32];
    size_t lat_long_len = 0;
    int ret = 0;

    while (!g->loop_quit)
    {
        printf("%s FIXME: Replace the data by real data.\n", __func__);
        memset(lat_long, 0, sizeof(lat_long));
        g->gps_lat = g->gps_lat + 0.00001;
        if (g->gps_lat > 122.38215)
        {
            g->gps_lat = 121.38215;
        }
        _gps_socket_info->gps_long = _gps_socket_info->gps_long + 0.00001;
        if (_gps_socket_info->gps_long > 32.07147)
        {
            _gps_socket_info->gps_long = 31.07147;
        }
        _gps_socket_info->gps_alt = _gps_socket_info->gps_alt + 1;
        if (_gps_socket_info->gps_alt > 9)
        {
            _gps_socket_info->gps_alt = 4;
        }
        lat_long_len = sprintf(lat_long, "%.5f %.5f %.1f 5 6", g->gps_lat, g->gps_long, g->gps_alt);
        lat_long[lat_long_len] = '\0';
        printf("%s GPS 1HZ. Sleep 1s.\n", __func__);
        sleep(1);
        printf("%s Execute command: geo fix %s\n", __func__, lat_long);
        ret = do_geo_fix(g->sock_client_fd, lat_long);
        if (ret == 0)
        {
            printf("%s Detected GPS server socket is closed. Quit.\n", __func__);
            break;
        }
    }
    return NULL;
}

/********************************************************************************************/
/********************************************************************************************/
/*****                                                                                 ******/
/*****                             G E O   C O M M A N D S                             ******/
/*****                                                                                 ******/
/********************************************************************************************/
/********************************************************************************************/

static int do_geo_fix(int fd, char *args)
{
    // GEO_SAT2 provides bug backwards compatibility.
    enum
    {
        GEO_LONG = 0,
        GEO_LAT,
        GEO_ALT,
        GEO_SAT,
        GEO_SAT2,
        NUM_GEO_PARAMS
    };
    char *p = args;
    int top_param = -1;
    double params[NUM_GEO_PARAMS];
    int n_satellites = 1;
    int ret = -1;

    if (!p)
        p = "";

    /* tokenize */
    while (*p)
    {
        char *end;
        double val = strtod(p, &end);

        if (end == p)
        {
            printf("KO: argument '%s' is not a number\n", p);
            return -1;
        }

        params[++top_param] = val;
        if (top_param + 1 == NUM_GEO_PARAMS)
            break;

        p = end;
        while (*p && (p[0] == ' ' || p[0] == '\t'))
            p += 1;
    }

    /* sanity check */
    if (top_param < GEO_LAT)
    {
        printf("KO: not enough arguments: see 'help geo fix' for details\r\n");
        return -1;
    }

    /* check number of satellites, must be integer between 1 and 12 */
    if (top_param >= GEO_SAT)
    {
        int sat_index = (top_param >= GEO_SAT2) ? GEO_SAT2 : GEO_SAT;
        n_satellites = (int)params[sat_index];
        if (n_satellites != params[sat_index] || n_satellites < 1 || n_satellites > 12)
        {
            printf("KO: invalid number of satellites. Must be an integer between 1 and 12\r\n");
            return -1;
        }
    }

    /* generate an NMEA sentence for this fix */
    {
        char s[S_SIZE];
        size_t s_len = 0;
        double val;
        int deg, min;
        char hemi;
        int hh = 0, mm = 0, ss = 0;

        printf("Note: currently, the size of s is fixed(S_SIZE: %d)\n", S_SIZE);
        memset(s, 0, S_SIZE);
        /* format overview:
         *    time of fix      123519     12:35:19 UTC
         *    latitude         4807.038   48 degrees, 07.038 minutes
         *    north/south      N or S
         *    longitude        01131.000  11 degrees, 31. minutes
         *    east/west        E or W
         *    fix quality      1          standard GPS fix
         *    satellites       1 to 12    number of satellites being tracked
         *    HDOP             <dontcare> horizontal dilution
         *    altitude         546.       altitude above sea-level
         *    altitude units   M          to indicate meters
         *    diff             <dontcare> height of sea-level above ellipsoid
         *    diff units       M          to indicate meters (should be <dontcare>)
         *    dgps age         <dontcare> time in seconds since last DGPS fix
         *    dgps sid         <dontcare> DGPS station id
         */

        // Get the current time as hh:mm:ss
        struct timeval tm;

        if (0 == gettimeofday(&tm, NULL))
        {
            // tm.tv_sec is elapsed seconds since epoch (UTC, which is what we want)
            hh = (int)(tm.tv_sec / (60 * 60)) % 24;
            mm = (int)(tm.tv_sec / 60) % 60;
            ss = (int)(tm.tv_sec) % 60;
        }

        s_len += sprintf(s + s_len, "$GPGGA,%02d%02d%02d", hh, mm, ss);

        /* then the latitude */
        hemi = 'N';
        val = params[GEO_LAT];
        if (val < 0)
        {
            hemi = 'S';
            val = -val;
        }
        deg = (int)val;
        val = 60 * (val - deg);
        min = (int)val;
        val = 10000 * (val - min);
        s_len += sprintf(s + s_len, ",%02d%02d.%04d,%c", deg, min, (int)val, hemi);

        /* the longitude */
        hemi = 'E';
        val = params[GEO_LONG];
        if (val < 0)
        {
            hemi = 'W';
            val = -val;
        }
        deg = (int)val;
        val = 60 * (val - deg);
        min = (int)val;
        val = 10000 * (val - min);
        s_len += sprintf(s + s_len, ",%02d%02d.%04d,%c", deg, min, (int)val, hemi);

        /* bogus fix quality, satellite count and dilution */
        s_len += sprintf(s + s_len, ",1,%02d,", n_satellites);

        /* optional altitude + bogus diff */
        if (top_param >= GEO_ALT)
        {
            s_len += sprintf(s + s_len, ",%.1g,M,0.,M", params[GEO_ALT]);
        }
        else
        {
            s_len += sprintf(s + s_len, ",,,,");
        }
        /* bogus rest and checksum */
        s_len += sprintf(s + s_len, ",,,*47");
        s[s_len] = '\n';
        s_len++;

        /* send it, then free */
        ret = send_message(fd, s, s_len);
        if (ret > 0)
        {
            s[s_len] = '\0';
            printf("Success to send s_len = %zd, ret = %d, s = %s\n", s_len, ret, s);
        }
        else
        {
            printf("Fail to send s_len = %zd, ret = %d, s = %s\n", s_len, ret, s);
        }
    }
    return ret;
}

static int do_geo_nmea(int fd, char *args)
{
    if (!args)
    {
        printf("KO: NMEA sentence missing, try 'help geo nmea'\r\n");
        return -1;
    }
    if (fd < 0)
    {
        printf("KO: have not connect to GPS server\r\n");
        return -1;
    }
    char s[S_SIZE];
    size_t s_len = 0;
    int ret = -1;
    printf("Note: currently, the size of s is fixed(S_SIZE: %d)\n", S_SIZE);
    memset(s, 0, S_SIZE);
    s_len = strlen(args);
    strncpy(s, args, s_len);
    s[s_len++] = '\n';

    ret = send_message(fd, s, s_len);
    if (ret > 0)
    {
        printf("Data: %s\n", args);
        printf("%s set ret(%d) data to GPS socket.\n", __func__, ret);
    }
    return ret;
}

static int send_message(int fd, const char *sentence, size_t n)
{
    int ret = -1;

    do
    {
        printf("Write %zd %s to GPS socket(%d). \n", n, sentence, fd);
        ret = write(fd, sentence, n);
    } while (ret < 0 && errno == EINTR);
    printf("%d is written to GPS server.\n", ret);
    return ret;
}