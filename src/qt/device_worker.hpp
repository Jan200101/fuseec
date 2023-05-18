#ifndef DEVICE_WORKER_HPP
#define DEVICE_WORKER_HPP

#include <QObject>

#include <libusb.h>

QT_BEGIN_NAMESPACE
namespace Ui { class Window; }
QT_END_NAMESPACE

class DeviceWorker : public QObject {
    Q_OBJECT
public:
	DeviceWorker(Ui::Window* pui);

    void stop();

private:
    Ui::Window *ui;
	bool running = true;

public slots:
    void find_devices();

signals:
    void finished();
};

#endif