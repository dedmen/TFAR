#include "Controller.hpp"

int intercept::api_version() { //This is required for the plugin to work.
    return INTERCEPT_SDK_API_VERSION;
}

void intercept::register_interfaces() {
    
}

void intercept::pre_start() {
    Controller::get().preStart();
}

void intercept::pre_init() {
    Controller::get().preInit();
}