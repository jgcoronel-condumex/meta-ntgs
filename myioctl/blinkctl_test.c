// gcc -O2 -Wall -o blinkctl_test blinkctl_test.c
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "blink_ioctl.h"

static int set_ms(int fd, uint32_t id, uint32_t ms)
{
    struct blink_ioc_ms a = { .id = id, .ms = ms };
    return ioctl(fd, BLINK_IOC_SET_MS, &a);
}
static int get_ms(int fd, uint32_t id, uint32_t *ms)
{
    struct blink_ioc_ms a = { .id = id, .ms = 0 };
    int ret = ioctl(fd, BLINK_IOC_GET_MS, &a);
    if (!ret) *ms = a.ms;
    return ret;
}
static int set_ms_from_ptr(int fd, uint32_t id, uint32_t *pms)
{
    struct blink_ioc_ptr p = { .id = id, .user_ptr = (uintptr_t)pms };
    return ioctl(fd, BLINK_IOC_SET_MS_FROM_PTR, &p);
}
static int echo_buf(int fd, char *buf, uint32_t len)
{
    struct blink_ioc_echo e = { .user_ptr = (uintptr_t)buf, .len = len };
    return ioctl(fd, BLINK_IOC_ECHO, &e);
}

int main(void)
{
    int fd = open("/dev/blinkctl", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    /* 1) Set por valor */
    printf("SET KTHREAD 200 ms\n");
    if (set_ms(fd, BLINK_ID_KTHREAD, 200)) perror("SET_MS kthread");

    /* 2) Get */
    uint32_t ms = 0;
    if (!get_ms(fd, BLINK_ID_KTHREAD, &ms))
        printf("GET KTHREAD -> %u ms\n", ms);
    else
        perror("GET_MS kthread");

    /* 3) Set desde puntero (DEMO user pointer) */
    uint32_t nuevo = 75;
    printf("SET_FROM_PTR TIMER %u ms (ptr=%p)\n", nuevo, (void *)&nuevo);
    if (set_ms_from_ptr(fd, BLINK_ID_TIMER, &nuevo)) perror("SET_MS_FROM_PTR");

    if (!get_ms(fd, BLINK_ID_TIMER, &ms))
        printf("GET TIMER -> %u ms\n", ms);

    /* Prueba de error: puntero inválido */
    printf("SET_FROM_PTR con puntero invalido...\n");
    struct blink_ioc_ptr bad = { .id = BLINK_ID_TIMER, .user_ptr = 0x123ULL };
    if (ioctl(fd, BLINK_IOC_SET_MS_FROM_PTR, &bad) < 0)
        perror("SET_MS_FROM_PTR (esperado EFAULT)");

    /* 4) ECHO ida/vuelta */
    char txt[32] = "hola ioctl";
    if (!echo_buf(fd, txt, strlen(txt)+1))
        printf("ECHO -> \"%s\"\n", txt);
    else
        perror("ECHO");

    /* HRTIMER: */
    printf("SET HRTIMER 30 ms\n");
    if (set_ms(fd, BLINK_ID_HRTIMER, 30)) perror("SET_MS hrtimer");
    if (!get_ms(fd, BLINK_ID_HRTIMER, &ms))
        printf("GET HRTIMER -> %u ms\n", ms);

    close(fd);
    return 0;
}
