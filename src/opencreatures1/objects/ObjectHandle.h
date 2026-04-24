#pragma once

#include "common/SlotMap.h"

#include <memory>
#include <cstdint>

class Object;

using ObjectHandle = DenseSlotMap<std::unique_ptr<Object>>::Key;
