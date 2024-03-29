#pragma once
#include <intercept.hpp>
#include <memory>
#include <string>
#include "CachedVariable.hpp"
#include "RadioInfo.hpp"


struct PositionInfo {
    vector3 eyePos;
    vector3 eyeDirection;

    bool operator !=(const PositionInfo& other) const {
        return !(eyePos == other.eyePos) || 
            !(eyeDirection == other.eyeDirection);
    }

};

class PlayerInfo : public std::enable_shared_from_this<PlayerInfo> {
public:
    PlayerInfo(std::shared_ptr<MainthreadScheduler> scheduler, object unit);
    void init();
    void simulate();
    void updateIntervals();
    void sendToTeamspeak();
    void updateRadios();
    void grabRadios(auto_array<r_string>& radioData);




    std::shared_ptr<MainthreadScheduler> scheduler;

    template <class Type, class Func>
    CachedValueMTS<Type> makeCachedVal(std::chrono::milliseconds interval, Func&& updateFunc, Type&& defaultValue) {
        return std::make_shared<CachedValueMT<Type>>(scheduler, interval, std::forward<Func>(updateFunc), std::forward<Type>(defaultValue));
    }

    template <class Type, class Func>
    CachedValueMTS<Type> makeCachedVal(std::chrono::milliseconds interval, Func&& updateFunc, std::function<bool()> canUpdateFunc, Type&& defaultValue) {
        return std::make_shared<CachedValueMT<Type>>(scheduler, interval, std::forward<Func>(updateFunc), std::move(canUpdateFunc), std::forward<Type>(defaultValue));
    }

#pragma region mainThreadFunctions
    CachedValueMTS<game_value> positionFunc;
    PositionInfo getPosition() const;
    r_string getVehicleID() const;
    bool getIsolatedAndInside() const;
    static bool isVehicleIsolated(const object& vehicle);

#pragma endregion mainThreadFunctions

    bool checkFlag = false; //Used by Controller.cpp







    object controlledUnit;
    object unit;
    bool isCurrentUnit = false;
    __itt_event evt;



    std::string unitName;

    std::mutex radiosLock;
    std::vector<std::shared_ptr<RadioInfo>> radios;
    CachedValueMTS<PositionInfo> position;
    CachedValueMTS<bool> isSpectating;

    CachedValueMTS<object> unitParent; //#TODO forceUpdate on controlledUnit change
    CachedValueMTS<r_string> vehicleID;
    CachedValueMTS<bool> isolatedAndInside;
    CachedValueMTS<float> terrainInterception;
    CachedValueMTS<float> objectInterception;

    CachedValueMTS<bool> radioUpdate;

    std::chrono::system_clock::time_point lastFullUpdate;
    std::chrono::system_clock::time_point lastUpdateSent;

    std::chrono::milliseconds updateSendDelay;
};

