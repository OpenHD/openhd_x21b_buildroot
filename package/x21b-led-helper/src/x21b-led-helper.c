#include "ohdled.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <gpiod.h>
#include <glob.h>

#define CHIP "gpiochip6"
#define DEFAULT_UART "/dev/ttyS3"
#define FW_GLOB "/lib/firmware/x21b-led-fw-v*.bin"

static int flash_stm(void)
{
    struct gpiod_chip *chip;
    struct gpiod_line *reset;
    struct gpiod_line *boot;

    fprintf(stderr, "Going to flash x21b LED MCU\n");

    chip = gpiod_chip_open_by_name(CHIP);
    if (!chip) {
        fprintf(stderr, "gpiod_chip_open_by_name(%s): %s\n", CHIP, strerror(errno));
        return 1;
    }

    reset = gpiod_chip_get_line(chip, 5);   // GPIO197 - 192
    boot  = gpiod_chip_get_line(chip, 10);  // GPIO202 - 192

    if (!reset || !boot) {
        fprintf(stderr, "gpiod_chip_get_line failed\n");
        gpiod_chip_close(chip);
        return 1;
    }

    if (gpiod_line_request_output(reset, "stm32-reset", 0) < 0 ||
        gpiod_line_request_output(boot,  "stm32-boot",  1) < 0) {
        fprintf(stderr, "gpiod_line_request_output failed\n");
        gpiod_chip_close(chip);
        return 1;
    }

    gpiod_line_set_value(reset, 1);
    sleep(1);

    gpiod_line_set_value(boot, 0);
    sleep(1);

    gpiod_line_set_value(boot, 1);
    sleep(1);

    int ret = system(
        "stm32flash -b115200 "
        "-w /lib/firmware/x21b-led-fw-v*.bin "
        "/dev/ttyS3"
    );

    gpiod_line_set_value(reset, 0);
    sleep(1);

    gpiod_line_set_value(boot, 0);
    sleep(1);

    gpiod_line_set_value(boot, 1);
    sleep(1);

    gpiod_chip_close(chip);

    if (WIFEXITED(ret))
        return WEXITSTATUS(ret);

    return 1;
}

static int find_available_fw_version(void)
{
    glob_t g;
    int best = -1;

    if (glob(FW_GLOB, 0, NULL, &g) != 0)
        return -1;

    for (size_t i = 0; i < g.gl_pathc; i++) {
        int version;

        printf("%s\n", g.gl_pathv[i]);

        if (sscanf(g.gl_pathv[i],
                   "/lib/firmware/x21b-led-fw-v%d.bin",
                   &version) == 1) {
            printf("Available fw version = %d\n", version);

            if (version > best)
                best = version;
        }
    }

    globfree(&g);
    return best;
}

int main(int argc, char **argv)
{
    const char *dev = argc > 1 ? argv[1] : DEFAULT_UART;
    int ret = 1;

    int fd = led_open(dev);
    if (fd < 0) {
        fprintf(stderr, "failed to open %s\n", dev);
        return 1;
    }

    uint8_t mcu_version = 0;

    if (led_get_version(fd, &mcu_version) < 0) {
        fprintf(stderr, "failed to read MCU version, flashing\n");
        flash_stm();
        goto out_pet;
    }

    printf("MCU fw version: %u\n", mcu_version);

    int available_version = find_available_fw_version();

    if (available_version < 0) {
        fprintf(stderr, "no firmware image found matching %s\n", FW_GLOB);
        goto out_pet;
    }

    if (mcu_version < available_version) {
        fprintf(stderr,
                "MCU fw is old: current=%u available=%d\n",
                mcu_version,
                available_version);
        flash_stm();
    }

    ret = 0;

out_pet:
    if (led_pet_userspace(fd) < 0)
        fprintf(stderr, "failed to send PET_USERSPACE: %s\n", strerror(errno));
    led_close(fd);
    return ret;
}
