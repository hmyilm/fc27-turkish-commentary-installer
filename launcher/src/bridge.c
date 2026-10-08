/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2025 John Tornblom (original SDK syscall/startup sample)
 * Modified: fixed-arity syscalls and local ELF-loader bridge.
 * This installed application does not run the installer with app privileges.
 * It forwards the embedded ELF to the local privileged payload loader.
 */
#include "installer_payload.h"

enum {
    SYS_EXIT = 1,
    SYS_WRITE = 4,
    SYS_CLOSE = 6,
    SYS_SOCKET = 97,
    SYS_CONNECT = 98,
    AF_INET_LOCAL = 2,
    SOCK_STREAM_LOCAL = 1,
    EINTR_LOCAL = 4,
    STREAM_CHUNK = 65536
};

struct local_sockaddr_in {
    unsigned char length;
    unsigned char family;
    unsigned short port;
    unsigned int address;
    unsigned char padding[8];
};
_Static_assert(sizeof(struct local_sockaddr_in) == 16, "FreeBSD sockaddr ABI");

static long raw_syscall(long number, long a1, long a2, long a3,
                        long a4, long a5, long a6) {
    unsigned long result;
    unsigned char is_error;
    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    register long r9 __asm__("r9") = a6;
    __asm__ __volatile__("syscall"
                         : "=a"(result), "=@ccc"(is_error),
                           "+r"(r10), "+r"(r8), "+r"(r9)
                         : "a"(number), "D"(a1), "S"(a2), "d"(a3)
                         : "rcx", "r11", "memory");
    return is_error ? -(long)result : (long)result;
}

static void raw_message(const char *message, unsigned long length) {
    unsigned long done = 0;
    while (done < length) {
        long n = raw_syscall(SYS_WRITE, 1, (long)(message + done),
                             (long)(length - done), 0, 0, 0);
        if (n == -EINTR_LOCAL)
            continue;
        if (n <= 0)
            return;
        done += (unsigned long)n;
    }
}
#define MESSAGE(text) raw_message((text), sizeof(text) - 1)

static int bridge_main(void) {
    MESSAGE("FC27 TR: yerel ELF yukleyiciye baglaniliyor (127.0.0.1:9021).\n");
    long fd = raw_syscall(SYS_SOCKET, AF_INET_LOCAL, SOCK_STREAM_LOCAL, 0, 0, 0, 0);
    if (fd < 0) {
        MESSAGE("FC27 TR HATA: socket acilamadi. ELF surumunu Payload Manager ile kullanin.\n");
        return 1;
    }
    const struct local_sockaddr_in address = {
        16, AF_INET_LOCAL, __builtin_bswap16(9021), 0x0100007fu, {0}
    };
    long connected;
    do {
        connected = raw_syscall(SYS_CONNECT, fd, (long)&address,
                                sizeof(address), 0, 0, 0);
    } while (connected == -EINTR_LOCAL);
    if (connected < 0) {
        MESSAGE("FC27 TR HATA: yerel ELF loader 9021 acik degil. Loaderi acip yeniden deneyin.\n");
        raw_syscall(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
        return 2;
    }
    unsigned long done = 0;
    while (done < installer_payload_size) {
        unsigned long wanted = installer_payload_size - done;
        if (wanted > STREAM_CHUNK)
            wanted = STREAM_CHUNK;
        long n = raw_syscall(SYS_WRITE, fd, (long)(installer_payload + done),
                             (long)wanted, 0, 0, 0);
        if (n == -EINTR_LOCAL)
            continue;
        if (n <= 0 || (unsigned long)n > wanted) {
            MESSAGE("FC27 TR HATA: ELF aktarimi tamamlanamadi.\n");
            raw_syscall(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
            return 3;
        }
        done += (unsigned long)n;
    }
    if (raw_syscall(SYS_CLOSE, fd, 0, 0, 0, 0, 0) < 0) {
        MESSAGE("FC27 TR HATA: ELF baglantisi kapatilamadi.\n");
        return 4;
    }
    MESSAGE("FC27 TR: kurucu ELF gonderildi. Kurulum sonucunu PS5 bildirimlerinden takip edin.\n");
    return 0;
}

__attribute__((noreturn, force_align_arg_pointer))
void _start(void) {
    int status = bridge_main();
    raw_syscall(SYS_EXIT, status, 0, 0, 0, 0, 0);
    __builtin_trap();
}
