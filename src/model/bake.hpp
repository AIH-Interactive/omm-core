#pragma once

#include "model/model.hpp"
#include "ysmparser/asset.hpp"

namespace omm::model {

std::shared_ptr<Model> bake_asset(std::shared_ptr<abi::Runtime> runtime, const ysm::Asset &asset,
                                  const omm_bake_desc &desc);

} // namespace omm::model
