#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <mtd/mtd-user.h>
#include <sys/ioctl.h>

#define SLOT_STATUS_MAGIC 0x53544C54u

#define SLOT_UNKNOWN_STATE 0xFFu
#define SLOT_GOOD_STATE 0xFEu

struct slot_status_record {
    uint32_t magic;
    uint32_t seq;
    uint8_t slot;
    uint8_t fail_count;
    uint8_t good;
    uint8_t reserved;
    uint32_t crc32;
};

static uint32_t crc32_calc(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;

    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
        }
    }

    return crc ^ 0xFFFFFFFFu;
}

static bool slot_record_valid(const struct slot_status_record *rec)
{
    uint32_t crc;

    if (rec->magic != SLOT_STATUS_MAGIC)
        return false;

    crc = crc32_calc(rec, offsetof(struct slot_status_record, fail_count));
    return crc == rec->crc32;
}

static bool record_is_erased(const struct slot_status_record *rec)
{
    const uint8_t *p = (const uint8_t *)rec;
    for (size_t i = 0; i < sizeof(*rec); i++) {
        if (p[i] != 0xFF)
            return false;
    }
    return true;
}

static int read_all(int fd, void *buf, size_t len)
{
    uint8_t *p = buf;
    while (len) {
        ssize_t r = read(fd, p, len);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (r == 0)
            return -1;
        p += r;
        len -= (size_t)r;
    }
    return 0;
}

static int write_all(int fd, const void *buf, size_t len)
{
    const uint8_t *p = buf;
    while (len) {
        ssize_t w = write(fd, p, len);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p += w;
        len -= (size_t)w;
    }
    return 0;
}

