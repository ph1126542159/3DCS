// A8 plugin host registry tests (README §13). Exercises the registration logic
// without loading a real shared library — both the public PluginHost API and
// the global C ABI symbols that loaded plugins link against.
#include <string>
#include <vector>

#include "dva_test.h"
#include "opendva/dcs_plugin_api.h"
#include "opendva/plugin/PluginHost.h"

using opendva::plugin::PluginHost;
using opendva::plugin::RegisteredRoutine;

namespace {

// Dummy routines used only as recognisable function-pointer identities.
void dummyMove(dcsDataPtr) {}
void dummyTole(dcsDataPtr) {}
struct DummyMeasureData {
    double value{0.0};
};
void dummyMeasure(dcsDataPtr data) {
    auto* typed = static_cast<DummyMeasureData*>(data);
    typed->value = 42.5;
}

const RegisteredRoutine* find(const std::vector<RegisteredRoutine>& v,
                              const std::string& name, dcsCalType type) {
    for (const auto& r : v)
        if (r.name == name && r.type == type) return &r;
    return nullptr;
}

}  // namespace

TEST("pluginhost: dcsCalType values match 3DCS User-DLL ABI order") {
    dvatest::check(static_cast<int>(dcsCalTypeIntern) == 0, "Intern ABI value");
    dvatest::check(static_cast<int>(dcsCalTypeMove) == 1, "Move ABI value");
    dvatest::check(static_cast<int>(dcsCalTypeMoveDlg) == 2, "MoveDlg ABI value");
    dvatest::check(static_cast<int>(dcsCalTypeTole) == 3, "Tole ABI value");
    dvatest::check(static_cast<int>(dcsCalTypeToleDlg) == 4, "ToleDlg ABI value");
    dvatest::check(static_cast<int>(dcsCalTypeMeas) == 5, "Meas ABI value");
    dvatest::check(static_cast<int>(dcsCalTypeMeasDlg) == 6, "MeasDlg ABI value");
    dvatest::check(static_cast<int>(dcsCalTypeExec) == 7, "Exec ABI value");
}

TEST("pluginhost: registerRoutine adds an entry to the registry") {
    PluginHost host;
    host.registerRoutine("myMove", &dummyMove, dcsCalTypeMove);
    dvatest::check(host.routines().size() == 1, "expected one routine");
    const auto* r = find(host.routines(), "myMove", dcsCalTypeMove);
    dvatest::check(r != nullptr, "routine not found");
    dvatest::check(r->fn == &dummyMove, "function pointer mismatch");
}

TEST("pluginhost: C ABI dcsApiRegisterCalFunc forwards to active host") {
    PluginHost host;
    PluginHost::ActiveScope scope(&host);  // route bare C ABI calls to host
    dcsApiRegisterCalFunc("test", &dummyMove, dcsCalTypeMove);
    const auto* r = find(host.routines(), "test", dcsCalTypeMove);
    dvatest::check(r != nullptr, "C ABI registration did not reach host");
    dvatest::check(r->type == dcsCalTypeMove, "type mismatch");
    dvatest::check(r->fn == &dummyMove, "fn mismatch via C ABI");
}

TEST("pluginhost: same name different type are distinct routines") {
    PluginHost host;
    host.registerRoutine("dup", &dummyMove, dcsCalTypeMove);
    host.registerRoutine("dup", &dummyTole, dcsCalTypeTole);
    dvatest::check(host.routines().size() == 2, "expected two distinct routines");
    dvatest::check(find(host.routines(), "dup", dcsCalTypeMove)->fn == &dummyMove,
                   "move slot wrong");
    dvatest::check(find(host.routines(), "dup", dcsCalTypeTole)->fn == &dummyTole,
                   "tole slot wrong");
}

TEST("pluginhost: re-register replaces fn for same (name,type)") {
    PluginHost host;
    host.registerRoutine("r", &dummyMove, dcsCalTypeMove);
    host.registerRoutine("r", &dummyTole, dcsCalTypeMove);
    dvatest::check(host.routines().size() == 1, "should not duplicate");
    dvatest::check(find(host.routines(), "r", dcsCalTypeMove)->fn == &dummyTole,
                   "fn was not replaced");
}

