#include "ohdled.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>

#define LED_RESP_TIMEOUT_MS 2000
#define MAX_PACKET 128

static uint8_t crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;

    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ 0x07);
            else
                crc <<= 1;
        }
    }

    return crc;
}

static int write_all(int fd, const uint8_t *buf, size_t len)
{
    while (len) {
        ssize_t n = write(fd, buf, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        buf += n;
        len -= (size_t)n;
    }

    return 0;
}

static int send_packet(int fd, uint8_t cmd, const uint8_t *payload, size_t payload_len)
{
    uint8_t packet[MAX_PACKET];
    size_t len = 0;

    if (payload_len + 2 > sizeof(packet))
        return -1;

    packet[len++] = cmd;

    if (payload_len) {
        memcpy(&packet[len], payload, payload_len);
        len += payload_len;
    }

    packet[len] = crc8(packet, len);
    len++;

    return write_all(fd, packet, len);
}

int led_open(const char *dev)
{
    int fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
        return -1;

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return -1;
    }

    cfmakeraw(&tty);
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);


    tty.c_cflag |=  CLOCAL | CREAD | CS8;
    tty.c_cflag &= ~(CRTSCTS | CSTOPB | PARENB | CSIZE);
    tty.c_cflag |=  CS8;

    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return -1;
    }

    return fd;
}

void led_close(int fd)
{
    if (fd >= 0)
        close(fd);
}

int led_get_version(int fd, uint8_t *version)
{
    uint8_t req[2];
    uint8_t resp[3];
    size_t got = 0;

    req[0] = LED_GET_VERSION;
    req[1] = crc8(req, 1);

    tcflush(fd, TCIFLUSH);

    if (write_all(fd, req, sizeof(req)) < 0)
        return -1;

    tcdrain(fd);

    while (got < sizeof(resp)) {
        struct pollfd pfd = {
            .fd = fd,
            .events = POLLIN,
        };

        int pr = poll(&pfd, 1, LED_RESP_TIMEOUT_MS);
        if (pr <= 0)
            return -1;

        ssize_t n = read(fd, resp + got, sizeof(resp) - got);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            return -1;
        }

        if (n == 0)
            return -1;

        got += (size_t)n;
    }

    if (resp[0] != LED_GET_VERSION)
        return -1;

    if (crc8(resp, 2) != resp[2])
        return -1;

    if (version)
        *version = resp[1];

    return 0;
}

int led_on(int fd)
{
    return send_packet(fd, LED_ON, NULL, 0);
}

int led_off(int fd)
{
    return send_packet(fd, LED_OFF, NULL, 0);
}

int led_static_color(
    int fd,
    struct led_color color,
    int soft_transition,
    int keep_across_reboot,
    int fatal_fault
) {
    uint8_t payload[6];

    payload[0] = color.r;
    payload[1] = color.g;
    payload[2] = color.b;
    payload[3] = soft_transition ? 1 : 0;
    payload[4] = keep_across_reboot ? 1 : 0;
    payload[5] = fatal_fault ? 1 : 0;

    return send_packet(fd, LED_STATIC_COLOR, payload, sizeof(payload));
}

static int led_animation(
    int fd,
    uint8_t cmd,
    const struct led_anim_frame *frames,
    size_t count,
    int soft_transition,
    int keep_across_reboot,
    int fatal_fault
) {
    uint8_t payload[MAX_PACKET];
    size_t pos = 0;

    if (!frames || count == 0 || count > 24)
        return -1;

    payload[pos++] = (uint8_t)count;
    payload[pos++] = soft_transition ? 1 : 0;
    payload[pos++] = keep_across_reboot ? 1 : 0;
    payload[pos++] = fatal_fault ? 1 : 0;

    for (size_t i = 0; i < count; i++) {
        payload[pos++] = frames[i].color.r;
        payload[pos++] = frames[i].color.g;
        payload[pos++] = frames[i].color.b;

        payload[pos++] = frames[i].delay_ms & 0xff;
        payload[pos++] = frames[i].delay_ms >> 8;
    }

    return send_packet(fd, cmd, payload, pos);
}

int led_breathe(
    int fd,
    const struct led_anim_frame *frames,
    size_t count,
    int soft_transition,
    int keep_across_reboot,
    int fatal_fault
) {
    return led_animation(
        fd,
        LED_BREATHE,
        frames,
        count,
        soft_transition,
        keep_across_reboot,
        fatal_fault
    );
}

int led_blink(
    int fd,
    const struct led_anim_frame *frames,
    size_t count,
    int soft_transition,
    int keep_across_reboot,
    int fatal_fault
) {
    return led_animation(
        fd,
        LED_BLINK,
        frames,
        count,
        soft_transition,
        keep_across_reboot,
        fatal_fault
    );
}

int led_pet_uboot(int fd)
{
    return send_packet(fd, PET_U_BOOT, NULL, 0);
}

int led_pet_userspace(int fd)
{
    return send_packet(fd, PET_USERSPACE, NULL, 0);
}

int led_pet_periodic(int fd)
{
    return send_packet(fd, PET_PERIODIC, NULL, 0);
}
