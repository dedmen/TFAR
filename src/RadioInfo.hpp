#pragma once
#include "CachedVariable.hpp"

class PlayerInfo;

class RadioInfo : public std::enable_shared_from_this<RadioInfo> {
public:
    RadioInfo(std::shared_ptr<MainthreadScheduler> scheduler, object obj, r_string variable);
    RadioInfo(std::shared_ptr<MainthreadScheduler> scheduler, r_string variable);

    template <class Type, class Func>
    CachedValueMTS<Type> makeCachedVal(std::chrono::milliseconds interval, Func&& updateFunc, Type defaultValue) {
        return std::make_shared<CachedValueMT<Type>>(scheduler, interval, std::forward<Func>(updateFunc), std::move(defaultValue));
    }

    template <class Type, class Func>
    CachedValueMTS<Type> makeCachedVal(std::chrono::milliseconds interval, Func&& updateFunc, std::function<bool()> canUpdateFunc, Type defaultValue) {
        return std::make_shared<CachedValueMT<Type>>(scheduler, interval, std::forward<Func>(updateFunc), std::move(canUpdateFunc), std::move(defaultValue));
    }

    void initValues();

    std::shared_ptr<MainthreadScheduler> scheduler;

    bool checkVar; //used by playerInfo to detect removed radios

    bool isLR;
    object obj;
    r_string variable;

    CachedValueMTS<bool> speakerEnabled;
    CachedValueMTS<r_string> radioCode;
    CachedValueMTS<std::vector<r_string>> frequencies;
    CachedValueMTS<r_string> netID;
    CachedValueMTS<float> volume;

    std::string buildString(const PlayerInfo& player) const;
};
