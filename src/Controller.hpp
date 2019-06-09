#pragma once
#include <chrono>
#include <intercept.hpp>
#include "PlayerInfo.hpp"
#include "MainthreadScheduler.hpp"
#include "../intercept/src/host/common/singleton.hpp"
#include "SharedMemoryTransfer.hpp"

using namespace std::chrono_literals;
using namespace intercept;

//CBA
/// Native function interface. Name: CBA_NativeFunction Version: 1
class NativeFunctionPluginInterface {
public:
    using functionType = std::function<intercept::types::game_value(intercept::types::game_value_parameter)>;
    /*
        @brief registers new Native function
        @throws std::invalid_argument if Function with that name already exists
    */
    virtual void registerNativeFunction(std::string_view name, functionType func) noexcept(false) = 0;
};

class Controller : public intercept::singleton<Controller> {
public:
    Controller();

    void preStart();
    void preInit();

    void processPlayerPositions();

    void updatePlayerlist();

    void threadWork();



    CachedValueMTS<bool> objectInterceptionEnabled;


    std::mutex playersLock;

    std::shared_ptr<PlayerInfo> currentUnit;
    std::vector<std::shared_ptr<PlayerInfo>> players;
    std::chrono::system_clock::time_point lastPlayerlistUpdate;


    NativeFunctionPluginInterface* CBAIface;
    std::shared_ptr<MainthreadScheduler> playerUpdateScheduler;
    std::unique_ptr<std::thread> workerThread;
    SharedMemoryHandler networkHandler;

};