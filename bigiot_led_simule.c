#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <cjson/cJSON.h>

#include <fcntl.h>
#include <termios.h>

#define DOMAIN "www.bigiot.net"
#define PORT 8181
#define IP "121.42.180.30"
#define ID "xxxx"
#define APIKEY "xxxxxxxx"

#define DID "xxxxx"

#define PASSWORD "xxxxxxxx"

#define DEV_NAME "/dev/ttyUSB0"

void *pthread_keepalive(void *);
void *pthread_handler(void *);
void *pthread_serialport(void *);
void *pthread_upload(void *);

int fd;
void init_serialport(void);
int len_str_cmd;
unsigned char *str_cmd[1024];
int len;

struct task
{
    pthread_t tidp;
    int micro_seconds;
    void *(*start_rtn)(void *);
};

struct task task_tbl[] = {
    {0, 30000000, pthread_keepalive},
    {0, 100000, pthread_handler},
    {0, 10000, pthread_serialport},
    {0, 60000000, pthread_upload},
};

int s;
int ret;
struct sockaddr_in bigiot_addr;
char buf[1024];
cJSON *cjson = NULL;
char *str_cjson = NULL;

void show_cjson(void)
{
    cjson = cJSON_Parse(buf);
    str_cjson = cJSON_PrintUnformatted(cjson);

    printf("%s\n", str_cjson);
}

struct cmd_oper
{
    char *cmd;
    void (*fun)();
};

void reboot(void)
{
    system("echo " PASSWORD " | sudo -S reboot");
}

struct cmd_oper cmd_oper_tbl[] = {
    {"reboot", reboot},
};

void cmd_handler(void)
{
    char *method = NULL;
    char *cmd = NULL;
    int i;

    cjson = cJSON_Parse(buf);
#if 0
    printf("%s\n", cJSON_GetObjectItem(cjson, "M")->valuestring);
    printf("%s\n", cJSON_GetObjectItem(cjson, "ID")->valuestring);
    printf("%s\n", cJSON_GetObjectItem(cjson, "NAME")->valuestring);
    printf("%s\n", cJSON_GetObjectItem(cjson, "C")->valuestring);
    printf("%s\n", cJSON_GetObjectItem(cjson, "T")->valuestring);
#else
    method = cJSON_GetObjectItem(cjson, "M")->valuestring;

    if (0 == strcmp(method, "say"))
    {
        cmd = cJSON_GetObjectItem(cjson, "C")->valuestring;

        for (i = 0; i < sizeof(cmd_oper_tbl) / sizeof(cmd_oper_tbl[0]); i++)
        {
            if (0 == strcmp(cmd, cmd_oper_tbl[i].cmd))
            {
                cmd_oper_tbl[i].fun();

                break;
            }
        }

        len_str_cmd = strlen(cmd);
        memcpy(str_cmd, cmd, len_str_cmd);

        if (0 == len_str_cmd)
        {
            // do_nothing();
        }
        else
        {
            len = 0;
            len = write(fd, str_cmd, len_str_cmd);

            if (len > 0)
            {
                printf("cmd send ok!\n");
            }

            len_str_cmd = 0;

            write(fd, '\n', 1);
        }
    }
#endif
}

int main(int argc, char *argv[])
{
    cJSON *checkin = NULL;
    char *str_checkin = NULL;

    int i;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
        exit(-1);
    }

    bigiot_addr.sin_family = AF_INET;
    bigiot_addr.sin_port = htons(PORT);

#if 0
    bigiot_addr.sin_addr.s_addr = inet_addr(IP);
#else
    struct hostent *h;

    h = gethostbyname(DOMAIN);

    printf("ip:");
    for (i = 0; h->h_addr_list[i]; i++)
    {
        printf("%s\t", inet_ntoa(*(struct in_addr *)(h->h_addr_list[i])));
    }
    printf("\n");

    char *ip = inet_ntoa(*((struct in_addr *)h->h_addr_list[0]));

    bigiot_addr.sin_addr.s_addr = inet_addr(ip);
