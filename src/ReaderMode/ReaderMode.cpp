#include "ReaderMode.hpp"
#include "../World.hpp"
#include "../MainProgram.hpp"
#include "../FontData.hpp"
#include "../Waypoints/Waypoint.hpp"
#include "../Waypoints/Edge.hpp"
#include "../Waypoints/WaypointGraph.hpp"
#include "../GUIStuff/GUIManager.hpp"
#include "../GUIStuff/Elements/Element.hpp"
#include "../GUIStuff/Elements/LayoutElement.hpp"
#include "../GUIStuff/ElementHelpers/ButtonHelpers.hpp"
#include "Helpers/ConvertVec.hpp"
#include <Helpers/NetworkingObjects/NetObjTemporaryPtr.decl.hpp>

#include <include/core/SkCanvas.h>
#include <include/core/SkFont.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRRect.h>
#include <include/core/SkTypeface.h>

#include <algorithm>

ReaderMode::ReaderMode(World& w) : world(w) {}

void ReaderMode::toggle() {
    set_active(!active);
}

void ReaderMode::set_active(bool a) {
    if (active == a) return;
    active = a;
    if (active) {
        // Pick a starting waypoint: existing selection if any, else the
        // first node in the graph. Reader mode with zero waypoints is a
        // valid (if empty) state — currentId stays nullopt.
        std::optional<NetworkingObjects::NetObjID> startId;
        if (world.wpGraph.has_selection())
            startId = world.wpGraph.get_selected();
        else if (world.wpGraph.get_nodes() && !world.wpGraph.get_nodes()->empty())
            startId = world.wpGraph.get_nodes()->begin()->obj.get_net_id();
        currentId = startId;
        history.clear();
        if (currentId.has_value())
            snap_camera_to_current();
    }
    world.set_to_layout_gui_if_focus();
}

void ReaderMode::set_current(NetworkingObjects::NetObjID id) {
    currentId = id;
    if (active) snap_camera_to_current();
}

void ReaderMode::forward() {
    if (!active || !currentId.has_value()) return;
    auto& edges = world.wpGraph.get_edges();
    if (!edges) return;
    // First outgoing edge wins (per-edge ordering inside the list is
    // stable; it's the order edges were created in). M7-c branch UI
    // is the proper destination picker; this stays as the keyboard
    // arrow's "advance" semantics.
    for (auto& info : *edges) {
        if (info.obj->get_from() != currentId.value()) continue;
        history.push_back(currentId.value());
        currentId = info.obj->get_to();
        snap_camera_to_current();
        world.set_to_layout_gui_if_focus();
        return;
    }
    // No outgoing edge — dead end. M7-d will surface a "the end"
    // affordance here.
}

void ReaderMode::back() {
    if (!active || history.empty()) return;
    currentId = history.back();
    history.pop_back();
    snap_camera_to_current();
    world.set_to_layout_gui_if_focus();
}

std::vector<std::pair<NetworkingObjects::NetObjID, std::optional<std::string>>>
ReaderMode::outgoing_choices() const {
    std::vector<std::pair<NetworkingObjects::NetObjID, std::optional<std::string>>> out;
    if (!currentId.has_value()) return out;
    auto& edges = world.wpGraph.get_edges();
    if (!edges) return out;
    for (auto& info : *edges) {
        if (info.obj->get_from() != currentId.value()) continue;
        out.emplace_back(info.obj->get_to(), info.obj->get_label());
    }
    return out;
}

bool ReaderMode::is_branch_point() const {
    return outgoing_choices().size() >= 2;
}

void ReaderMode::navigate_to(NetworkingObjects::NetObjID id) {
    if (!active) return;
    if (currentId.has_value()) history.push_back(currentId.value());
    currentId = id;
    snap_camera_to_current();
    world.set_to_layout_gui_if_focus();
}

