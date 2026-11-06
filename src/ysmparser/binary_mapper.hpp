// binary::Model → resource set + manifest + baked geometry.
// Port of OMP YsmBinaryResourceMapper / YsmBinaryMetadataMapper / YsmBinaryAnimationMapper
// with one deliberate difference: images keep their encoded bytes (".encoded") instead of
// being transcoded to PNG; the host decodes them using the media type in the resource index.
#pragma once

#include <string>
#include <unordered_map>

#include "ysmparser/asset.hpp"
#include "ysmparser/binary_model.hpp"

namespace omm::ysm {

struct BinaryMapping {
    ResourceSet resources;
    Manifest manifest;
    std::vector<Geometry> geometries;
    std::string metadata_json;
    // Raw RGBA image dimensions keyed by resource path (encoding -1).
    std::unordered_map<std::string, std::pair<int32_t, int32_t>> raw_images;
};

BinaryMapping map_binary(const binary::Model &model, const std::string &display_name, Budget &budget);

} // namespace omm::ysm