TEST("pluginhost: registerRoutine ignores null function pointers") {
    PluginHost host;
    host.registerRoutine("nullMeasure", nullptr, dcsCalTypeMeas);
    dvatest::check(host.routines().empty(),
                   "null function pointer must not enter registry");

    DummyMeasureData data;
    dvatest::check(!host.invokeRoutine("nullMeasure", dcsCalTypeMeas, &data),
                   "ignored null routine must not dispatch");
}

TEST("pluginhost: registerRoutine ignores empty routine names") {
    PluginHost host;
    host.registerRoutine("", &dummyMeasure, dcsCalTypeMeas);
    dvatest::check(host.routines().empty(),
                   "empty routine name must not enter registry");

    PluginHost::ActiveScope scope(&host);
    dcsApiRegisterCalFunc(nullptr, &dummyMove, dcsCalTypeMove);
    dvatest::check(host.routines().empty(),
                   "null C ABI routine name must not enter registry");
}

TEST("pluginhost: registerRoutine ignores unknown routine types") {
    PluginHost host;
    const auto unknownType = static_cast<dcsCalType>(99);
    host.registerRoutine("unknown", &dummyMove, unknownType);
    dvatest::check(host.routines().empty(),
                   "unknown routine type must not enter registry");

    DummyMeasureData data;
    dvatest::check(!host.invokeRoutine("unknown", unknownType, &data),
                   "unknown routine type must not dispatch");
}

TEST("pluginhost: removeRoutine / dcsApiRemoveCalFunc erases the entry") {
    PluginHost host;
    PluginHost::ActiveScope scope(&host);
    dcsApiRegisterCalFunc("gone", &dummyMove, dcsCalTypeMeas);
    dvatest::check(find(host.routines(), "gone", dcsCalTypeMeas) != nullptr,
                   "precondition: routine present");
    dcsApiRemoveCalFunc("gone", dcsCalTypeMeas);
    dvatest::check(find(host.routines(), "gone", dcsCalTypeMeas) == nullptr,
                   "routine should be removed");
    dvatest::check(host.routines().empty(), "registry should be empty");
}

TEST("pluginhost: log sink is injectable and receives messages") {
    PluginHost host;
    std::vector<std::string> logs;
    std::vector<std::string> hints;
    host.setLogSink([&](const std::string& m, bool isHint) {
        (isHint ? hints : logs).push_back(m);
    });
    PluginHost::ActiveScope scope(&host);
    dcsApiLogWrite("hello");
    dcsApiDisplayHint("tip");
    dvatest::check(logs.size() == 1 && logs[0] == "hello", "log not captured");
    dvatest::check(hints.size() == 1 && hints[0] == "tip", "hint not captured");
}

TEST("pluginhost: display message sink receives dcsApiDisplayMsg") {
    PluginHost host;
    std::vector<std::string> messages;
    host.setMessageSink([&](const std::string& m) {
        messages.push_back(m);
    });
    PluginHost::ActiveScope scope(&host);
    dcsApiDisplayMsg("error message");
    dvatest::check(messages.size() == 1 && messages[0] == "error message",
                   "display message not captured");
}

TEST("pluginhost: loadPlugin fails gracefully on a missing library") {
    PluginHost host;
    dvatest::check(!host.loadPlugin("definitely_not_a_real_plugin_xyz.dll"),
                   "loading a nonexistent library must return false");
    dvatest::check(host.routines().empty(), "no routines from a failed load");
}

TEST("pluginhost: invokeRoutine dispatches a registered measure by name and type") {
    PluginHost host;
    host.registerRoutine("customMeasure", &dummyMeasure, dcsCalTypeMeas);

    DummyMeasureData data;
    dvatest::check(host.invokeRoutine("customMeasure", dcsCalTypeMeas, &data),
                   "registered measure routine should be invoked");
    dvatest::checkNear(data.value, 42.5, 1e-12,
                       "routine should receive and update the opaque measure data");
    dvatest::check(!host.invokeRoutine("customMeasure", dcsCalTypeMove, &data),
                   "wrong routine type must not dispatch");
    dvatest::check(!host.invokeRoutine("missing", dcsCalTypeMeas, &data),
                   "missing routine must return false");
}

TEST("pluginhost: active() is null outside any ActiveScope") {
    // After the previous scopes unwind, no host should be active.
    dvatest::check(PluginHost::active() == nullptr,
                   "active host leaked outside ActiveScope");
}
