#pragma once
#include <thread>

static inline auto mainthreadID = std::this_thread::get_id();

class MainthreadTester {
public:
#ifdef _DEBUG
    static void checkNow() {
        if (std::this_thread::get_id() != mainthreadID)
            __debugbreak();
    }
#else
    static void checkNow() {}
#endif
};

