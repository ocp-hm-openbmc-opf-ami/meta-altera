// spidev_test — minimal SPI loopback/transfer utility for board bring-up.
//
// Compact, self-contained replacement for the kernel tools/spi/spidev_test.c.
// Intended for proving the SPI signal path (HPS SPIM -> FPGA fabric -> header
// pins) WITHOUT a sensor attached: jumper MOSI to MISO at the connector and
// run with -v; if RX echoes TX the whole path (pinmux, fabric, I/O) is alive.
//
// Examples:
//   spidev_test -D /dev/spidev1.0 -v -p "LOOPBACK"   # loopback proof
//   spidev_test -D /dev/spidev1.0 -s 1000000 -v       # default pattern @ 1MHz
//
// Default device is /dev/spidev1.0 (HPS SPIM0 / accel net on this board).

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

static const char *device = "/dev/spidev1.0";
static uint32_t mode;
static uint8_t bits = 8;
static uint32_t speed = 500000;
static uint16_t delay;
static int verbose;

static void pabort(const char *s)
{
    perror(s);
    exit(EXIT_FAILURE);
}

static void hex_dump(const char *label, const uint8_t *buf, size_t len)
{
    printf("%s:", label);
    for (size_t i = 0; i < len; i++)
        printf(" %02X", buf[i]);
    printf("\n");
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [options]\n"
            "  -D <dev>   device (default %s)\n"
            "  -s <hz>    max speed (default %u)\n"
            "  -d <us>    delay between words\n"
            "  -b <n>     bits per word (default 8)\n"
            "  -m <0-3>   SPI mode\n"
            "  -p <data>  bytes to send (string); default 6-byte pattern\n"
            "  -v         verbose: print TX/RX and PASS/FAIL on echo\n",
            prog, device, speed);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    int ret, fd, opt;
    const uint8_t default_tx[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x55, 0xAA };
    const uint8_t *tx = default_tx;
    size_t len = sizeof(default_tx);

    while ((opt = getopt(argc, argv, "D:s:d:b:m:p:vh")) != -1) {
        switch (opt) {
        case 'D':
            device = optarg;
            break;
        case 's':
            speed = (uint32_t)strtoul(optarg, NULL, 0);
            break;
        case 'd':
            delay = (uint16_t)strtoul(optarg, NULL, 0);
            break;
        case 'b':
            bits = (uint8_t)strtoul(optarg, NULL, 0);
            break;
        case 'm':
            mode = (uint32_t)strtoul(optarg, NULL, 0) & 0x3;
            break;
        case 'p':
            tx = (const uint8_t *)optarg;
            len = strlen(optarg);
            if (len == 0)
                usage(argv[0]);
            break;
        case 'v':
            verbose = 1;
            break;
        default:
            usage(argv[0]);
        }
    }

    fd = open(device, O_RDWR);
    if (fd < 0)
        pabort("can't open device");

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0)
        pabort("can't set spi mode");
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0)
        pabort("can't set bits per word");
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
        pabort("can't set max speed hz");

    printf("device: %s  mode: %u  bits: %u  speed: %u Hz\n", device, mode, bits, speed);

    uint8_t *rx = calloc(1, len);
    if (!rx)
        pabort("calloc");

    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = (uint32_t)len;
    tr.delay_usecs = delay;
    tr.speed_hz = speed;
    tr.bits_per_word = bits;

    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1)
        pabort("can't send spi message");

    if (verbose) {
        hex_dump("TX", tx, len);
        hex_dump("RX", rx, len);
        if (memcmp(tx, rx, len) == 0)
            printf("LOOPBACK PASS\n");
        else
            printf("LOOPBACK FAIL\n");
    } else {
        hex_dump("RX", rx, len);
    }

    free(rx);
    close(fd);
    return 0;
}
