#include <QThread>
#include <QComboBox>
#include <QMetaType>
#include <QDebug>

#include "tegra.h"
#include "usb.h"
#include "util.h"

#include "./ui_window.h"

#include "device_worker.hpp"

DeviceWorker::DeviceWorker(Ui::Window* pui): ui(pui) {}

void DeviceWorker::stop()
{
    this->running = false;
}

void DeviceWorker::find_devices()
{
    libusb_device** devices;

    while (this->running)
    {

        devices = tegra_get_rcm_devices(NULL);

        QList<libusb_device*> device_list;
        if (devices)
        {
            int device_index = 0;
            libusb_device* device;
            while ((device = devices[device_index++]) != NULL)
            {
                device_list.append(device);
            }
        }
        bool found_devices = !device_list.empty();

        QComboBox* combo = this->ui->deviceSelect;
        libusb_device* combo_device;
        int combo_count = combo->count();

        // Check what devices we already know of
        for (int i = 0; i < combo_count; ++i)
        {
            combo_device = *(libusb_device**)combo->itemData(i).data();

            auto list_index = device_list.indexOf(combo_device);
            if (0 <= list_index)
            {
                device_list.removeAt(list_index);
            }
            else
            {
                combo->removeItem(i);
            }
        }

        // If we find new devices, identify and add them
        for (libusb_device*& device: device_list)
        {
            char vendor[0xFF] = "Unknown";
            char product[0xFF] = "Unknown";

            usb_get_vendor_name(device, vendor, ARRAY_LEN(vendor));
            usb_get_product_name(device, product, ARRAY_LEN(product));

            combo->addItem(QString("%1 %2").arg(vendor).arg(product), QVariant::fromValue((void*)libusb_ref_device(device)));

        }

        if (!combo_count && !device_list.empty())
        {
            combo->setCurrentIndex(0);
        }
        else if (combo_count && !found_devices)
        {
            combo->setCurrentIndex(-1);
        }

        libusb_free_device_list(devices, 1); // combo box now retains a reference

        QThread::msleep(500);
    }

    emit finished();
}
