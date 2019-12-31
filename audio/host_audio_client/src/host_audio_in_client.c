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

#define OUTBUFFERSIZE 1024
static int global_audio_in_client_fd;
static const char *k_sock = "./workdir/ipc/audio-in-sock0";
static const char *pcm_file = "./yrzr_8000_mono.pcm";
char *const short_options = "h";
struct option long_options[] = {
    {"help", 1, NULL, 'h'},
    {0, 0, 0, 0},
};
int loop_exit = 1;

static int connect_audio_in_server();
void *audio_in_worker_thread(void *args);

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
        printf("Please input command('quit' for quit):");
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
                pthread_create(&thread_id, NULL, audio_in_worker_thread, NULL);
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

static int connect_audio_in_server()
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

void *audio_in_worker_thread(void *args)
{
    global_audio_in_client_fd = connect_audio_in_server();
    if (global_audio_in_client_fd < 0)
    {
        printf("\tFail to connect %s.\n", k_sock);
        return NULL;
    }

    void *out_buffer = malloc(OUTBUFFERSIZE);
    memset(out_buffer, 0, OUTBUFFERSIZE);
    size_t out_size = 320;
    ssize_t write_len = 0;
    long count = 1;
    FILE *file_pcm = fopen(pcm_file, "rb");
    int ret = 0;
    loop_exit = 0;
    while (!loop_exit)
    {
        if (feof(file_pcm))
        {
            fclose(file_pcm);
            file_pcm = fopen(pcm_file, "rb");
            if (!file_pcm)
            {
                printf("\t%s:%d Fail to open %s. %s\n", __func__, __LINE__, pcm_file, strerror(errno));
                break;
            }
        }
        ret = fread(out_buffer, out_size, 1, file_pcm);
        if (ret < 0)
        {
            printf("\t%s: %d Fail to read %s\n", __func__, __LINE__, pcm_file);
            break;
        }

        write_len = write(global_audio_in_client_fd, out_buffer, out_size);
        if (write_len > 0)
        {

            printf("\tcount: %ld Write %zd to %s\n", count++, write_len, k_sock);
        }
        else if (write_len == 0)
        {
            printf("\tcount: %ld Audio in server closed\n", count++);
            break;
        }
        else
        {
            printf("\tcount: %ld Write error: %s\n", count++, strerror(errno));
            break;
        }
    }
    free(out_buffer);
    out_buffer = NULL;
    fclose(file_pcm);
    file_pcm = NULL;
    loop_exit = 1;
    printf("%s Quit\n", __func__);
    return 0;
}