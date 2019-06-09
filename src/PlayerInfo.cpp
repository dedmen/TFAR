#include "PlayerInfo.hpp"
#include <utility>
#include "CacheHelper.hpp"
#include "Controller.hpp"

static inline __itt_domain* PlayerInfoDomain = __itt_domain_create("PlayerInfo");

static inline __itt_string_handle* PlayerInfo_grabRadios = __itt_string_handle_create("grabRadios");
static inline __itt_string_handle* PlayerInfo_updateRadios = __itt_string_handle_create("updateRadios");


std::chrono::milliseconds timeInterp(float value, std::chrono::milliseconds minTime, std::chrono::milliseconds maxTime, float minValue, float maxValue) {
    if (value < minValue) return minTime;
    if (value > maxValue) return maxTime;

    return std::chrono::duration_cast<std::chrono::milliseconds>(minTime + (maxTime - minTime) * (value - minValue) / (maxValue - minValue));
}

std::chrono::milliseconds timeInterp(std::chrono::milliseconds value, std::chrono::milliseconds minTime, std::chrono::milliseconds maxTime, std::chrono::milliseconds minValue, std::chrono::milliseconds maxValue) {
    if (value < minValue) return minTime;
    if (value > maxValue) return maxTime;

    return std::chrono::milliseconds(minTime.count() + ((maxTime - minTime).count() * (value - minValue).count() / (maxValue - minValue).count()));
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
    scheduler(std::move(sched)),
    controlledUnit(unit),
    unit(unit),
    unitName(intercept::sqf::name(unit))
     {
    evt = __itt_event_create(unitName.c_str(), unitName.length());

}

void PlayerInfo::init() {
    auto player = weak_from_this();
    //If value doesn't change for a long time, you can reduce update frequency. Player standing still, sitting in same vehicle for long time
    positionFunc = makeCachedVal<game_value>(500ms, [player]() -> game_value {
        if (auto lockedPlayer = player.lock())
            return intercept::sqf::get_variable(lockedPlayer->controlledUnit, "TF_fnc_position"sv);
        return {};
    }, game_value{});

    positionFunc->setName(std::string("positionFunc ") + unitName);

    position = makeCachedVal<PositionInfo>(50ms, [player]()->PositionInfo {
        if (auto lockedPlayer = player.lock())
            return lockedPlayer->getPosition();
        return {};
    }, {});
    position->setName(std::string("position ") + unitName);
    isSpectating = makeCachedVal<bool>(500ms, [player]() -> bool {
        if (auto lockedPlayer = player.lock())
            return intercept::sqf::get_variable(lockedPlayer->controlledUnit, "TFAR_forceSpectator"sv);
        return false;
    }, false);
    isSpectating->setName(std::string("isSpectating ") + unitName);
    unitParent = makeCachedVal<object>(200ms, [player]()->object {
        if (auto lockedPlayer = player.lock())
            return intercept::sqf::object_parent(lockedPlayer->controlledUnit);
        return {};
    }, {});
    unitParent->setName(std::string("unitParent ") + unitName);
    vehicleID = makeCachedVal<r_string>(1000ms, [player]()->r_string {
        if (auto lockedPlayer = player.lock())
            return lockedPlayer->getVehicleID();
        return {};
        }, [player]()->bool {
            if (auto lockedPlayer = player.lock()) //Only update if we are in a vehicle
                return !lockedPlayer->unitParent->get().is_null();
            return false;
    }, {});
    vehicleID->setName(std::string("vehicleID ") + unitName);
    isolatedAndInside = makeCachedVal<bool>(200ms, [player]()->bool {
        if (auto lockedPlayer = player.lock())
            return lockedPlayer->getIsolatedAndInside();
        return {};
        }, [player]()->bool {
            if (auto lockedPlayer = player.lock()) //Only update if we are in a vehicle
                return !lockedPlayer->unitParent->get().is_null();
            return false;
    }, {});
    isolatedAndInside->setName(std::string("isolatedAndInside ") + unitName);



    terrainInterception = makeCachedVal<float>(2s, [player]()->float {
        if (auto lockedPlayer = player.lock()) {
            auto TFAR_fnc_calcTerrainInterception = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_calcTerrainInterception"sv);
            return intercept::sqf::call(TFAR_fnc_calcTerrainInterception, lockedPlayer->controlledUnit);
        }
        return {};
        }, {});
    terrainInterception->setName(std::string("terrainInterception ") + unitName);
    
	//#TODO only update when talking
    objectInterception = makeCachedVal<float>(100ms, [player]()->float {
        if (auto lockedPlayer = player.lock()) {
            auto TFAR_fnc_objectInterception = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_objectInterception"sv);

            return intercept::sqf::call(TFAR_fnc_objectInterception, lockedPlayer->controlledUnit);
        }
        return {};
    }, {});
    objectInterception->setName(std::string("objectInterception ") + unitName);

    radioUpdate = makeCachedVal<bool>(1s, [player]()->bool { //#TODO increase time if player doesn't have any
        if (auto lockedPlayer = player.lock()) {
            lockedPlayer->updateRadios();
        }
        return {};
    }, {});

    //Immediately update vehicleID and isolatedAndInside on vehicle changed
    unitParent->addUpdateEventhandler([vehicleID = vehicleID->getWeak(), isolatedAndInside = isolatedAndInside->getWeak(), player = weak_from_this()](const object& newParent) {
        if (auto lockedPlayer = player.lock()) {
            if (auto lockedVehicleID = vehicleID.lock())
                lockedVehicleID->manualUpdate(lockedPlayer->getVehicleID());
            if (auto lockedISO = isolatedAndInside.lock())
                lockedISO->manualUpdate(lockedPlayer->getIsolatedAndInside());
        }
    });

    updateSendDelay = 100ms;





    positionFunc->forceUpdate();
    position->forceUpdate();
    isSpectating->forceUpdate();
    unitParent->forceUpdate();
    vehicleID->forceUpdate();
    isolatedAndInside->forceUpdate();
    terrainInterception->forceUpdate();
    objectInterception->forceUpdate();
    radioUpdate->forceUpdate();
}

void PlayerInfo::simulate() {

    if (std::chrono::system_clock::now() - lastUpdateSent > updateSendDelay) {
        ittScopeEvt sc(evt);
        sendToTeamspeak();
        updateIntervals();
    }
}

void PlayerInfo::updateIntervals() {
    auto currentUnit = Controller::get().currentUnit;
    if (!currentUnit) return;

    auto distance = currentUnit->position->get().eyePos.distance(position->get().eyePos);
    auto now = std::chrono::system_clock::now();

#define LAST_CHANGE(OBJ) std::chrono::duration_cast<std::chrono::milliseconds>(now - OBJ->getLastChange())

    //#TODO move into cached value, and update interval on value update, or regularly
    updateSendDelay = timeInterp(distance, 50ms, 5s, 5, 5000);
    position->setInterval(timeInterp(distance, 50ms, 5s, 5, 5000));
    terrainInterception->setInterval(timeInterp(LAST_CHANGE(terrainInterception), 2s, 5s, 2s, 20s));
    objectInterception->setInterval(timeInterp(LAST_CHANGE(objectInterception), 100ms, 500ms, 1s, 5s));
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
            objectInterception = this->objectInterception->get();
        }

    } else {
        terrainInterception = this->terrainInterception->get();
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
    data += "1"; //#TODO //_unit getVariable["tf_voiceVolume", 1.0]
    data += "\t";
    data += std::to_string(objectInterception);
    data += "\t";
    data += isSpectating->get() ? "1"sv : "0"sv;
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

    //#TODO if data is same as last time, then only send every second

    std::string answ;
    Controller::get().networkHandler.doSyncRequest(data, answ);

    lastUpdateSent = std::chrono::system_clock::now();
}

