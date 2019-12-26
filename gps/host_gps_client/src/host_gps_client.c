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
 *  this console is enabled automatically at emulator startup, on port 5554 by default,
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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>

#define S_SIZE 1024
static int global_sock_client_fd;

static int connect_gps_server()
{
    const char *k_sock = "./workdir/ipc/gps-sock0";
    struct sockaddr_un addr;
    int sock_client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_client_fd < 0)
    {
        printf("Can't create socket\n");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[0], k_sock, strlen(k_sock));
    if (connect(sock_client_fd, (struct sockaddr *)&addr,
                sizeof(sa_family_t) + strlen(k_sock) + 1) < 0)
    {
        printf("Can't connect to remote\n");
        perror("Failed to connect to serversock\n");
        close(sock_client_fd);
        sock_client_fd = -1;
        return -1;
    }
    printf("Connect GPS server sucessfully.\n");
    return sock_client_fd;
}

static int send_message(const char *sentence, size_t n)
{
    int ret;

    do
    {
        printf("Write %zd %s to GPS socket(%d). \n", n, sentence, global_sock_client_fd);
        ret = write(global_sock_client_fd, sentence, n);
    } while (ret < 0 && errno == EINTR);
    printf("%d is writen to GPS server.\n", ret);
    return ret;
}

/********************************************************************************************/
/********************************************************************************************/
/*****                                                                                 ******/
/*****                             G E O   C O M M A N D S                             ******/
/*****                                                                                 ******/
/********************************************************************************************/
/********************************************************************************************/

static int
do_geo_nmea(char *args)
{
    if (!args)
    {
        printf("KO: NMEA sentence missing, try 'help geo nmea'\r\n");
        return -1;
    }
    if (global_sock_client_fd < 0)
    {
        printf("KO: have not connect to GPS server\r\n");
        return -1;
    }
    char s[S_SIZE];
    size_t s_len = 0;
    int ret = 0;
    printf("Note: currently, the size of s is fixed(S_SIZE: %d)\n", S_SIZE);
    memset(s, 0, S_SIZE);
    s_len = strlen(args);
    strncpy(s, args, s_len);
    s[s_len++] = '\n';

    ret = send_message(s, s_len);
    if (ret > 0)
    {
        printf("Data: %s\n", args);
        printf("%s set ret(%d) data to GPS socket.\n", __func__, ret);
    }
    return 0;
}

static int
do_geo_fix(char *args)
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
        int ret = send_message(s, s_len);
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
    return 0;
}

char *const short_options = "l:o:a:c:h";
struct option long_options[] = {
    {"lat", 0, NULL, 'l'},
    {"long", 1, NULL, 'o'},
    {"alt", 1, NULL, 'a'},
    {"count", 1, NULL, 'c'},
    {"help", 1, NULL, 'h'},
    {0, 0, 0, 0},
};

double gps_lat = 121.38215;
double gps_long = 31.07147;
double gps_alt = 4;
int gps_count = 10000;
int loop_exit = 1;
void *injection_gps_data(void *args)
{
    int count = 0;
    char lat_long[32];
    size_t lat_long_len = 0;
    count = 0;
    loop_exit = 0;
    while (count++ < gps_count)
    {
        memset(lat_long, 0, sizeof(lat_long));
        gps_lat = gps_lat + 0.00001;
        if (gps_lat > 122.38215)
        {
            gps_lat = 121.38215;
        }
        gps_long = gps_long + 0.00001;
        if (gps_long > 32.07147)
        {
            gps_long = 31.07147;
        }
        gps_alt = gps_alt + 1;
        if (gps_alt > 9)
        {
            gps_alt = 4;
        }
        lat_long_len = sprintf(lat_long, "%.5f %.5f %.1f 5 6", gps_lat, gps_long, gps_alt);
        lat_long[lat_long_len] = '\0';
        printf("GPS 1HZ. Sleep 1s.\n");
        sleep(1);
        printf("Execute command(count:%d): geo fix %s\n", count, lat_long);
        do_geo_fix(lat_long);
        if (loop_exit)
        {
            printf("%s exit\n", __func__);
            break;
        }
    }
    loop_exit = 1;
}
int main(int argc, char *argv[])
{
    int c;
    int index = 0;
    char *p_opt_arg = NULL;

    while ((c = getopt_long(argc, argv, short_options, long_options, &index)) != -1)
    {
        switch (c)
        {
        case 'l':
            p_opt_arg = optarg;
            gps_lat = strtod(p_opt_arg, NULL);
            printf("gps_lat = %lf\n", gps_lat);
            break;
        case 'o':
            p_opt_arg = optarg;
            gps_long = strtod(p_opt_arg, NULL);
            printf("gps_long = %lf\n", gps_long);
            break;
        case 'a':
            p_opt_arg = optarg;
            gps_alt = strtod(p_opt_arg, NULL);
            printf("gps_alt = %lf\n", gps_alt);
            break;
        case 'c':
            p_opt_arg = optarg;
            gps_count = atoi(p_opt_arg);
            printf("gps_count = %d\n", gps_count);
            break;
        case 'h':
            printf("%s\n\
            \t-l, --lat lat\n\
            \t-o, --long long\n\
            \t-a, --alt alt\n\
            \t-c, --count count\n\
            \t-h, --help help\n",
                   argv[0]);
            break;
        default:
            printf("Enock: c = %c, index =%d \n", c, index);
        }
    }

    char str[1024];
    int flag = 1;

    while (flag)
    {
        memset(str, 0, sizeof(str));
        printf("Please input comand('quit' for quit):");
        scanf("%s", str);
        if (strcmp(str, "quit") == 0)
        {
            flag = 0;
            break;
        }
        printf("The command is : %s\n\n", str);
        if (strcmp(str, "run") == 0)
        {
            global_sock_client_fd = connect_gps_server();
            if (global_sock_client_fd < 0)
            {
                printf("Fail to connect GPS server.\n");
                continue;
            }
            if (loop_exit == 1)
            {
                pthread_t thread_id;
                pthread_create(&thread_id, NULL, injection_gps_data, NULL);
            } else {
                printf("injection_gps_data thread is running\n");
            }
        }
        if (strcmp(str, "stop") == 0)
        {
            loop_exit = 1;
        }
    }

    printf("Quit\n");
    return 0;
}