#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <pthread.h>

static int global_audio_out_client_fd;
static const char *k_sock = "./workdir/ipc/audio-out-sock0";
static const char *pcm_file = "./audio_out_sock0.pcm";
char *const short_options = "h";
struct option long_options[] = {
    {"help", 1, NULL, 'h'},
    {0, 0, 0, 0},
};
int loop_exit = 1;

static int connect_audio_out_server();
void *audio_out_worker_thread(void *args);

int main(int argc, char *argv[])
{
    int c;
    int index = 0;

    while ((c = getopt_long(argc, argv, short_options, long_options, &index)) != -1)
    {
        switch (c)
        {
        case 'h':
            printf("%s\t-h, --help help\n",
                   argv[0]);
            break;
        default:
            printf("Enock: c = %c, index =%d \n", c, index);
        }
    }

    char str[1024];
    int flag = 1;
    loop_exit = 1;

    while (flag)
    {
        memset(str, '\0', sizeof(str));
        printf("Please input comand('quit' for quit):");
        scanf("%s", str);
        if (strcmp(str, "quit") == 0)
        {
            loop_exit = 1;
            flag = 0;
            break;
        }
        printf("The command is : %s\n\n", str);
        if (strcmp(str, "run") == 0)
        {
            if (loop_exit == 1)
            {
                pthread_t thread_id;
                pthread_create(&thread_id, NULL, audio_out_worker_thread, NULL);
            }
            else
            {
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
void *audio_out_worker_thread(void *args)
{
    global_audio_out_client_fd = connect_audio_out_server();
    if (global_audio_out_client_fd < 0)
    {
        printf("\tFail to connect %s.\n", k_sock);
        return NULL;
    }

    FILE *g_paudio_record_file = fopen(pcm_file, "w");
    if (!g_paudio_record_file)
    {
        printf("\t%s:%d Fail to open %s. %s\n", __func__, __LINE__, pcm_file, strerror(errno));
    }
    void *in_buffer = malloc(4096);

    ssize_t read_len = 0;
    long count = 1;
    int ret = 0;
    loop_exit = 0;
    while (!loop_exit)
    {
        read_len = read(global_audio_out_client_fd, in_buffer, 3600);
        if (read_len > 0)
            printf("\tRead %zd\n", read_len);
        else if (read_len == 0)
        {
            printf("\tAudio out server closed. %zd\n", read_len);
            break;
        }
        else
        {
            printf("\tRead error happen. Error: %s\n", strerror(read_len));
            break;
        }
        ret = fwrite(in_buffer, 1, read_len, g_paudio_record_file);
        printf("\tcount: %ld Read from %s: read_len: %zd\n", count++, k_sock, read_len);
        fflush(g_paudio_record_file);
    }

    close(global_audio_out_client_fd);
    global_audio_out_client_fd = -1;
    fclose(g_paudio_record_file);
    g_paudio_record_file = NULL;
    free(in_buffer);
    in_buffer = NULL;
    loop_exit = 1;
    printf("\t%s Quit\n", __func__);
    return 0;
}

static int connect_audio_out_server()
{
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
    while (1)
    {
        printf("Try to connect %s server\n", k_sock);
        if (connect(sock_client_fd, (struct sockaddr *)&addr,
                    sizeof(sa_family_t) + strlen(k_sock) + 1) < 0)
        {
            printf("Fail to connect to %s\n", k_sock);
            close(sock_client_fd);
            sock_client_fd = -1;
            sleep(1);
            continue;
        }
        printf("Connect %s sucessfully.\n", k_sock);
        break;
    }
    return sock_client_fd;
}