#ifndef BLINK_IOCTL_H
#define BLINK_IOCTL_H

#include <linux/types.h>   /* para __u32, __u64 (en kernel) */
                           /* En user space, stdint.h define uint32_t/uint64_t */
#ifdef __KERNEL__
# include <linux/ioctl.h>
#else
# include <sys/ioctl.h>
# include <stdint.h>
// typedef uint32_t __u32;
// typedef uint64_t __u64;
#endif

#define BLINK_IOC_MAGIC  'b'

/* IDs de dispositivos que controlamos */
enum {
    BLINK_ID_KTHREAD = 0,
    BLINK_ID_TIMER   = 1,
    BLINK_ID_HRTIMER = 2,
    BLINK_ID__MAX
};

/* SET / GET del periodo por valor */
struct blink_ioc_ms {
    __u32 id;        /* uno de BLINK_ID_* */
    __u32 ms;        /* periodo en milisegundos (>=1) */
};

/* Demostración: pasar un puntero de user space al kernel */
struct blink_ioc_ptr {
    __u32 id;        /* a quién aplicarlo */
    __u32 pad;
    __u64 user_ptr;  /* dirección en user space con un uint32_t */
};

/* Demostración: eco ida/vuelta de un buffer pequeño */
struct blink_ioc_echo {
    __u64 user_ptr;  /* buffer en user space */
    __u32 len;       /* bytes válidos del buffer */
    __u32 pad;
};

#define BLINK_IOC_SET_MS          _IOW (BLINK_IOC_MAGIC, 0x01, struct blink_ioc_ms)
#define BLINK_IOC_GET_MS          _IOWR(BLINK_IOC_MAGIC, 0x02, struct blink_ioc_ms)
#define BLINK_IOC_SET_MS_FROM_PTR _IOW (BLINK_IOC_MAGIC, 0x03, struct blink_ioc_ptr)
#define BLINK_IOC_ECHO            _IOWR(BLINK_IOC_MAGIC, 0x04, struct blink_ioc_echo)

#endif /* BLINK_IOCTL_H */
