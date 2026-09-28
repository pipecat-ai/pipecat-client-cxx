//
// Copyright (c) 2024, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "pipecat/rtvi/messages.h"
#include "pipecat/version.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <sys/sysctl.h>
#elif !defined(_WIN32)
#include <sys/utsname.h>
#endif

namespace pipecat::rtvi {

namespace {

const char* LIBRARY_NAME = "pipecat-client-cxx";

std::optional<std::string> platform_name() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__ANDROID__)
    return "Android";
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    return "iOS";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#elif defined(__QNX__)
    return "QNX";
#else
    return std::nullopt;
#endif
}

std::optional<std::string> platform_version() {
#if defined(__APPLE__)
    // The OS version (e.g. "15.1"), not the Darwin kernel version.
    char version[64];
    size_t size = sizeof(version);
    if (sysctlbyname("kern.osproductversion", version, &size, nullptr, 0) ==
        0) {
        return std::string(version);
    }
    return std::nullopt;
#elif defined(_WIN32)
    return std::nullopt;
#else
    struct utsname name;
    if (uname(&name) == 0) {
        return std::string(name.release);
    }
    return std::nullopt;
#endif
}

const char* architecture() {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#elif defined(__riscv) && __riscv_xlen == 64
    return "riscv64";
#else
    return "unknown";
#endif
}

std::string compiler() {
#if defined(__clang__)
    return "Clang " __clang_version__;
#elif defined(__GNUC__)
    return "GCC " __VERSION__;
#elif defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_FULL_VER);
#else
    return "unknown";
#endif
}

}  // namespace

AboutClientData default_about_client() {
    AboutClientData about;
    about.library = LIBRARY_NAME;
    about.library_version = PIPECAT_VERSION;
    about.platform = platform_name();
    about.platform_version = platform_version();
    about.platform_details = {
            {"arch", architecture()},
            {"compiler", compiler()},
    };
    return about;
}

}  // namespace pipecat::rtvi