static int get_device_size(const char *path, size_t *size_out, size_t *erase_out)
{
    int fd;
    struct mtd_info_user info;

    fd = open(path, O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (ioctl(fd, MEMGETINFO, &info) == 0) {
        *size_out = info.size;
        *erase_out = info.erasesize;
        close(fd);
        return 0;
    }

    perror("MEMGETINFO");
    close(fd);
    return -1;
}

static int load_partition(const char *path, uint8_t **buf_out, size_t *size_out, size_t *erase_size)
{
    uint8_t *buf;
    int fd;

    if (get_device_size(path, size_out, erase_size)) {
        fprintf(stderr, "invalid size for %s\n", path);
        return -1;
    }

    buf = malloc(*size_out);
    if (!buf) {
        perror("malloc");
        return -1;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        free(buf);
        return -1;
    }

    if (read_all(fd, buf, *size_out) < 0) {
        perror("read");
        close(fd);
        free(buf);
        return -1;
    }

    close(fd);
    *buf_out = buf;
    return 0;
}

static int find_latest_record(const uint8_t *buf, size_t size,
                              struct slot_status_record *best_out,
                              size_t *best_idx_out,
                              size_t *first_free_idx_out)
{
    size_t count = size / sizeof(struct slot_status_record);
    const struct slot_status_record *best = NULL;
    size_t best_idx = 0;
    size_t first_free = count;

    for (size_t i = 0; i < count; i++) {
        const struct slot_status_record *rec =
            (const struct slot_status_record *)(buf + i * sizeof(*rec));

        if (record_is_erased(rec) && first_free == count)
            first_free = i;

        if (!slot_record_valid(rec))
            continue;

        if (!best || rec->seq > best->seq) {
            best = rec;
            best_idx = i;
        }
    }

    if (best) {
        *best_out = *best;
        *best_idx_out = best_idx;
    }
    *first_free_idx_out = first_free;

    return best ? 0 : -1;
}

static void fill_record(struct slot_status_record *rec,
                        uint32_t seq, char slot, uint8_t fail_count, uint8_t good)
{
    memset(rec, 0xFF, sizeof(*rec));
    rec->magic = SLOT_STATUS_MAGIC;
    rec->seq = seq;
    rec->slot = (uint8_t)slot;
    rec->fail_count = fail_count;
    rec->good = good;
    rec->reserved = 0;
    rec->crc32 = crc32_calc(rec, offsetof(struct slot_status_record, fail_count));
}

static int write_record_at(const char *path, off_t off,
                           const struct slot_status_record *rec)
{
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (lseek(fd, off, SEEK_SET) < 0) {
        perror("lseek");
        close(fd);
        return -1;
    }

    if (write_all(fd, rec, sizeof(*rec)) < 0) {
        perror("write");
        close(fd);
        return -1;
    }

    fsync(fd);
    close(fd);
    return 0;
}

static int erase_to_ff_and_rewrite_first(const char *path, size_t size,
                                         const struct slot_status_record *rec)
{
    int fd;
    size_t dev_size = 0, erase_size = 0;
    struct erase_info_user ei;

    fd = open(path, O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (get_device_size(path, &dev_size, &erase_size) < 0) {
        fprintf(stderr, "failed to get MTD info for %s\n", path);
        close(fd);
        return -1;
    }

    if (dev_size != size) {
        fprintf(stderr, "device size mismatch: got %zu expected %zu\n", dev_size, size);
        close(fd);
        return -1;
    }

    for (size_t off = 0; off < size; off += erase_size) {
        ei.start = off;
        ei.length = erase_size;
        if (ioctl(fd, MEMERASE, &ei) < 0) {
            perror("MEMERASE");
            close(fd);
            return -1;
        }
    }

    if (lseek(fd, 0, SEEK_SET) < 0) {
        perror("lseek");
        close(fd);
        return -1;
    }

    if (write_all(fd, rec, sizeof(*rec)) < 0) {
        perror("write first");
        close(fd);
        return -1;
    }

    if (fsync(fd) < 0) {
        perror("fsync");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int write_bytes_at(const char *path, off_t off, const void *buf, size_t len)
{
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (lseek(fd, off, SEEK_SET) < 0) {
        perror("lseek");
        close(fd);
        return -1;
    }

    if (write_all(fd, buf, len) < 0) {
        perror("write");
        close(fd);
        return -1;
    }

    if (fsync(fd) < 0) {
        perror("fsync");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int cmd_set_last_good_if_unknown(const char *path)
{
    uint8_t *buf = NULL;
    size_t size = 0;
    size_t erase_size = 0;
    struct slot_status_record best;
    size_t best_idx = 0, first_free = 0;
    uint8_t new_good = SLOT_GOOD_STATE;
    off_t good_off;

    if (load_partition(path, &buf, &size, &erase_size) < 0)
        return 1;

    if (find_latest_record(buf, size, &best, &best_idx, &first_free) < 0) {
        fprintf(stderr, "No valid record found\n");
        free(buf);
        return 1;
    }

    if (best.good != SLOT_UNKNOWN_STATE) {
        printf("Latest record already resolved: seq=%u slot=%c good=0x%02x\n",
               best.seq, best.slot, best.good);
        free(buf);
        return 0;
    }

    if ((new_good & (uint8_t)~best.good) != 0) {
        fprintf(stderr, "Illegal flash transition for good: 0x%02x -> 0x%02x\n",
                best.good, new_good);
        free(buf);
        return 1;
    }

    good_off = (off_t)(best_idx * sizeof(struct slot_status_record) +
                       offsetof(struct slot_status_record, good));

    if (write_bytes_at(path, good_off, &new_good, sizeof(new_good)) < 0) {
        free(buf);
        return 1;
    }

    printf("Updated latest record in place: index=%zu seq=%u slot=%c good 0x%02x -> 0x%02x\n",
           best_idx, best.seq, best.slot, best.good, new_good);

    free(buf);
    return 0;
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s show <path>\n"
        "  %s get_slot <path>\n"
        "  %s set <path> <a|b> <good> <fail_count>\n"
        "  %s set_last_good_if_unknown <path>\n",
        prog, prog, prog, prog);
}

static int cmd_show(const char *path)
{
    uint8_t *buf = NULL;
    size_t size = 0;
    size_t erase_size = 0;
    struct slot_status_record best;
    size_t best_idx = 0, first_free = 0;

    if (load_partition(path, &buf, &size, &erase_size) < 0)
        return 1;

    if (find_latest_record(buf, size, &best, &best_idx, &first_free) < 0) {
        printf("No valid record found\n");
        free(buf);
        return 1;
    }

    printf("Latest valid record:\n");
    printf("  index      : %zu\n", best_idx);
    printf("  seq        : %u\n", best.seq);
    printf("  slot       : %c\n", best.slot);
    printf("  fail_count : %u\n", best.fail_count);
    printf("  good       : %u\n", best.good);

    if (first_free < size / sizeof(struct slot_status_record))
        printf("  next free  : %zu\n", first_free);
    else
        printf("  next free  : none (partition full)\n");

    free(buf);
    return 0;
}

static int cmd_get_slot(const char *path)
{
    uint8_t *buf = NULL;
    size_t size = 0;
    size_t erase_size = 0;
    struct slot_status_record best;
    size_t best_idx = 0, first_free = 0;

    if (load_partition(path, &buf, &size, &erase_size) < 0)
        return 1;

    if (find_latest_record(buf, size, &best, &best_idx, &first_free) < 0) {
        printf("No valid record found\n");
        free(buf);
        return 1;
    }

    printf("%c\n", best.slot);

    free(buf);
    return 0;
}

static int cmd_set(const char *path, char slot, uint8_t good, uint8_t fail_count)
{
    uint8_t *buf = NULL;
    size_t size = 0;
    size_t erase_size = 0;
    struct slot_status_record best;
    struct slot_status_record next;
    size_t best_idx = 0, first_free = 0;
    uint32_t next_seq = 1;

    if (slot != 'a' && slot != 'b') {
        fprintf(stderr, "slot must be 'a' or 'b'\n");
        return 1;
    }

    if (load_partition(path, &buf, &size, &erase_size) < 0)
        return 1;

    if (find_latest_record(buf, size, &best, &best_idx, &first_free) == 0)
        next_seq = best.seq + 1;

    if (first_free < size / sizeof(struct slot_status_record)) {
        fill_record(&next, next_seq, slot, fail_count, good);
        printf("Appending record at index %zu: seq=%u slot=%c good=%u fail=%u\n",
               first_free, next.seq, slot, good, fail_count);

        if (write_record_at(path,
                            (off_t)(first_free * sizeof(struct slot_status_record)),
                            &next) < 0) {
            free(buf);
            return 1;
        }
    } else {
        fill_record(&next, 1, slot, fail_count, good);
        printf("Partition full, recycling log\n");
        printf("Erasing to 0xFF and writing first record: seq=%u slot=%c good=%u fail=%u\n",
               next.seq, slot, good, fail_count);

        if (erase_to_ff_and_rewrite_first(path, size, &next) < 0) {
            free(buf);
            return 1;
        }
    }

    free(buf);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "show") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            return 1;
        }
        return cmd_show(argv[2]);
    }

    if (strcmp(argv[1], "get_slot") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            return 1;
        }
        return cmd_get_slot(argv[2]);
    }

    if (strcmp(argv[1], "set_last_good_if_unknown") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            return 1;
        }
        return cmd_set_last_good_if_unknown(argv[2]);
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc != 6) {
            usage(argv[0]);
            return 1;
        }
        return cmd_set(argv[2], argv[3][0],
                       (uint8_t)atoi(argv[4]),
                       (uint8_t)atoi(argv[5]));
    }

    usage(argv[0]);
    return 1;
}