void ReaderMode::snap_camera_to_current() {
    if (!currentId.has_value()) return;
    auto wpRef = world.netObjMan.get_obj_temporary_ref_from_id<Waypoint>(currentId.value());
    if (!wpRef) return;
    // PHASE2 M4 + M5: per-waypoint speed multiplier scales the transition
    // duration; per-waypoint easing preset overrides the global curve.
    // Both apply to the camera move INTO this waypoint.
    world.drawData.cam.smooth_move_to(world, wpRef->get_coords(),
                                      wpRef->get_window_size().cast<float>(),
                                      /*instantJump=*/false,
                                      wpRef->get_transition_speed_multiplier(),
                                      transition_easing_to_bezier_curve(wpRef->get_transition_easing()));
}

namespace {

constexpr float BRANCH_BUTTON_SIDE = 140.0f;  // square buttons
constexpr float BRANCH_OVERLAY_PADDING = 12.0f;

// One choice in the reader-mode branch overlay. Renders the target
// waypoint's skin when set, falls back to a labeled rounded-rect.
// Click navigates the reader to the target.
class BranchChoiceElement : public GUIStuff::Element {
    public:
        explicit BranchChoiceElement(GUIStuff::GUIManager& g) : Element(g) {}

        void layout(const Clay_ElementId& id, World* w,
                    NetworkingObjects::NetObjID target,
                    std::optional<std::string> edgeLabel) {
            world = w;
            targetId = target;
            label = std::move(edgeLabel);
            CLAY(id, {
                .layout = {.sizing = {.width = CLAY_SIZING_FIXED(BRANCH_BUTTON_SIDE),
                                     .height = CLAY_SIZING_FIXED(BRANCH_BUTTON_SIDE)}},
                .custom = {this}
            }) {}
        }

        void clay_draw(SkCanvas* canvas, GUIStuff::UpdateInputData&, Clay_RenderCommand*, bool skiaAA) override {
            if (!boundingBox.has_value() || !world) return;
            const auto& bb = boundingBox.value();
            const SkRect rect = SkRect::MakeLTRB(bb.min.x(), bb.min.y(), bb.max.x(), bb.max.y());

            auto wpRef = world->netObjMan.get_obj_temporary_ref_from_id<Waypoint>(targetId);
            const bool hasSkin = wpRef && wpRef->has_skin();

            // Hover/held visual feedback — outline brightens.
            const bool active = mouseHovering;

            if (hasSkin) {
                sk_sp<SkImage> img = wpRef->get_skin();
                const float imgW = static_cast<float>(img->width());
                const float imgH = static_cast<float>(img->height());
                const float scale = std::min((rect.width() - 4.0f) / imgW, (rect.height() - 4.0f) / imgH);
                const float drawW = imgW * scale;
                const float drawH = imgH * scale;
                const float dx = rect.fLeft + (rect.width() - drawW) * 0.5f;
                const float dy = rect.fTop + (rect.height() - drawH) * 0.5f;
                SkPaint imgPaint;
                imgPaint.setAntiAlias(skiaAA);
                canvas->drawImageRect(img.get(),
                                      SkRect::MakeXYWH(dx, dy, drawW, drawH),
                                      SkSamplingOptions{SkFilterMode::kLinear}, &imgPaint);
                if (active) {
                    SkPaint sel;
                    sel.setAntiAlias(skiaAA);
                    sel.setStyle(SkPaint::kStroke_Style);
                    sel.setStrokeWidth(3.0f);
                    sel.setColor4f({0.95f, 0.85f, 0.45f, 1.0f});
                    canvas->drawRect(SkRect::MakeXYWH(dx, dy, drawW, drawH), sel);
                }
            } else {
                // No skin — draw a rounded rect with the choice label
                // (or target waypoint name) centered.
                SkRRect rr = SkRRect::MakeRectXY(rect, 8.0f, 8.0f);
                SkPaint fill;
                fill.setAntiAlias(skiaAA);
                fill.setColor4f(active
                    ? SkColor4f{0.32f, 0.27f, 0.18f, 1.0f}
                    : SkColor4f{0.20f, 0.20f, 0.24f, 1.0f});
                canvas->drawRRect(rr, fill);
                SkPaint outline;
                outline.setAntiAlias(skiaAA);
                outline.setStyle(SkPaint::kStroke_Style);
                outline.setStrokeWidth(active ? 2.5f : 1.5f);
                outline.setColor4f({0.88f, 0.69f, 0.25f, 1.0f});
                canvas->drawRRect(rr, outline);

                std::string text = label.value_or(std::string{});
                if (text.empty() && wpRef) text = wpRef->get_label();
                if (text.empty()) text = "(unnamed)";

                SkFont font;
                font.setSize(14.0f);
                if (world->main.fonts) {
                    auto it = world->main.fonts->map.find("Roboto");
                    if (it != world->main.fonts->map.end()) font.setTypeface(it->second);
                }
                SkPaint textPaint;
                textPaint.setAntiAlias(skiaAA);
                textPaint.setColor4f({0.95f, 0.95f, 0.95f, 1.0f});
                // Crude single-line center; M-skin will not be the only
                // place that wants better text layout, but for now this
                // ships the feature.
                canvas->drawSimpleText(text.data(), text.size(), SkTextEncoding::kUTF8,
                                       rect.fLeft + 12.0f, rect.fTop + rect.height() * 0.5f + 5.0f,
                                       font, textPaint);
            }
        }

