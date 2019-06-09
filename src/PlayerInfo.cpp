#include "PlayerInfo.hpp"
#include "CacheHelper.hpp"
#include "Controller.hpp"

std::chrono::milliseconds timeInterp(float value, std::chrono::milliseconds minTime, std::chrono::milliseconds maxTime, float minValue, float maxValue) {
    if (value < minValue) return minTime;
    if (value > maxValue) return maxTime;

    return std::chrono::duration_cast<std::chrono::milliseconds>(minTime + (maxTime - minTime) * (value - minValue) / (maxValue - minValue));
}

std::string vectorToString(const vector3& vec) {
    std::string ret;
    ret.reserve(7);
    ret += "["sv;
    ret += std::to_string(vec.x);
    ret += ","sv;
    ret += std::to_string(vec.y);
    ret += ","sv;
    ret += std::to_string(vec.z);
    ret += "]"sv;
    return ret;
}



PlayerInfo::PlayerInfo(std::shared_ptr<MainthreadScheduler> sched, object unit) :

    positionFunc(),
    scheduler(sched),
    controlledUnit(unit),
    unit(unit),
    unitName(intercept::sqf::name(unit))
 
{
    positionFunc = makeCachedVal<game_value>(200ms, [player = weak_from_this()]() -> game_value {
        if (auto lockedPlayer = player.lock())
            return intercept::sqf::get_variable(lockedPlayer->controlledUnit, "TF_fnc_position"sv);
        return {};
    }, game_value{});

    position = makeCachedVal<PositionInfo>(1s, [player = weak_from_this()]()->PositionInfo {
        if (auto lockedPlayer = player.lock())
            return lockedPlayer->getPosition();
        return {};
    }, {});
    isSpectating = makeCachedVal<bool>(200ms, [player = weak_from_this()]() -> bool {
        if (auto lockedPlayer = player.lock())
            return intercept::sqf::get_variable(lockedPlayer->controlledUnit, "TFAR_forceSpectator"sv);
        return false;
    }, false);
    unitParent = makeCachedVal<object>(200ms, [player = weak_from_this()]()->object {
        if (auto lockedPlayer = player.lock())
            return intercept::sqf::object_parent(lockedPlayer->controlledUnit);
        return {};
    }, {});
    vehicleID = makeCachedVal<r_string>(1000ms, [player = weak_from_this()]()->r_string {
        if (auto lockedPlayer = player.lock())
            return lockedPlayer->getVehicleID();
        return {};
    }, [player = weak_from_this()]()->bool {
        if (auto lockedPlayer = player.lock()) //Only update if we are in a vehicle
            return !lockedPlayer->unitParent->get().is_null();
        return false;
    }, {});

    isolatedAndInside = makeCachedVal<bool>(200ms, [player = weak_from_this()]()->bool {
        if (auto lockedPlayer = player.lock())
            return lockedPlayer->getIsolatedAndInside();
        return {};
    }, [player = weak_from_this()]()->bool {
        if (auto lockedPlayer = player.lock()) //Only update if we are in a vehicle
            return !lockedPlayer->unitParent->get().is_null();
        return false;
    }, game_value{});


    //Immediately update vehicleID and isolatedAndInside on vehicle changed
    unitParent->addUpdateEventhandler([vehicleID = vehicleID->getWeak(), isolatedAndInside = isolatedAndInside->getWeak(), player = weak_from_this()](const object& newParent) {
        if (auto lockedPlayer = player.lock()) {
            if (auto lockedVehicleID = vehicleID.lock())
                lockedVehicleID->manualUpdate(lockedPlayer->getVehicleID());
            if (auto lockedISO = isolatedAndInside.lock())
                lockedISO->manualUpdate(lockedPlayer->getIsolatedAndInside());
        }
    });
}

void PlayerInfo::simulate() {
    if (std::chrono::system_clock::now() - lastUpdateSent > 100ms)
        sendToTeamspeak();
}

void PlayerInfo::updateIntervals() {
    


}

