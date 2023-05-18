#include <QApplication>
#include <libusb.h>

#include "window.hpp"

int main(int argc, char *argv[])
{
    if (libusb_init(NULL) < 0)
    {
        fprintf(stderr, "Failed to init libusb\n");
        return -1;
    }

    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("fuseeqt");
    QCoreApplication::setApplicationName("fuseeqt");

    Window w;

    w.show();
    int retval = a.exec();

    libusb_exit(NULL);

    return retval;
}