#endif

    ret = connect(s, (const struct sockaddr *)&bigiot_addr, sizeof(bigiot_addr));
    if (ret < 0)
    {
        exit(-2);
    }

    memset(buf, 0, sizeof(buf));

    ret = recv(s, buf, sizeof(buf), 0);
    if (ret > 0)
    {
        show_cjson();
    }

    checkin = cJSON_CreateObject();

    if (NULL == checkin)
    {
        printf("cJSON error!\n");
        exit(-1);
    }

    cJSON_AddStringToObject(checkin, "M", "checkin");
    cJSON_AddStringToObject(checkin, "ID", ID);
    cJSON_AddStringToObject(checkin, "K", APIKEY);

    str_checkin = cJSON_PrintUnformatted(checkin);

    if (NULL == str_checkin)
    {
        printf("cJSON error!\n");
        exit(-1);
    }

    strcat(str_checkin, "\n");

    ret = send(s, str_checkin, strlen(str_checkin), 0);

    if (ret < 0)
    {
        printf("send error!\n");
        exit(-3);
    }

    printf("%s", str_checkin);

    ret = recv(s, buf, sizeof(buf), 0);
    if (ret > 0)
    {
        show_cjson();
    }

    init_serialport();

    for (i = 0; i < sizeof(task_tbl) / sizeof(task_tbl[0]); i++)
    {
        ret = pthread_create(&task_tbl[i].tidp,
                             NULL,
                             task_tbl[i].start_rtn,
                             &task_tbl[i].micro_seconds);

        if (ret)
        {
            printf("Create pthread error:%d\n", i);

            exit(-1);
        }
    }

    for (i = 0; i < sizeof(task_tbl) / sizeof(task_tbl[0]); i++)
    {
        pthread_join(task_tbl[i].tidp, NULL);
    }

    close(s);

    return 0;
}

void *pthread_keepalive(void *arg)
{
    cJSON *beat = NULL;
    char *str_beat = NULL;

    beat = cJSON_CreateObject();

    if (NULL == beat)
    {
        printf("cJSON error!\n");
        exit(-1);
    }

    cJSON_AddStringToObject(beat, "M", "beat");

    str_beat = cJSON_PrintUnformatted(beat);

    if (NULL == str_beat)
    {
        printf("cJSON error!\n");
        exit(-1);
    }

    strcat(str_beat, "\n");

    while (1)
    {
        ret = send(s, str_beat, strlen(str_beat), 0);
        if (ret < 0)
        {
            printf("send error!\n");
            exit(-3);
        }

        printf("%s", str_beat);

        usleep(*(int *)arg);
    }

    return NULL;
}

void *pthread_handler(void *arg)
{
    cJSON *cjson = NULL;
    char *str_cjson = NULL;

    while (1)
    {
        memset(buf, 0, sizeof(buf));

        ret = recv(s, buf, sizeof(buf), 0);

        if (ret > 0)
        {
            show_cjson();

            cmd_handler();
        }

        usleep(*(int *)arg);
    }

    return NULL;
}

void *pthread_serialport(void *arg)
{
    int len;
    char data;

    while (1)
    {
        len = 0;
        len = read(fd, &data, 1);

        if (len > 0)
        {
            printf("%c", data);
        }

        usleep(*(int *)arg);
    }

    return NULL;
}

void init_serialport(void)
{
    struct termios opt;

    fd = open(DEV_NAME, O_RDWR);
    if (fd < 0)
    {
        perror(DEV_NAME);

        return;
    }

    tcgetattr(fd, &opt);
#if 0     
    printf("%x\n", opt.c_iflag);
    printf("%x\n", opt.c_oflag);
    printf("%x\n", opt.c_cflag);
    printf("%x\n", opt.c_lflag);
#endif
    opt.c_iflag = 0x500;
    opt.c_oflag = 0x5;
    opt.c_cflag = 0x1cb2;
    opt.c_lflag = 0x8a33;

    opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    opt.c_iflag &= ~(INLCR | ICRNL | IGNCR | IXON);
    opt.c_oflag &= ~(ONLCR | OCRNL);

    tcsetattr(fd, TCSANOW, &opt);

    tcgetattr(fd, &opt);

    usleep(200000);

    tcflush(fd, TCIFLUSH);  //清空输入缓存
    tcflush(fd, TCOFLUSH);  //清空输出缓存
    tcflush(fd, TCIOFLUSH); //清空输入输出缓存
}

void *pthread_upload(void *arg)
{
    unsigned char temp = 0;
    char str_buf[1024];

    while (1)
    {
        cJSON *data = NULL;
        char *str_data = NULL;

        data = cJSON_CreateObject();

        if (NULL == data)
        {
            printf("cJSON error!\n");
            exit(-1);
        }

        sprintf(str_buf, "%d", temp);

        cJSON_AddStringToObject(data, DID, str_buf);

        cJSON *update = NULL;
        char *str_update = NULL;

        update = cJSON_CreateObject();

        if (NULL == update)
        {
            printf("cJSON error!\n");
            exit(-1);
        }

        cJSON_AddStringToObject(update, "M", "update");
        cJSON_AddStringToObject(update, "ID", ID);
        cJSON_AddItemToObject(update, "V", data);

        str_update = cJSON_PrintUnformatted(update);

        if (NULL == str_update)
        {
            printf("cJSON error!\n");
            exit(-1);
        }

        strcat(str_update, "\n");

        ret = send(s, str_update, strlen(str_update), 0);
        if (ret < 0)
        {
            printf("send error!\n");
            exit(-3);
        }

        printf("%s", str_update);

        temp++;

        usleep(*(int *)arg);
    }

    return NULL;
}