void PlayerInfo::sendToTeamspeak() {
    auto currentUnit = Controller::get().currentUnit;
    if (!currentUnit) return;

    if (!Controller::get().networkHandler.canDoAsyncRequest()) return;

    auto curPos = position->get();
    bool isolatedInside = isolatedAndInside->get();


    bool canSpeak = curPos.eyePos.z > 0 || isolatedInside;
    bool isRemote = !isCurrentUnit; //#TODO just compare with currentUnit

    bool useSR = true;
    bool useLR = true;
    bool useDD = false;

    if (curPos.eyePos.z < 0) {

        auto TFAR_fnc_canUseSWRadio = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_canUseSWRadio"sv);
        auto TFAR_fnc_canUseLRRadio = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_canUseLRRadio"sv);
        auto TFAR_fnc_canUseDDRadio = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_canUseDDRadio"sv);
        //#TODO implement this in c++
        useSR = intercept::sqf::call(TFAR_fnc_canUseSWRadio, { controlledUnit, isolatedInside, canSpeak, curPos.eyePos.z });
        useLR = intercept::sqf::call(TFAR_fnc_canUseLRRadio, { controlledUnit, isolatedInside, curPos.eyePos.z });
        useDD = intercept::sqf::call(TFAR_fnc_canUseDDRadio, { controlledUnit, isolatedInside });
    }

    bool nearPlayer = currentUnit->position->get().eyePos.distance(curPos.eyePos) < 40; //#TODO use voice volume

    float objectInterception = 0;
    float terrainInterception = 0;

    if (nearPlayer) {
        
        if (isRemote && Controller::get().objectInterceptionEnabled->get()) {
            auto TFAR_fnc_objectInterception = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_objectInterception"sv);
            //#TODO 50ms+ caching
            objectInterception = intercept::sqf::call(TFAR_fnc_objectInterception, controlledUnit);
        }

    } else {
        auto TFAR_fnc_calcTerrainInterception = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_calcTerrainInterception"sv);
        //#TODO 500ms+ caching
        terrainInterception = intercept::sqf::call(TFAR_fnc_calcTerrainInterception, controlledUnit);
    }

    bool isEnemy = false;
    if (isRemote && isSpectating) {
        //#TODO 60s caching

    }


    std::string data;
    data.reserve(512);
    data += "POS\t"sv;
    data += unitName;
    data += "\t";
    data += vectorToString(curPos.eyePos);
    data += "\t";
    data += vectorToString(curPos.eyeDirection);
    data += "\t";
    data += canSpeak ? "1"sv : "0"sv;
    data += "\t";
    data += useSR ? "1"sv : "0"sv;
    data += "\t";
    data += useSR ? "1"sv : "0"sv;
    data += "\t";
    data += useDD ? "1"sv : "0"sv;
    data += "\t";
    data += vehicleID->get();
    data += "\t";
    data += std::to_string(terrainInterception);
    data += "\t";
    data += 1.f; //#TODO //_unit getVariable["tf_voiceVolume", 1.0]
    data += "\t";
    data += std::to_string(objectInterception);
    data += "\t";
    data += isSpectating ? "1"sv : "0"sv;
    data += "\t";
    data += isEnemy ? "1"sv : "0"sv;

    //private _data = [
    //    "POS	%1	%2	%3	%4	%5	%6	%7	%8	%9	%10	%11	%12	%13",
    //        _unitName,
    //        _pos, _curViewDir,//Position
    //        _can_speak, _useSw, _useLr, _useDd, _vehicle,
    //        _terrainInterception,
    //        _unit getVariable["tf_voiceVolume", 1.0],//Externally used API variable. Don't change name
    //        _object_interception, //Interceptions
    //        _isSpectating, _isEnemy
    //];

    Controller::get().networkHandler.doAsyncRequest(data);

    lastUpdateSent = std::chrono::system_clock::now();
}

