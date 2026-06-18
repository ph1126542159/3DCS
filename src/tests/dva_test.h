// Minimal self-contained test harness shared by all test TUs.
#pragma once
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace dvatest {

struct Case {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> r;
    return r;
}

struct Registrar {
    Registrar(const std::string& n, std::function<void()> f) {
        registry().push_back({n, std::move(f)});
    }
};

struct Failure {
    std::string msg;
};

inline void check(bool cond, const std::string& msg) {
    if (!cond) throw Failure{msg};
}

inline void checkNear(double a, double b, double tol, const std::string& msg) {
    if (std::fabs(a - b) > tol)
        throw Failure{msg + " (got " + std::to_string(a) + ", expected " + std::to_string(b) + ")"};
}

}  // namespace dvatest

#define DVA_CONCAT_(a, b) a##b
#define DVA_CONCAT(a, b) DVA_CONCAT_(a, b)
#define TEST(name)                                                                \
    static void DVA_CONCAT(dva_test_, __LINE__)();                                \
    static ::dvatest::Registrar DVA_CONCAT(dva_reg_, __LINE__){                   \
        name, &DVA_CONCAT(dva_test_, __LINE__)};                                  \
    static void DVA_CONCAT(dva_test_, __LINE__)()
