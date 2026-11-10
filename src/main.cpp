#include "aiws/gui/main_window.hpp"

#include <QApplication>
#include <QMessageBox>
#include <QDebug>

namespace
{
    void confirm_exit(QWidget *parent)
    {
        const auto answer = QMessageBox::question(
            parent,
            "Confirm Exit",
            "Are you sure you want to exit?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (answer == QMessageBox::Yes)
        {
            QApplication::quit();
        }
    }
}

int parse_build_delay(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--build-delay-ms")
        {
            return std::stoi(argv[i + 1]);
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    aiws::gui::MainWindow window;
    int time = parse_build_delay(argc, argv);
    window.set_build_delay_ms(time);
    window.set_exit_callback(confirm_exit);
    window.show();

    return app.exec();
}