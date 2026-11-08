#include "aiws/gui/main_window.hpp"

#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>

using aiws::gui::MainWindow;

class StudentTests final : public QObject
{
    Q_OBJECT

private:
    static QString create_test_doc(QTemporaryDir &directory,
                                   const QString &filename)
    {
        const QString path = directory.filePath(filename);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return {};

        if (file.write("file contents!") < 0)
            return {};

        return path;
    }

    static QPushButton *find_button(MainWindow &window, const QString &text)
    {
        for (auto *button : window.findChildren<QPushButton *>())
        {
            if (button->text() == text)
                return button;
        }
        return nullptr;
    }

private slots:
    void base_build_test()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString first_path = create_test_doc(directory, "first.txt");
        const QString second_path = create_test_doc(directory, "second.txt");
        QVERIFY(!first_path.isEmpty());
        QVERIFY(!second_path.isEmpty());

        MainWindow window;
        auto *status = window.findChild<QLabel *>("currentStatus");
        QVERIFY(status != nullptr);
        window.set_build_delay_ms(750);
        QVERIFY(window.add_document_file(first_path));

        auto *build_button = find_button(window, "Build Corpus");
        QVERIFY(build_button != nullptr);
        QElapsedTimer elapsed;
        elapsed.start();
        build_button->click();

        QVERIFY2(elapsed.elapsed() < 500,
                 "Starting a delayed build blocked the GUI thread");
        QCOMPARE(status->text(), QStringLiteral("Building Corpus..."));

        QVERIFY(window.add_document_file(second_path));
        QTRY_COMPARE_WITH_TIMEOUT(status->text(),
                                  QStringLiteral("Corpus Build Discarded"),
                                  5000);
    }

    void reset_while_building()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = create_test_doc(directory, "reset.txt");
        QVERIFY(!path.isEmpty());

        MainWindow window;
        auto *status = window.findChild<QLabel *>("currentStatus");
        QVERIFY(status != nullptr);
        window.set_build_delay_ms(400);
        QVERIFY(window.add_document_file(path));

        auto *build_button = find_button(window, "Build Corpus");
        auto *reset_button = find_button(window, "Reset");
        QVERIFY(build_button != nullptr);
        QVERIFY(reset_button != nullptr);

        build_button->click();
        QCOMPARE(status->text(), QStringLiteral("Building Corpus..."));
        reset_button->click();
        QCOMPARE(status->text(), QStringLiteral("Workspace Reset"));

        QTRY_COMPARE_WITH_TIMEOUT(status->text(),
                                  QStringLiteral("Corpus Build Discarded"),
                                  5000);
    }

    void report_rebuild_failure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = create_test_doc(directory, "duplicate.txt");
        QVERIFY(!path.isEmpty());

        MainWindow window;
        auto *status = window.findChild<QLabel *>("currentStatus");
        QVERIFY(status != nullptr);
        QVERIFY(window.add_document_file(path));
        QVERIFY(window.add_document_file(path));

        auto *build_button = find_button(window, "Build Corpus");
        QVERIFY(build_button != nullptr);
        build_button->click();

        QTRY_VERIFY_WITH_TIMEOUT(
            status->text().startsWith(QStringLiteral("Corpus Build Failed:")),
            5000);
    }

    void destroying_window_during_build_joins_worker()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = create_test_doc(directory, "lifecycle.txt");
        QVERIFY(!path.isEmpty());

        QElapsedTimer elapsed;
        elapsed.start();
        {
            MainWindow window;
            window.set_build_delay_ms(250);
            QVERIFY(window.add_document_file(path));

            auto *build_button = find_button(window, "Build Corpus");
            QVERIFY(build_button != nullptr);
            build_button->click();
        }

        QVERIFY2(elapsed.elapsed() < 5000,
                 "Destroying the window did not finish the worker promptly");
    }
};

QTEST_MAIN(StudentTests)
#include "student_tests.moc"
