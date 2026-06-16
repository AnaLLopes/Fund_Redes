#include "common.h"
#include <stdarg.h>
#include <sys/ioctl.h>
#include <net/if.h>

/* Log com timestamp */
void print_log(const char *format, ...) {
    va_list args;
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    char timestamp[20];
    
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timeinfo);
    printf("[%s] ", timestamp);
    
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    fflush(stdout);
}

/* Obter IP local */
char *get_local_ip() {
    static char ip[16];
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        strcpy(ip, "127.0.0.1");
        return ip;
    }
    
    /* Conectar a um IP externo (não precisa ser alcançável) */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("8.8.8.8");
    addr.sin_port = htons(80);
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        strcpy(ip, "127.0.0.1");
        close(sock);
        return ip;
    }
    
    struct sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);
    if (getsockname(sock, (struct sockaddr *)&local_addr, &len) == 0) {
        inet_ntop(AF_INET, &local_addr.sin_addr, ip, sizeof(ip));
    } else {
        strcpy(ip, "127.0.0.1");
    }
    
    close(sock);
    return ip;
}

/* Obter timestamp em milissegundos */
long get_timestamp_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
