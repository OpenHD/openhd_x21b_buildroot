#include "ohdled.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s /dev/ttyS0 version\n"
        "  %s /dev/ttyS0 on\n"
        "  %s /dev/ttyS0 off\n"
        "  %s /dev/ttyS0 color R G B\n"
        "  %s /dev/ttyS0 blink R G B ms [R G B ms ...]\n"
        "  %s /dev/ttyS0 breathe R G B ms [R G B ms ...]\n"
        "  %s /dev/ttyS0 pet-uboot\n"
        "  %s /dev/ttyS0 pet-userspace\n"
        "  %s /dev/ttyS0 pet-periodic\n",
        argv0, argv0, argv0, argv0, argv0,
        argv0, argv0, argv0, argv0
    );
}

static int parse_u8(const char *s, uint8_t *out)
{
    long v = strtol(s, NULL, 0);
    if (v < 0 || v > 255)
        return -1;
    *out = (uint8_t)v;
    return 0;
}

static int parse_u16(const char *s, uint16_t *out)
{
    long v = strtol(s, NULL, 0);
    if (v < 0 || v > 65535)
        return -1;
    *out = (uint16_t)v;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    const char *dev = argv[1];
    const char *cmd = argv[2];

    int fd = led_open(dev);
    if (fd < 0) {
        perror("led_open");
        return 1;
    }

    int rc = -1;

    if (!strcmp(cmd, "version")) {
        uint8_t version = 0;
        rc = led_get_version(fd, &version);
        if (rc == 0)
            printf("version: %u\n", version);
    } else if (!strcmp(cmd, "on")) {
        rc = led_on(fd);
    } else if (!strcmp(cmd, "off")) {
        rc = led_off(fd);
    } else if (!strcmp(cmd, "color")) {
        if (argc != 6) {
            usage(argv[0]);
            goto out;
        }

        struct led_color color;
        if (parse_u8(argv[3], &color.r) ||
            parse_u8(argv[4], &color.g) ||
            parse_u8(argv[5], &color.b)) {
            fprintf(stderr, "invalid RGB value\n");
            goto out;
        }

        rc = led_static_color(fd, color, 1, 0, 0);
    } else if (!strcmp(cmd, "blink") || !strcmp(cmd, "breathe")) {
        int values = argc - 3;

        if (values <= 0 || values % 4 != 0) {
            usage(argv[0]);
            goto out;
        }

        size_t count = values / 4;
        struct led_anim_frame frames[24];

        if (count > 24) {
            fprintf(stderr, "too many animation frames\n");
            goto out;
        }

        for (size_t i = 0; i < count; i++) {
            char **p = &argv[3 + i * 4];

            if (parse_u8(p[0], &frames[i].color.r) ||
                parse_u8(p[1], &frames[i].color.g) ||
                parse_u8(p[2], &frames[i].color.b) ||
                parse_u16(p[3], &frames[i].delay_ms)) {
                fprintf(stderr, "invalid animation frame\n");
                goto out;
            }
        }

        if (!strcmp(cmd, "blink"))
            rc = led_blink(fd, frames, count, 1, 0, 0);
        else
            rc = led_breathe(fd, frames, count, 1, 0, 0);
    } else if (!strcmp(cmd, "pet-uboot")) {
        rc = led_pet_uboot(fd);
    } else if (!strcmp(cmd, "pet-userspace")) {
        rc = led_pet_userspace(fd);
    } else if (!strcmp(cmd, "pet-periodic")) {
        rc = led_pet_periodic(fd);
    } else {
        usage(argv[0]);
        goto out;
    }

out:
    led_close(fd);

    if (rc < 0) {
        fprintf(stderr, "command failed\n");
        return 1;
    }

    return 0;
}
