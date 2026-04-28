#pragma once
#include <filesystem>
#include "CoordSpaceHelper.hpp"
#include <Helpers/SCollision.hpp>

struct WorldScreenshotInfo {
    enum class ScreenshotType : size_t {
        JPG,
        PNG,
        WEBP,
        SVG
    };
    std::filesystem::path filePath;
    ScreenshotType type;
    Vector2i imageSizePixels;
    CoordSpaceHelper cameraCoords;
    SCollision::AABB<float> imageBounds;
    bool transparentBackground;
    bool displayGrid;
};

NLOHMANN_JSON_SERIALIZE_ENUM(WorldScreenshotInfo::ScreenshotType, {
    {WorldScreenshotInfo::ScreenshotType::JPG, "jpg"},
    {WorldScreenshotInfo::ScreenshotType::PNG, "png"},
    {WorldScreenshotInfo::ScreenshotType::WEBP, "webp"},
    {WorldScreenshotInfo::ScreenshotType::SVG, "svg"}
})

void world_take_screenshot(const std::shared_ptr<World>& w, const WorldScreenshotInfo& info);
