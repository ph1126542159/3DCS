// A8 plugin host implementation (README §13).
#include "opendva/plugin/PluginHost.h"

#include <cstdio>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace opendva::plugin {
namespace {

// Plugin entry-point signatures (README §13). Resolved by name on load.
using DllInitFn = void (*)();
using DllExitFn = void (*)();

// The active host for the current registration scope. Global C ABI symbols
// below forward to whatever this points at. nullptr means "no host" — calls are
// dropped (registration) or routed to stderr (logging).
PluginHost* g_activeHost = nullptr;

void defaultSink(const std::string& message, bool isHint) {
    std::fprintf(stderr, "[plugin %s] %s\n", isHint ? "hint" : "log", message.c_str());
}

void defaultMessageSink(const std::string& message) {
    std::fprintf(stderr, "[plugin message] %s\n", message.c_str());
}

bool isKnownCalType(dcsCalType type) {
    switch (type) {
        case dcsCalTypeIntern:
        case dcsCalTypeMove:
        case dcsCalTypeMoveDlg:
        case dcsCalTypeTole:
        case dcsCalTypeToleDlg:
        case dcsCalTypeMeas:
        case dcsCalTypeMeasDlg:
        case dcsCalTypeExec:
            return true;
    }
    return false;
}

#ifdef _WIN32
void* nativeLoad(const std::string& path) {
    return reinterpret_cast<void*>(::LoadLibraryA(path.c_str()));
}
void* nativeSymbol(void* handle, const char* name) {
    return reinterpret_cast<void*>(
        ::GetProcAddress(reinterpret_cast<HMODULE>(handle), name));
}
void nativeUnload(void* handle) {
    ::FreeLibrary(reinterpret_cast<HMODULE>(handle));
}
#else
void* nativeLoad(const std::string& path) {
    return ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
}
void* nativeSymbol(void* handle, const char* name) {
    return ::dlsym(handle, name);
}
void nativeUnload(void* handle) {
    ::dlclose(handle);
}
#endif

}  // namespace

PluginHost* PluginHost::active() { return g_activeHost; }

void PluginHost::setActive(PluginHost* host) { g_activeHost = host; }

PluginHost::PluginHost() : sink_(&defaultSink), messageSink_(&defaultMessageSink) {}

PluginHost::~PluginHost() {
    unloadAll();
    if (g_activeHost == this) g_activeHost = nullptr;
}

void PluginHost::setLogSink(LogSink sink) {
    sink_ = sink ? std::move(sink) : LogSink(&defaultSink);
}

void PluginHost::setMessageSink(MessageSink sink) {
    messageSink_ = sink ? std::move(sink) : MessageSink(&defaultMessageSink);
}

void PluginHost::registerRoutine(const std::string& name, dcsCalFuncPtr fn,
                                 dcsCalType type) {
    if (name.empty() || !fn || !isKnownCalType(type)) return;
    // README §13.4: (name, type) is the identity. Re-registering replaces.
    for (auto& r : routines_) {
        if (r.name == name && r.type == type) {
            r.fn = fn;
            return;
        }
    }
    routines_.push_back({name, fn, type});
}

void PluginHost::removeRoutine(const std::string& name, dcsCalType type) {
    for (auto it = routines_.begin(); it != routines_.end(); ++it) {
        if (it->name == name && it->type == type) {
            routines_.erase(it);
            return;
        }
    }
}

bool PluginHost::invokeRoutine(const std::string& name, dcsCalType type,
                               dcsDataPtr data) {
    for (const auto& r : routines_) {
        if (r.name == name && r.type == type) {
            if (!r.fn) return false;
            ActiveScope scope(this);
            r.fn(data);
            return true;
        }
    }
    return false;
}

void PluginHost::writeLog(const std::string& message) {
    if (sink_) sink_(message, /*isHint=*/false);
}

void PluginHost::displayHint(const std::string& hint) {
    if (sink_) sink_(hint, /*isHint=*/true);
}

void PluginHost::displayMessage(const std::string& message) {
    if (messageSink_) messageSink_(message);
}

bool PluginHost::loadPlugin(const std::string& path) {
    void* handle = nativeLoad(path);
    if (!handle) return false;

    auto initFn = reinterpret_cast<DllInitFn>(nativeSymbol(handle, "dcsDLLInit"));
    if (!initFn) {
        nativeUnload(handle);
        return false;
    }

    handles_.push_back(handle);
    {
        ActiveScope scope(this);
        initFn();  // plugin registers routines through the C ABI -> this host
    }
    return true;
}

void PluginHost::unloadAll() {
    for (auto it = handles_.rbegin(); it != handles_.rend(); ++it) {
        ActiveScope scope(this);
        auto exitFn = reinterpret_cast<DllExitFn>(nativeSymbol(*it, "dcsDLLExit"));
        if (exitFn) exitFn();  // plugin removes its own routines here
        nativeUnload(*it);
    }
    handles_.clear();
}

}  // namespace opendva::plugin

// ---- C ABI definitions (declared non-extern in dcs_plugin_api.h) ----
// These are the symbols a loaded plugin links against. They forward to the
// active host, which loadPlugin() installs for the duration of dcsDLLInit /
// dcsDLLExit. Defined with C linkage to match the header's extern "C" block.

extern "C" void dcsApiRegisterCalFunc(const char* name, dcsCalFuncPtr fn,
                                      dcsCalType type) {
    if (auto* host = ::opendva::plugin::PluginHost::active())
        host->registerRoutine(name ? name : "", fn, type);
}

extern "C" void dcsApiRemoveCalFunc(const char* name, dcsCalType type) {
    if (auto* host = ::opendva::plugin::PluginHost::active())
        host->removeRoutine(name ? name : "", type);
}

extern "C" void dcsApiLogWrite(const char* str) {
    if (auto* host = ::opendva::plugin::PluginHost::active())
        host->writeLog(str ? str : "");
}

extern "C" void dcsApiDisplayHint(const char* hint) {
    if (auto* host = ::opendva::plugin::PluginHost::active())
        host->displayHint(hint ? hint : "");
}

extern "C" void dcsApiDisplayMsg(const char* msg) {
    if (auto* host = ::opendva::plugin::PluginHost::active())
        host->displayMessage(msg ? msg : "");
}
