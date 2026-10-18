#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "config_store.hpp"

class ProvisioningPortal {
 public:
  bool run(ConfigStore& store, DeviceConfig& config);
};

#endif
