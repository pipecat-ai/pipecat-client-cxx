//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_DAILY_CONTEXT_H
#define PIPECAT_DAILY_CONTEXT_H

namespace pipecat::daily {

// Keeps daily-core's context alive. daily-core supports one context per
// process, so all transports share it: the first ContextRef creates it and
// the last one destroys it. Thread-safe.
class ContextRef {
   public:
    ContextRef();
    ~ContextRef();

    ContextRef(const ContextRef&) = delete;
    ContextRef& operator=(const ContextRef&) = delete;
};

}  // namespace pipecat::daily

#endif
