#include "Controller.hpp"
#include <unordered_set>
#include <future>

static inline __itt_domain* ControllerDomain = __itt_domain_create("Controller");

static inline __itt_string_handle* Controller_processPlayerPositions = __itt_string_handle_create("processPlayerPositions");
static inline __itt_string_handle* Controller_updatePlayerlist = __itt_string_handle_create("updatePlayerlist");
static inline __itt_string_handle* Controller_sendSpeakers = __itt_string_handle_create("sendSpeakers");

Controller::Controller() : playerUpdateScheduler(std::make_shared<MainthreadScheduler>()) {
    
    objectInterceptionEnabled = std::make_shared<CachedValueMT<bool>>(playerUpdateScheduler, 2s, []() -> bool {
        return intercept::sqf::get_variable(sqf::mission_namespace(), "TFAR_objectInterceptionEnabled"sv);
    }, true);

    speakerDistance = std::make_shared<CachedValueMT<float>>(playerUpdateScheduler, 2s, []() -> float {
        return intercept::sqf::get_variable(sqf::mission_namespace(), "TF_speakerDistance"sv);
    }, true);

    objectInterceptionEnabled->forceUpdate();
    speakerDistance->forceUpdate();
}

void Controller::preStart() {
    auto found = intercept::client::host::request_plugin_interface("CBA_NativeFunction"sv, 1);
    if (!found) return; //Fail
    CBAIface = static_cast<NativeFunctionPluginInterface*>(*found);


    CBAIface->registerNativeFunction("TFAR_fnc_processPlayerPositions"sv, [this](game_value_parameter) -> game_value {
        processPlayerPositions();
        return {};
        });

    workerThread = std::make_unique<std::thread>([this]() {
        threadWork();
    });


}

void Controller::preInit() {
    


}

void Controller::processPlayerPositions() {
    __itt_frame_begin_v3(ControllerDomain, NULL);

    if (intercept::sqf::get_client_state_number() != 10) {
        players.clear();
        currentUnit.reset();
        return;
    }

    ittScope sc(ControllerDomain, Controller_processPlayerPositions);
    auto currentTime = std::chrono::system_clock::now();

    if (currentTime - lastPlayerlistUpdate > 1s)
        updatePlayerlist();

    auto curUnit = sqf::get_variable(sqf::mission_namespace(), "TFAR_currentUnit"sv);

    if (!currentUnit || curUnit != currentUnit->unit) {
        auto found = std::find_if(players.begin(), players.end(), [&curUnit](const std::shared_ptr<PlayerInfo>& inf) {
            return inf->unit == curUnit;
        });

        if (found == players.end())
            __debugbreak();
        else
            (*found)->isCurrentUnit = true;

        if (currentUnit)
            currentUnit->isCurrentUnit = false;
        currentUnit = *found;
    }


    playerUpdateScheduler->executeTasks();
    __itt_frame_end_v3(ControllerDomain, NULL);
}

void Controller::updatePlayerlist() {
    ittScope sc(ControllerDomain, Controller_updatePlayerlist);
    //In mainthread

    //Get near players
    //object currentUnit = sqf::get_variable(sqf::mission_namespace(), "TFAR_currentUnit"sv);

    //std::unordered_set<object> nearUnits;
    //
    //auto unitsInPlayersGroup = sqf::units(currentUnit);
    //auto range = static_cast<float>(sqf::get_variable(sqf::mission_namespace(), "TF_max_voice_volume"sv))+40;//+40 because he won't be updated fast enough when coming into region
    //
    //auto nearbyUnits = sqf::near_entities(currentUnit, "Man"sv, range);
    //
    //nearUnits.insert(nearbyUnits.begin(), nearbyUnits.end());
    //
    //if (unitsInPlayersGroup.size() < 10)
    //    nearUnits.insert(unitsInPlayersGroup.begin(), unitsInPlayersGroup.end());
    //
    //nearUnits.insert(currentUnit);
    //
    //std::vector<r_string> vehicleFilter; //#TODO cache in Controller
    //vehicleFilter.emplace_back("LandVehicle"sv);
    //vehicleFilter.emplace_back("Air"sv);
    //vehicleFilter.emplace_back("Ship"sv);
    //
    //auto nearVehicles = sqf::near_entities(currentUnit, vehicleFilter, range);
    //
    //for (auto& it : nearVehicles) {
    //    auto crew = sqf::crew(it);
    //    nearUnits.insert(crew.begin(), crew.end());
    //}

    auto currentPlayers = sqf::all_players();
    //auto currentPlayers = sqf::all_units();

    

    for (auto& it : currentPlayers) {
        
        auto found = std::find_if(players.begin(), players.end(), [it](const std::shared_ptr<PlayerInfo>& info) {
            return info->unit == it;
        });

        if (found == players.end()) {
            std::unique_lock lock(playersLock);
            auto& newPlayer = players.emplace_back(std::make_shared<PlayerInfo>(playerUpdateScheduler, it));
            newPlayer->init();
        }
    }

    //Remove null players
    auto newEnd = std::remove_if(players.begin(), players.end(), [](const std::shared_ptr<PlayerInfo>& info) {
        return info->unit.is_null();
        });
    if (newEnd != players.end()) {
        std::unique_lock lock(playersLock);
        players.erase(newEnd, players.end());
    }
    

}

void Controller::threadWork() {
    std::chrono::system_clock::time_point lastSpeakerUpdate;
    std::string lastSpeakerInfo;

    while (true) {
        if (players.empty()) {
            std::this_thread::sleep_for(1s);
            continue;
        }

        std::unique_lock<std::mutex> lock(playersLock);

        for (auto& it : players) {
            it->simulate();
        }

        if (std::chrono::system_clock::now() - lastSpeakerUpdate > 200ms) {
            ittScope sc(ControllerDomain, Controller_sendSpeakers);
            auto_array<r_string> radioData; //#TODO use std::string and vector

            for (auto& it : players)
                it->grabRadios(radioData);

            //#TODO add ground radios from cached value in controller

            std::string data;
            data += "SPEAKERS\t";
            for (auto& it : radioData) {
                data += it;
                data += "\xB";
            }
            if (!radioData.empty())
                data.pop_back();
            data += "~"; //async command

            if (data != lastSpeakerInfo) //Don't bother teamspeak if nothing changed
                networkHandler.doAsyncRequest(data);
            lastSpeakerInfo = std::move(data);
        }


        std::this_thread::sleep_for(1ms);

    }

}
