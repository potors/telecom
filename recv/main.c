#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <ctype.h>

int fd = 0;

#define next(T) (*(T*) fetch(sizeof(T)))
void* fetch(int len) {
    static void* buffer = NULL;

    if (buffer) free(buffer);
    buffer = malloc(len);

    while (len > 0) {
        int n = read(fd, buffer, len);
        if (n < 0) {
            fprintf(stderr, "read error: %s\n", strerror(errno));
            exit(1);
        }

        len -= n;
    }

    return buffer;
}

#define EVENT      0xE0
#define EVENT_DATA 0x0D

void event_data(int8_t argc, uint16_t* argv);
void event(uint8_t code, uint8_t argc) {
    printf("<< event 0x%X (%d arg%s)\n",
           code, argc, argc > 1 ? "s" : "");

    uint16_t* argv = malloc(argc * sizeof(*argv));
    for (int i = 0; i < argc; i++) {
        argv[i] = next(typeof(*argv));
    }

    switch (code) {
        case EVENT_DATA: event_data(argc, argv); break;
        default: printf("!! invalid event\n"); break;
    }

    free(argv);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", *argv);
        exit(1);
    }

    const char* port = argv[1];

    fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", port, strerror(errno));
        exit(1);
    }

    struct termios tty;
    tcgetattr(fd, &tty);

    cfmakeraw(&tty);
    cfsetspeed(&tty, B115200);

    tty.c_cc[VMIN]  = 1; // block until 1 byte
    tty.c_cc[VTIME] = 0; // no timeout

    tcsetattr(fd, TCSANOW, &tty);

    printf("listening %s @ 115200 8N1\n", port);

    while (true) {
        uint8_t c = next(uint8_t);
        // printf("%02X ", c);
        // continue;

        if ((c & 0xF0) == EVENT) {
            uint8_t code = c & 0xF;
            uint8_t args = (next(uint8_t) - 0x80) % 0x80;

            event(code, args);
            continue;
        }

        printf("## ");

        while (c != '\n') {
            if (isascii(c)) {
                putc(c, stdout);
            }

            c = next(uint8_t);
        }

        putc('\n', stdout);

        fflush(stdout);
    }

    close(fd);
}

void event_data(int8_t argc, uint16_t* argv) {
    uint16_t samples = argv[0];
    printf("\tsamples: %d\n", samples);

    uint16_t bytes = argv[1];
    printf("\tbytes: %d\n", bytes);

    char* buffer = fetch(samples * bytes);
    printf("\t got %d bytes\n", samples * bytes);
}
