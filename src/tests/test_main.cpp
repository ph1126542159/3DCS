// Test runner: discovers all registered TESTs and reports pass/fail.
#include <cstdio>

#include "dva_test.h"

int main() {
    int passed = 0, failed = 0;
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
