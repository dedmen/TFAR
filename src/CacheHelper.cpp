#include "CacheHelper.hpp"
#include "MainthreadTester.hpp"


game_value CacheHelper::getVehicleConfigProperty(const r_string& classname, const r_string& property, game_value defaultValue) {
    MainthreadTester::checkNow();
    auto found = vehicleCache.find({ classname, property });
    if (found != vehicleCache.end())
        return found->second;

    auto configEntry =intercept::sqf::config_entry() >> "CfgVehicles"sv >> classname >> property;

    if (intercept::sqf::is_number(configEntry)) {
        auto value = intercept::sqf::get_number(configEntry);
        vehicleCache.insert({ { classname, property }, value });
        return value;
    }

    if (intercept::sqf::is_text(configEntry)) {
        auto value = intercept::sqf::get_text(configEntry);
        vehicleCache.insert({ { classname, property }, value });
        return value;
    }

    if (intercept::sqf::is_array(configEntry)) {
        auto value = intercept::sqf::get_array(configEntry);
        vehicleCache.insert({ { classname, property }, value });
        return value;
    }
    vehicleCache.insert({ { classname, property }, defaultValue });

    return defaultValue;
}

game_value CacheHelper::getMissionNamespaceVariable(const r_string& varName) {
    MainthreadTester::checkNow();
    auto found = missionNamespaceVarCache.find(varName);
    if (found != missionNamespaceVarCache.end())
        return found->second;

    auto value = intercept::sqf::get_variable(intercept::sqf::mission_namespace(), varName);
    missionNamespaceVarCache.insert({ varName, value });
    return value;
}
