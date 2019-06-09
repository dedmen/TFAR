#include "RadioInfo.hpp"
#include <utility>
#include "CacheHelper.hpp"
#include "PlayerInfo.hpp"

RadioInfo::RadioInfo(std::shared_ptr<MainthreadScheduler> scheduler, object obj, r_string variable) 
    : scheduler(scheduler), isLR(true), obj(std::move(obj)), variable(std::move(variable))
    {}
RadioInfo::RadioInfo(std::shared_ptr<MainthreadScheduler> scheduler, r_string variable) 
    : scheduler(scheduler), isLR(false), variable(std::move(variable))
    {}

void RadioInfo::initValues() {
    
  
    auto radio = weak_from_this();
    if (isLR) {      
        speakerEnabled = makeCachedVal<bool>(100ms, [radio]()->bool {
            if (auto lockedRadio = radio.lock()) {
                auto TFAR_fnc_getLrSpeakers = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_getLrSpeakers"sv);

                return intercept::sqf::call(TFAR_fnc_getLrSpeakers, { lockedRadio->obj, lockedRadio->variable});
            }
            return {};
        }, {});

        radioCode = makeCachedVal<r_string>(2s, [radio]()->r_string {
            if (auto lockedRadio = radio.lock()) {
                auto TFAR_fnc_getLrRadioCode = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_getLrRadioCode"sv);

                return intercept::sqf::call(TFAR_fnc_getLrRadioCode, { lockedRadio->obj, lockedRadio->variable });
            }
            return {};
        }, {});

        frequencies = makeCachedVal<std::vector<r_string>>(2s, [radio]()->std::vector<r_string> {
            if (auto lockedRadio = radio.lock()) {
                auto TFAR_fnc_getLrFrequency = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_getLrFrequency"sv);
                auto TFAR_fnc_getAdditionalLrChannel = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_getAdditionalLrChannel"sv);
                auto TFAR_fnc_getChannelFrequency = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_getChannelFrequency"sv);

                std::vector<r_string> freqs;
                r_string mainFreq = intercept::sqf::call(TFAR_fnc_getLrFrequency, { lockedRadio->obj, lockedRadio->variable });
                mainFreq += lockedRadio->radioCode->get();
                freqs.emplace_back(std::move(mainFreq));

                float additionalChannel = intercept::sqf::call(TFAR_fnc_getAdditionalLrChannel, { lockedRadio->obj, lockedRadio->variable });
                if (additionalChannel > -1) {
                    r_string addFreq = intercept::sqf::call(TFAR_fnc_getChannelFrequency, { { lockedRadio->obj, lockedRadio->variable }, additionalChannel + 1.f });
                    addFreq += lockedRadio->radioCode->get();
                    freqs.emplace_back(std::move(addFreq));
                }

                return freqs;
            }
            return {};
        }, {});

        netID = makeCachedVal<r_string>(20s, [radio]()->r_string {
            if (auto lockedRadio = radio.lock()) {
                return intercept::sqf::net_id(lockedRadio->obj);
            }
            return {};
        }, intercept::sqf::net_id(obj));

        volume = makeCachedVal<float>(500ms, [radio]()->float {
            if (auto lockedRadio = radio.lock()) {
                auto TFAR_fnc_getLrVolume = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_getLrVolume"sv);

                return intercept::sqf::call(TFAR_fnc_getLrVolume, { lockedRadio->obj, lockedRadio->variable });
            }
            return {};
        }, {});

    } else {
        speakerEnabled = makeCachedVal<bool>(100ms, [radio]()->bool {
            if (auto lockedRadio = radio.lock()) {
                auto TFAR_fnc_getSwSpeakers = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_getSwSpeakers"sv);

                return intercept::sqf::call(TFAR_fnc_getSwSpeakers, lockedRadio->variable);
            }
            return {};
            }, {});

        radioCode = makeCachedVal<r_string>(2s, [radio]()->r_string {
            if (auto lockedRadio = radio.lock()) {
                auto TFAR_fnc_getSwRadioCode = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_getSwRadioCode"sv);

                return intercept::sqf::call(TFAR_fnc_getSwRadioCode, lockedRadio->variable );
            }
            return {};
            }, {});

        frequencies = makeCachedVal<std::vector<r_string>>(2s, [radio]()->std::vector<r_string> {
            if (auto lockedRadio = radio.lock()) {
                auto TFAR_fnc_getSwFrequency = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_getSwFrequency"sv);
                auto TFAR_fnc_getAdditionalSwChannel = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_getAdditionalSwChannel"sv);
                auto TFAR_fnc_getChannelFrequency = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_getChannelFrequency"sv);

                std::vector<r_string> freqs;
                r_string mainFreq = intercept::sqf::call(TFAR_fnc_getSwFrequency, lockedRadio->variable);
                mainFreq += lockedRadio->radioCode->get();
                freqs.emplace_back(std::move(mainFreq));

                float additionalChannel = intercept::sqf::call(TFAR_fnc_getAdditionalSwChannel, lockedRadio->variable );
                if (additionalChannel > -1) {
                    r_string addFreq = intercept::sqf::call(TFAR_fnc_getChannelFrequency, { lockedRadio->variable , additionalChannel + 1.f });
                    addFreq += lockedRadio->radioCode->get();
                    freqs.emplace_back(std::move(addFreq));
                }

                return freqs;
            }
            return {};
            }, {});

        netID = makeCachedVal<r_string>(30min, [radio]()->r_string {
            if (auto lockedRadio = radio.lock()) {
                return lockedRadio->netID->get(); //blergh
            }
            return {};
        }, variable);

        volume = makeCachedVal<float>(500ms, [radio]()->float {
            if (auto lockedRadio = radio.lock()) {
                auto TFAR_fnc_getSwVolume = CacheHelper::get().getMissionNamespaceVariable("TFAR_fnc_getSwVolume"sv);

                return intercept::sqf::call(TFAR_fnc_getSwVolume, lockedRadio->variable);
            }
            return {};
        }, {});
    }



    speakerEnabled->forceUpdate();
    radioCode->forceUpdate();
    frequencies->forceUpdate();
    netID->forceUpdate();
    volume->forceUpdate();







}

std::string RadioInfo::buildString(const PlayerInfo& player) const {
    std::string ret;
    ret.reserve(128);

    ret += netID->get();
    ret += "\n";

    std::string freqStr; //#TODO reserve
    for (auto& it : frequencies->get()) {
        freqStr += it;
        freqStr += "|";
    }
    freqStr.pop_back();

    ret += freqStr;
    ret += "\n";
    ret += player.unitName;
    ret += "\n[]\n"; //Position
    ret += std::to_string(volume->get());
    ret += "\n";

    ret += player.vehicleID->get();
    ret += "\n";
    ret += std::to_string(player.position->get().eyePos.z);

    return ret;
}
