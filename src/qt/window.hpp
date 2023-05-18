#ifndef WINDOW_HPP
#define WINDOW_HPP

#include <QMainWindow>

#include "device_worker.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class Window; }
QT_END_NAMESPACE

class Window : public QMainWindow
{
    Q_OBJECT

public:
    Window(QWidget* parent = nullptr);
    ~Window();

private:
    QString payload_path;
    Ui::Window *ui;

    DeviceWorker* device_manager;

private slots:
    void selectPayload();
    void uploadPayload();
};
#endif // WINDOW_HPP
