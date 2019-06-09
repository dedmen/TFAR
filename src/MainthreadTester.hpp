#pragma once
#include <thread>
#include <ittnotify.h>
#include "../lib/ittnotify/ittnotify.h"


static inline __itt_domain* MTTesterDomain = __itt_domain_create("MainthreadTester");

static inline __itt_string_handle* MainthreadTester_checkNow = __itt_string_handle_create("checkNow");

class ittScope {
public:
    ittScope(__itt_domain* dm, __itt_string_handle* han) : dm(dm) { __itt_task_begin(dm, __itt_null, __itt_null, han); }
    ~ittScope() { __itt_task_end(dm); }
    __itt_domain* dm;
};

class ittScopeEvt {
public:
    ittScopeEvt(__itt_event evt) : evt(evt) { __itt_event_start(evt); }
    ~ittScopeEvt() { __itt_event_end(evt); }
    __itt_event evt;
};


static inline auto mainthreadID = std::this_thread::get_id();

class MainthreadTester {
public:
#ifdef _DEBUG
    static void checkNow() {
        ittScope sc(MTTesterDomain, MainthreadTester_checkNow);
        if (std::this_thread::get_id() != mainthreadID)
            __debugbreak();
    }
#else
    static void checkNow() {}
#endif
};

