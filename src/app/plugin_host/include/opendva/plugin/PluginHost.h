// A8 plugin host (README §13). Loads User-DLL plugins via the C ABI declared
// in opendva/dcs_plugin_api.h and owns the registry of calculation routines.
//
// The C ABI functions (dcsApiRegisterCalFunc / dcsApiRemoveCalFunc /
// dcsApiLogWrite / dcsApiDisplayHint) are *global* C symbols defined in
// PluginHost.cpp. They forward to the currently active PluginHost instance,
// which a plugin's dcsDLLInit reaches through during loadPlugin().
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "opendva/dcs_plugin_api.h"

namespace opendva::plugin {

// One registered calculation routine (README §13.4). `fn` is the raw C entry
// the plugin supplied; `type` selects Move / Tolerance / Measure semantics.
struct RegisteredRoutine {
    std::string name;
    dcsCalFuncPtr fn{nullptr};
    dcsCalType type{dcsCalTypeMove};
};

// Sink for dcsApiLogWrite / dcsApiDisplayHint. Injectable for tests; defaults
// to stderr. Takes the message and a bool that is true for display hints.
using LogSink = std::function<void(const std::string& message, bool isHint)>;
using MessageSink = std::function<void(const std::string& message)>;

class PluginHost {
public:
    PluginHost();
    ~PluginHost();

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;

    // Dynamically loads a shared library (.dll / .so), resolves and invokes its
    // exported `dcsDLLInit` entry. Returns false if the library cannot be loaded
    // or the entry point is missing. While dcsDLLInit runs this host is the
    // active registration target.
    bool loadPlugin(const std::string& path);

    // Calls `dcsDLLExit` on every loaded library (if exported), then frees them.
    // Leaves the routine registry untouched (a plugin is expected to remove its
    // own routines in dcsDLLExit).
    void unloadAll();

    // Registry access (used by the UI dropdowns and by tests).
    const std::vector<RegisteredRoutine>& routines() const { return routines_; }

    // Routine table management. Normally driven by the C ABI from plugin code,
    // but public so headless tests can exercise the registry directly.
    void registerRoutine(const std::string& name, dcsCalFuncPtr fn, dcsCalType type);
    void removeRoutine(const std::string& name, dcsCalType type);
    bool invokeRoutine(const std::string& name, dcsCalType type, dcsDataPtr data);

    // Host services exposed to plugins.
    void writeLog(const std::string& message);
    void displayHint(const std::string& hint);
    void displayMessage(const std::string& message);

    // Replaces the log/hint sink. Passing an empty function restores stderr.
    void setLogSink(LogSink sink);
    void setMessageSink(MessageSink sink);

    // The host that global C ABI calls forward to (the most recently active).
    static PluginHost* active();

    // RAII scope that makes a host the active C ABI target, restoring the
    // previous one on destruction (supports nested scopes). loadPlugin() uses it
    // internally around dcsDLLInit; headless code/tests can use it to route bare
    // dcsApiRegisterCalFunc() calls to a chosen host without loading a library.
    class ActiveScope {
    public:
        explicit ActiveScope(PluginHost* host) : prev_(PluginHost::active()) {
            PluginHost::setActive(host);
        }
        ~ActiveScope() { PluginHost::setActive(prev_); }
        ActiveScope(const ActiveScope&) = delete;
        ActiveScope& operator=(const ActiveScope&) = delete;

    private:
        PluginHost* prev_;
    };

private:
    static void setActive(PluginHost* host);

    std::vector<RegisteredRoutine> routines_;
    std::vector<void*> handles_;  // native library handles (HMODULE / void*)
    LogSink sink_;
    MessageSink messageSink_;
};

}  // namespace opendva::plugin
