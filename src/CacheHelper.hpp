#pragma once
#include <intercept.hpp>
#include "../intercept/src/host/common/singleton.hpp"

class pairHasher {
public:
    std::size_t operator()(const std::pair<r_string, r_string>& arg) const {
        return intercept::types::__internal::pairhash(arg.first, arg.second);
    }

};


class CacheHelper : public intercept::singleton<CacheHelper> {
public:


    game_value getVehicleConfigProperty(const r_string& classname, const r_string& property, game_value defaultValue);
    game_value getMissionNamespaceVariable(const r_string& varName);

private:

    std::unordered_map<std::pair<r_string, r_string>, game_value, pairHasher> vehicleCache;
    std::unordered_map<r_string, game_value> missionNamespaceVarCache;

};