        void input_mouse_button_callback(const InputManager::MouseButtonCallbackArgs& button) override {
            if (!world) return;
            if (button.button != InputManager::MouseButton::LEFT) return;
            if (!button.down) return;
            // mouseHovering doesn't update reliably for floating elements;
            // hit-test the bounding box directly.
            if (!boundingBox.has_value()) return;
            const auto& bb = boundingBox.value();
            if (button.pos.x() < bb.min.x() || button.pos.x() > bb.max.x() ||
                button.pos.y() < bb.min.y() || button.pos.y() > bb.max.y()) return;
            world->readerMode.navigate_to(targetId);
        }

    private:
        World* world = nullptr;
        NetworkingObjects::NetObjID targetId{};
        std::optional<std::string> label;
};

}  // namespace

void render_reader_branch_overlay(World& world, GUIStuff::GUIManager& gui) {
    if (!world.readerMode.is_active()) return;
    auto choices = world.readerMode.outgoing_choices();
    // Render whenever there's something to show — at least one
    // outgoing choice OR history to step back through. Dead-ends with
    // history still get a back button so the reader isn't stranded.
    // (M7-d will add a "the end" affordance for true dead-ends with
    // no history either.)
    if (choices.empty() && !world.readerMode.has_history()) return;
    using namespace GUIStuff;

    gui.element<LayoutElement>("reader branch overlay", [&] (LayoutElement*, const Clay_ElementId& lId) {
        CLAY(lId, {
            .layout = {
                .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
                .padding = CLAY_PADDING_ALL(static_cast<uint16_t>(BRANCH_OVERLAY_PADDING)),
                .childGap = static_cast<uint16_t>(BRANCH_OVERLAY_PADDING),
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                .layoutDirection = CLAY_LEFT_TO_RIGHT
            },
            // Transparent container — only the buttons themselves
            // visually layer over the canvas. Pinned to the screen's
            // bottom-center; small upward offset keeps the buttons
            // from kissing the screen edge.
            .floating = {
                .offset = { .y = -BRANCH_OVERLAY_PADDING },
                .zIndex = static_cast<int16_t>(gui.get_z_index() + 10),
                .attachPoints = {
                    .element = CLAY_ATTACH_POINT_CENTER_BOTTOM,
                    .parent  = CLAY_ATTACH_POINT_CENTER_BOTTOM
                },
                .attachTo = CLAY_ATTACH_TO_ROOT
            },
        }) {
            // Back button as the first item, only when there's history
            // to pop. Same square size as the choice buttons so the row
            // reads as a uniform strip.
            if (world.readerMode.has_history()) {
                GUIStuff::ElementHelpers::svg_icon_button(
                    gui, "reader back",
                    "data/icons/RemixIcon/arrow-left-s-line.svg", {
                    .drawType = GUIStuff::SelectableButton::DrawType::TRANSPARENT_ALL,
                    .size = BRANCH_BUTTON_SIDE,
                    .onClick = [&world] { world.readerMode.back(); }
                });
            }
            for (size_t i = 0; i < choices.size(); ++i) {
                gui.new_id(static_cast<int64_t>(i), [&] {
                    gui.element<BranchChoiceElement>("button", &world, choices[i].first, choices[i].second);
                });
            }
        }
    });
}
