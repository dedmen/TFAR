#pragma once
#include <intercept.hpp>
#include <utility>
#include "MainthreadScheduler.hpp"
#include "SignalSlot.hpp"

using namespace std::chrono_literals;


class CachedVariable {
public:
    CachedVariable(r_string name, std::chrono::milliseconds interval) : variableName(std::move(name)), interval(interval) {}

    game_value get() {
        if (std::chrono::system_clock::now() > lastUpdate + interval || value.is_nil()) {
            value = intercept::sqf::get_variable(intercept::sqf::mission_namespace(), variableName);
        }
        return value;
    }

private:
    r_string variableName;
    game_value value;
    std::chrono::system_clock::time_point lastUpdate;
    std::chrono::milliseconds interval;
};

template <class Type>
class CachedValue {
public:
    CachedValue(r_string name, std::chrono::milliseconds interval, std::function<Type()> updateFunc) :
        variableName(std::move(name)), updateFunc(std::move(updateFunc)), interval(interval) {}

    game_value get() {
        if (std::chrono::system_clock::now() > lastUpdate + interval || value.is_nil()) {
            value = updateFunc();
        }
        return value;
    }

private:
    r_string variableName;
    std::function<Type()> updateFunc;
    std::chrono::milliseconds interval;

    std::chrono::system_clock::time_point lastUpdate;
    Type value;
};


static inline __itt_domain* CachedValueMTDomain = __itt_domain_create("CachedValueMT");

static inline __itt_string_handle* CachedValueMT_get = __itt_string_handle_create("get");
static inline __itt_string_handle* CachedValueMT_doUpdate = __itt_string_handle_create("doUpdate");
static inline __itt_string_handle* CachedValueMT_doUpdate_ex = __itt_string_handle_create("doUpdate_ex");

template <class Type>
class CachedValueMT : public std::enable_shared_from_this<CachedValueMT<Type>> {
public:
    CachedValueMT(std::weak_ptr<MainthreadScheduler> scheduler, std::chrono::milliseconds interval, std::function<Type()> updateFunc, Type defaultValue) :
        scheduler(std::move(scheduler)), updateFunc(std::move(updateFunc)), interval(interval), value(defaultValue) {
        lastChange = std::chrono::system_clock::now();
    }
    CachedValueMT(std::weak_ptr<MainthreadScheduler> scheduler, std::chrono::milliseconds interval, std::function<Type()> updateFunc, std::function<bool()> canUpdate, Type defaultValue) :
        scheduler(std::move(scheduler)), updateFunc(std::move(updateFunc)), canUpdate(canUpdate), interval(interval), value(defaultValue) {
        lastChange = std::chrono::system_clock::now();
    }

    Type get() const {
        ittScope sc(CachedValueMTDomain, CachedValueMT_get);
        if (std::chrono::system_clock::now() - lastUpdate > interval) {
            doUpdate();
        }

        std::unique_lock lock(valueMutex);
        return value;
    }

    void setInterval(std::chrono::milliseconds newInterval) {
        interval = newInterval;
    }

    void forceUpdate() {
        //Only if there's not already a update in progress
        if (lastUpdate != std::numeric_limits<std::chrono::system_clock::time_point>::max())
            doUpdate();
    }

    void manualUpdate(Type newValue, bool fireEvents = false) {
        if (lastUpdate != std::chrono::system_clock::time_point::max()) {
            std::unique_lock lock(valueMutex);
            value = newValue;
            if (fireEvents)
                onUpdate(newValue);
            lastUpdate = std::chrono::system_clock::now();
        }
    }

    void addUpdateEventhandler(typename Signal<void(Type)>::Slot handler) {
        onUpdate.connect(handler);
    }

    std::weak_ptr<CachedValueMT<Type>> getWeak() {
        return this->weak_from_this();
    }

    void setName(std::string x) {
        
//#ifdef _DEBUG
        name = std::move(x);
        evt = __itt_event_create(name.c_str(), name.length());
//#endif
    }

    std::chrono::system_clock::time_point getLastChange() {
        return lastChange;
    }


private:

    void doUpdate() const {
        ittScope sc(CachedValueMTDomain, CachedValueMT_doUpdate);
        if (canUpdate && !(*canUpdate)()) { //If we are not allowed to update, just skip
            lastUpdate = std::chrono::system_clock::now();
            return;
        }
        lastUpdate = std::chrono::system_clock::time_point::max(); //Just mark to stop updates while thread is working

        if (auto sched = scheduler.lock()) {
            std::weak_ptr<const CachedValueMT<Type>> value = this->shared_from_this();
            sched->pushTask([value, evt = evt]() {
//#ifdef _DEBUG
                ittScopeEvt sc(evt);
//#endif
                std::shared_ptr<const CachedValueMT<Type>> lockedValue = value.lock();
                if (!lockedValue) return;
                auto newValue = lockedValue->updateFunc();
                std::unique_lock lock(lockedValue->valueMutex);
                if (newValue != lockedValue->value) {
                    lockedValue->onUpdate(newValue);
                    lockedValue->lastChange = std::chrono::system_clock::now();
                }
                lockedValue->value = newValue;
                lockedValue->lastUpdate = std::chrono::system_clock::now();
            });
        }
        else {
            __debugbreak();
        }
    }

//#ifdef _DEBUG
    std::string name;
    __itt_event evt = 0;
//#endif


    const std::weak_ptr<MainthreadScheduler> scheduler;

    const std::function<Type()> updateFunc;
    const std::optional<std::function<bool()>> canUpdate;
    std::chrono::milliseconds interval;

    mutable std::chrono::system_clock::time_point lastUpdate;
    mutable std::chrono::system_clock::time_point lastChange;
    mutable std::recursive_mutex valueMutex;
    mutable Type value;
    Signal<void(const Type&)> onUpdate;
};

template <class Type>
using CachedValueMTS = std::shared_ptr<CachedValueMT<Type>>;

template <class Type>
using CachedValueMTW = std::shared_ptr<CachedValueMT<Type>>;