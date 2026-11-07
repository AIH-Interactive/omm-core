#pragma once

#include <memory>

#include "abi/runtime.hpp"
#include "ysmparser/asset.hpp"

namespace omm::ysm {

// Internal retained view: the alias keeps the parsed asset alive across external release.
// A model may only consume an asset from the same runtime.
std::shared_ptr<const Asset> retain_asset(omm_handle handle, const abi::Runtime *runtime);

} // namespace omm::ysm