PositionInfo PlayerInfo::getPosition() const {
    auto posFunc = positionFunc->get();
    if (!posFunc.is_nil()) {
        auto position = intercept::sqf::call(posFunc);
        auto positionArray = position.to_array(); //#TODO
        auto eyePos = positionArray[0].to_array();
        auto eyeDir = positionArray[1].to_array();
        PositionInfo info;

        info.eyePos = { eyePos[0], eyePos[1], eyePos[2] };
        info.eyeDirection = { eyeDir[0], eyeDir[1], eyeDir[2] };

        return info;
    }

    if (isSpectating.get()) {
        auto pctw = intercept::sqf::position_camera_to_world({ 0,0,0 });

        PositionInfo info;

        info.eyePos = intercept::sqf::atl_to_asl(pctw);
        info.eyeDirection = intercept::sqf::position_camera_to_world({ 0,0,1 }) - info.eyePos;
        return info;
    }

    PositionInfo info;
    auto selPos = intercept::sqf::selection_positon(controlledUnit, "pilot"sv);
    info.eyePos = intercept::sqf::model_to_world_visual_world(controlledUnit, selPos);
    info.eyeDirection = intercept::sqf::get_camera_view_direction(controlledUnit);

    return info;
}

r_string PlayerInfo::getVehicleID() const {
    object parent = unitParent.get();

    if (parent.is_null()) return "no"sv;

    auto netID = intercept::sqf::get_variable(parent, "TFAR_vehicleIDOverride"sv);
    if (netID.is_nil())
        netID = intercept::sqf::net_id(parent);

    auto vehicleType = intercept::sqf::type_of(parent);

    bool hasIntercom = static_cast<float>(CacheHelper::get().getVehicleConfigProperty(vehicleType, "TFAR_hasIntercom"sv, 0)) > 0;

    auto TFAR_fnc_isTurnedOut = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_isTurnedOut"sv);

    auto turnedOutData = intercept::sqf::call(TFAR_fnc_isTurnedOut, controlledUnit);

    bool turnedOut = turnedOutData[0];
    float isolation = turnedOutData[1];

    r_string ret;

    ret += static_cast<r_string>(netID);
    ret += "\x10"sv;
    if (turnedOut)
        ret += "turnout"sv;
    else
        ret += std::to_string(isolation);
    ret += "\x10"sv;
    if (hasIntercom) {
        r_string varName("TFAR_IntercomSlot_"sv);
        varName += static_cast<r_string>(netID);

        auto intercomSlot = intercept::sqf::get_variable(parent, varName);

        if (intercomSlot.is_nil()) {
            intercomSlot = intercept::sqf::get_variable(parent, "TFAR_defaultIntercomSlot"sv);

            if (intercomSlot.is_nil()) {
                intercomSlot = intercept::sqf::get_variable(intercept::sqf::mission_namespace(), "TFAR_defaultIntercomSlot"sv);
            }
        }
        ret += static_cast<r_string>(intercomSlot);
        ret += "\x10"sv;

    } else {
        ret += "-1\x10"sv;
    }

    auto velocity = intercept::sqf::velocity(controlledUnit);
    ret += static_cast<r_string>(static_cast<game_value>(velocity)); //hacky vector3 to string

    return ret;
}

bool PlayerInfo::getIsolatedAndInside() const {
    object parent = unitParent.get();

    if (parent.is_null()) return false;

    auto TFAR_fnc_isTurnedOut = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_isTurnedOut"sv);




    if (!isVehicleIsolated(parent)) return false;

    auto turnedOutData = intercept::sqf::call(TFAR_fnc_isTurnedOut, controlledUnit);
    bool turnedOut = turnedOutData[0];
    return !turnedOut;
}

bool PlayerInfo::isVehicleIsolated(const object& vehicle) {
    
    auto isolatedAmount = intercept::sqf::get_variable(vehicle, "TFAR_isolatedAmount"sv);

    if (!isolatedAmount.is_null())
        return static_cast<float>(isolatedAmount) > 0.5f;

    auto vehicleType = intercept::sqf::type_of(vehicle);

    float isolated = static_cast<float>(CacheHelper::get().getVehicleConfigProperty(vehicleType, "tf_isolatedAmount"sv, 0));


    intercept::sqf::set_variable(vehicle, "TFAR_isolatedAmount"sv, isolated);
    return isolated > 0.5f;
}
