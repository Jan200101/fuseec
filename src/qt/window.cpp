#include <QMainWindow> 
#include <QFileDialog> 
#include <QMessageBox>
#include <QThread>
#include <libusb.h>

#include "fs.h"
#include "payload.h"
#include "tegra.h"
#include "usb.h"
#include "util.h"

#include "./ui_window.h"

#include "window.hpp"
#include "device_worker.hpp"

Window::Window(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Window)
{
    ui->setupUi(this);

    connect(ui->selectButton, SIGNAL(clicked()), this, SLOT(selectPayload()));
    connect(ui->uploadButton, SIGNAL(clicked()), this, SLOT(uploadPayload()));

    QThread* thread = new QThread();
    this->device_manager = new DeviceWorker(ui);
    this->device_manager->moveToThread(thread);
    connect(thread, &QThread::started, this->device_manager, &DeviceWorker::find_devices);
    connect(this->device_manager, &DeviceWorker::finished, thread, &QThread::quit);
    connect(this->device_manager, &DeviceWorker::finished, this->device_manager, &DeviceWorker::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    thread->start();
}


Window::~Window()
{
    this->device_manager->stop();

    QComboBox* combo = this->ui->deviceSelect;

    for (int i = 0; i < combo->count(); ++i)
    {
        libusb_unref_device(*(libusb_device**)combo->itemData(i).data());
    }
}

void Window::selectPayload()
{
    QString selected_path = QFileDialog::getOpenFileName(
        this,
        tr("Select Payload"),
        QDir::currentPath(),
        tr("Payload (*.bin);;All files (*.*)")
    );

    if (!selected_path.isEmpty())
    {
        QFileInfo fi(selected_path);
        QString fileName = fi.fileName(); 

        this->payload_path = selected_path;
        this->ui->selectLabel->setText(fileName);
        this->ui->uploadButton->setEnabled(true);
    }

}

void Window::uploadPayload()
{
    auto cur_data = this->ui->deviceSelect->currentData();
    if (!cur_data.isValid())
    {
        QMessageBox::information(this, windowTitle(), tr("No device was selected"));
        return;
    }

    libusb_device* device = *(libusb_device**)cur_data.data();
    tegra_handle* handle;

    if (tegra_open(device, &handle))
    {
        QMessageBox::information(this, windowTitle(), tr("Couldn't open device"));
        return;
    }

    uint8_t device_id[16];
    if (tegra_read_device_id(handle, device_id))
    {
        tegra_close(handle);
        QMessageBox::information(this, windowTitle(), tr("Failed to read Device ID\nStack may already be smashed"));
        return;
    }

    uint8_t* payload_buffer;
    size_t payload_size = load_file(this->payload_path.toStdString().c_str(), &payload_buffer);
    if (!payload_buffer)
    {
        tegra_close(handle);
        QMessageBox::information(this, windowTitle(), tr("Failed to load Payload"));
        return;
    }

    upload_payload(handle, payload_buffer, payload_size);
    tegra_switch_to_high_buffer(handle);

    free(payload_buffer);

    if (run_payload(handle) < 0)
    {
        QMessageBox::information(this, windowTitle(), tr("Something went wrong"));
    }

    tegra_close(handle);
}