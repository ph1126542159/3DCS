// Qt UI test runner: uses the lightweight DVA harness under QApplication.
#include <cstdio>
#include <exception>

#include <QApplication>
#include <QByteArray>

#include "dva_test.h"

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);

    int passed = 0;
    int failed = 0;
    for (auto& c : dvatest::registry()) {
        try {
            std::printf("[ RUN  ] %s\n", c.name.c_str());
            std::fflush(stdout);
            c.fn();
            std::printf("[ PASS ] %s\n", c.name.c_str());
            std::fflush(stdout);
            ++passed;
        } catch (const dvatest::Failure& f) {
            std::printf("[ FAIL ] %s: %s\n", c.name.c_str(), f.msg.c_str());
            std::fflush(stdout);
            ++failed;
        } catch (const std::exception& e) {
            std::printf("[ ERR  ] %s: %s\n", c.name.c_str(), e.what());
            std::fflush(stdout);
            ++failed;
        }
    }
    std::printf("\n%d passed, %d failed\n", passed, failed);
    std::fflush(stdout);
    return failed == 0 ? 0 : 1;
}
