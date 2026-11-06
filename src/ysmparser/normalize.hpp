// JSON → normalized asset pieces. Each function mirrors one OMP parser:
//   parse_geometry     YsmGeometryParser.parse
//   map_geometry       YsmBinaryGeometryMapper.map
//   parse_animations   YsmAnimationParser.parse   (data half; expressions stay text)
//   parse_controllers  YsmAnimationControllerParser.parse
#pragma once

#include <string>

#include "ysmparser/asset.hpp"
#include "ysmparser/binary_model.hpp"
#include "ysmparser/json.hpp"

namespace omm::ysm {

// Throws Failure(format) where the reference throws (missing geometry, bad numbers).
Geometry parse_geometry(const json::Value &root, const std::string &path);

Geometry map_geometry(const binary::Geometry &raw, const std::string &path, Budget &budget);

// Never throws for content: like the reference, a failure keeps the clips parsed so far
// and records a diagnostic.
AnimationFile parse_animations(const json::Value &root, const std::string &path, bool merge_multiline,
                               uint32_t worker_count = 1);

ControllerFile parse_controllers(const json::Value &root, const std::string &path, bool merge_multiline);

} // namespace omm::ysm
