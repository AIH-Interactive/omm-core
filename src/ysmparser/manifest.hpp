// Port of OMP YsmManifestReader for raw resource sets (directories, ZIP, legacy YSGP).
#pragma once

#include <string>

#include "ysmparser/asset.hpp"
#include "ysmparser/json.hpp"

namespace omm::ysm {

// Reads ysm.json (current layout) or main.json with minecraft:geometry (legacy layout).
// Throws Failure(format) "UNRECOGNIZED_SOURCE" when neither is present.
// out_root_json receives the compact JSON of the manifest root used for the metadata section.
Manifest read_manifest(const ResourceSet &resources, const std::string &display_name, std::string &out_root_json);

// File name without directories / without extension (OMP displayFromPath / idFromPath).
std::string display_from_path(const std::string &path);
std::string id_from_path(const std::string &path);

// Parses a resource as lenient JSON text; nullopt (plus diagnostic) when the text is not JSON.
std::optional<json::Document> parse_resource_json(const ResourceSet &resources, const std::string &path,
                                                  std::vector<std::string> &diagnostics);

} // namespace omm::ysm
