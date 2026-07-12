#pragma once
#include <stdint.h>
#include <stddef.h>

enum led_cmd {
    LED_GET_VERSION     = 0,
    LED_OFF             = 1,
    LED_ON              = 2,
    LED_STATIC_COLOR    = 3,
    LED_BREATHE         = 4,
    LED_BLINK           = 5,
    PET_U_BOOT          = 6,
    PET_USERSPACE       = 7,
    PET_PERIODIC        = 8,
    LED_REBOOT          = 9,
};

struct led_color {
    uint8_t r, g, b;
};

struct led_anim_frame {
    struct led_color color;
    uint16_t delay_ms;
};

int led_open(const char *dev);
void led_close(int fd);

int led_get_version(int fd, uint8_t *version);
int led_on(int fd);
int led_off(int fd);

int led_static_color(
    int fd,
    struct led_color color,
    int soft_transition,
    int keep_across_reboot,
    int fatal_fault
);

int led_breathe(
    int fd,
    const struct led_anim_frame *frames,
    size_t count,
    int soft_transition,
    int keep_across_reboot,
    int fatal_fault
);

int led_blink(
    int fd,
    const struct led_anim_frame *frames,
    size_t count,
    int soft_transition,
    int keep_across_reboot,
    int fatal_fault
);

int led_pet_uboot(int fd);
int led_pet_userspace(int fd);
int led_pet_periodic(int fd);