void PlayerInfo::updateRadios() {
    ittScope sc(PlayerInfoDomain, PlayerInfo_updateRadios);
    MainthreadTester::checkNow();

    auto TFAR_fnc_lrRadiosList = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_lrRadiosList"sv);
    auto TFAR_fnc_radiosList = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_radiosList"sv);

    auto_array<game_value> srRadiosGV = sqf::call(TFAR_fnc_radiosList, controlledUnit).to_array();
    auto_array<game_value> lrRadiosGV = sqf::call(TFAR_fnc_lrRadiosList, controlledUnit).to_array();

    for (auto& it : radios)
        it->checkVar = false;


    for (r_string radio : srRadiosGV) {
        auto found = std::find_if(radios.begin(), radios.end(), [&radio](std::shared_ptr<RadioInfo>& rad) {
            return !rad->isLR && rad->variable == radio;
        });

        if (found == radios.end()) {
            auto& newRadio = radios.emplace_back(std::make_shared<RadioInfo>(scheduler, radio));
            newRadio->checkVar = true;
            newRadio->initValues();
        } else {
            (*found)->checkVar = true;
        }
    }

    for (const game_value& radioGV : lrRadiosGV) {
        object obj = radioGV[0];
        r_string variable = radioGV[1];

        auto found = std::find_if(radios.begin(), radios.end(), [&obj, &variable](std::shared_ptr<RadioInfo>& rad) {
            return rad->isLR && rad->obj == obj && rad->variable == variable;
            });

        if (found == radios.end()) {
            std::unique_lock lock(radiosLock);
            auto& newRadio = radios.emplace_back(std::make_shared<RadioInfo>(scheduler, obj, variable));
            newRadio->checkVar = true;
            newRadio->initValues();
        }
        else {
            (*found)->checkVar = true;
        }
    }

    auto newEnd = std::remove_if(radios.begin(), radios.end(), [](const std::shared_ptr<RadioInfo>& info) {
        return !info->checkVar;
    });

    if (newEnd != radios.end()) {
        std::unique_lock lock(radiosLock);
        radios.erase(newEnd, radios.end());
    }

}

void PlayerInfo::grabRadios(auto_array<r_string>& radioData) {
    ittScope sc(PlayerInfoDomain, PlayerInfo_grabRadios);
    radioUpdate->get(); //trigger update

    std::unique_lock lock(radiosLock);

    for (auto& it : radios) {
        if (!it->speakerEnabled->get()) continue;
        if (it->frequencies->get().empty()) continue;

        radioData.emplace_back(it->buildString(*this));
    }
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

    if (isSpectating->get()) {
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
    object parent = unitParent->get();

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
    object parent = unitParent->get();

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
