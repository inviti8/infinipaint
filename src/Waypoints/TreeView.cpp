#include "TreeView.hpp"
#include "../World.hpp"
#include "../MainProgram.hpp"
#include "../FontData.hpp"
#include "../GUIStuff/GUIManager.hpp"
#include "../GUIStuff/Elements/Element.hpp"
#include "../GUIStuff/Elements/LayoutElement.hpp"
#include "../GUIStuff/ElementHelpers/TextLabelHelpers.hpp"
#include "Helpers/ConvertVec.hpp"
#include "Waypoint.hpp"
#include "WaypointGraph.hpp"
#include <Helpers/NetworkingObjects/NetObjTemporaryPtr.decl.hpp>

#include <include/core/SkCanvas.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkTextBlob.h>
#include <include/core/SkTypeface.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

// Custom Element that draws the WaypointGraph using Skia inside the
// tree-view panel. Uses the same ".custom = {this}" layout mechanism
// ColorRectangleDisplay uses, so it slots into Clay's layout naturally
// and gets a clay_draw call with the panel's bounding box.
//
// M6-b is read-only: render nodes (rounded rect + label) and edges
// (line with arrowhead). M6-c will add input callbacks for drag,
// edge creation, and double-click to focus.
class TreeViewGraphElement : public GUIStuff::Element {
    public:
        explicit TreeViewGraphElement(GUIStuff::GUIManager& g) : Element(g) {}

        void layout(const Clay_ElementId& id, World* w) {
            world = w;
            CLAY(id, {
                .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}},
                .custom = {this}
            }) {}
        }

        void clay_draw(SkCanvas* canvas, GUIStuff::UpdateInputData&, Clay_RenderCommand*, bool skiaAA) override {
            if (!boundingBox.has_value() || !world) return;
            const auto& bb = boundingBox.value();
            const Vector2f panelOrigin = bb.min;
            const Vector2f panelSize = bb.max - bb.min;

            canvas->save();
            canvas->clipRect(SkRect::MakeLTRB(bb.min.x(), bb.min.y(), bb.max.x(), bb.max.y()), skiaAA);

            // Subtle backing fill behind the graph area so nodes/edges
            // don't visually merge into the panel chrome.
            SkPaint bgPaint;
            bgPaint.setColor4f({0.10f, 0.10f, 0.12f, 1.0f});
            canvas->drawRect(SkRect::MakeLTRB(bb.min.x(), bb.min.y(), bb.max.x(), bb.max.y()), bgPaint);

            // Auto-place any node missing a layout entry. Single-column
            // stack inside the panel; positions get persisted into
            // WaypointGraph::layout so later frames are stable.
            auto& nodes = world->wpGraph.get_nodes();
            auto& layout = world->wpGraph.mutable_layout();
            if (!nodes) { canvas->restore(); return; }

            constexpr float NODE_W = 220.0f;
            constexpr float NODE_H = 40.0f;
            constexpr float NODE_PAD_Y = 14.0f;
            constexpr float STACK_TOP = 40.0f;
            const float stackX = (panelSize.x() - NODE_W) * 0.5f;

            int autoIndex = 0;
            std::unordered_map<NetworkingObjects::NetObjID, Vector2f> currentPositions;
            for (auto& info : *nodes) {
                const auto id = info.obj.get_net_id();
                auto it = layout.find(id);
                if (it == layout.end()) {
                    Vector2f autoPos(stackX, STACK_TOP + autoIndex * (NODE_H + NODE_PAD_Y));
                    layout.emplace(id, autoPos);
                    currentPositions.emplace(id, autoPos);
                } else {
                    currentPositions.emplace(id, it->second);
                }
                ++autoIndex;
            }

            // Render edges first so nodes draw on top.
            auto& edges = world->wpGraph.get_edges();
            if (edges) {
                SkPaint edgePaint;
                edgePaint.setAntiAlias(skiaAA);
                edgePaint.setStyle(SkPaint::kStroke_Style);
                edgePaint.setStrokeWidth(2.0f);
                edgePaint.setColor4f({0.88f, 0.69f, 0.25f, 0.80f});  // muted gold (matches canvas markers)
                for (auto& einfo : *edges) {
                    auto fromIt = currentPositions.find(einfo.obj->get_from());
                    auto toIt = currentPositions.find(einfo.obj->get_to());
                    if (fromIt == currentPositions.end() || toIt == currentPositions.end()) continue;
                    const Vector2f from = panelOrigin + fromIt->second + Vector2f(NODE_W * 0.5f, NODE_H);
                    const Vector2f to   = panelOrigin + toIt->second   + Vector2f(NODE_W * 0.5f, 0.0f);
                    canvas->drawLine(from.x(), from.y(), to.x(), to.y(), edgePaint);

                    // Tiny arrowhead at the destination end.
                    const Vector2f dir = (to - from).normalized();
                    const Vector2f perp(-dir.y(), dir.x());
                    constexpr float ARROW_LEN = 8.0f;
                    constexpr float ARROW_HALF = 5.0f;
                    const Vector2f a1 = to - dir * ARROW_LEN + perp * ARROW_HALF;
                    const Vector2f a2 = to - dir * ARROW_LEN - perp * ARROW_HALF;
                    SkPathBuilder pb;
                    pb.moveTo(a1.x(), a1.y());
                    pb.lineTo(to.x(), to.y());
                    pb.lineTo(a2.x(), a2.y());
                    canvas->drawPath(pb.detach(), edgePaint);
                }
            }

            // Render nodes. Each waypoint is a rounded rect with its label
            // (or "(unnamed)" if empty) centered inside. Pull Roboto from
            // the shared FontData — using a default-constructed SkFont
            // gets you an empty typeface and silently no glyphs.
            SkFont font;
            font.setSize(14.0f);
            if (world->main.fonts) {
                auto it = world->main.fonts->map.find("Roboto");
                if (it != world->main.fonts->map.end()) font.setTypeface(it->second);
            }

            SkPaint nodeFill;
            nodeFill.setAntiAlias(skiaAA);
            nodeFill.setColor4f({0.20f, 0.20f, 0.24f, 1.0f});
            SkPaint nodeOutline;
            nodeOutline.setAntiAlias(skiaAA);
            nodeOutline.setStyle(SkPaint::kStroke_Style);
            nodeOutline.setStrokeWidth(1.5f);
            nodeOutline.setColor4f({0.88f, 0.69f, 0.25f, 1.0f});
            SkPaint textPaint;
            textPaint.setAntiAlias(skiaAA);
            textPaint.setColor4f({0.95f, 0.95f, 0.95f, 1.0f});

            for (auto& info : *nodes) {
                const auto id = info.obj.get_net_id();
                auto it = currentPositions.find(id);
                if (it == currentPositions.end()) continue;
                const Vector2f topLeft = panelOrigin + it->second;
                SkRect r = SkRect::MakeXYWH(topLeft.x(), topLeft.y(), NODE_W, NODE_H);
                SkRRect rr = SkRRect::MakeRectXY(r, 6.0f, 6.0f);
                canvas->drawRRect(rr, nodeFill);
                canvas->drawRRect(rr, nodeOutline);

                const std::string& label = info.obj->get_label();
                const std::string display = label.empty() ? std::string("(unnamed)") : label;
                canvas->drawSimpleText(display.data(), display.size(), SkTextEncoding::kUTF8,
                                       topLeft.x() + 12.0f, topLeft.y() + NODE_H * 0.5f + 5.0f,
                                       font, textPaint);
            }

            canvas->restore();
        }

    private:
        World* world = nullptr;
};

TreeView::TreeView(World& w)
    : world(w) {}

void TreeView::gui(GUIStuff::GUIManager& gui) {
    if (!visible) return;
    using namespace GUIStuff;
    using namespace GUIStuff::ElementHelpers;
    auto& io = gui.io;

    // Panel is a fixed-width column on the right side of the canvas.
    // Width chosen empirically — wide enough to read short labels and
    // see a few nodes without overwhelming the canvas.
    constexpr float PANEL_WIDTH = 360.0f;

    gui.element<LayoutElement>("tree view panel", [&] (LayoutElement*, const Clay_ElementId& lId) {
        CLAY(lId, {
            .layout = {
                .sizing = {.width = CLAY_SIZING_FIXED(PANEL_WIDTH), .height = CLAY_SIZING_GROW(0)},
                .padding = CLAY_PADDING_ALL(static_cast<uint16_t>(io.theme->padding1)),
                .childGap = static_cast<uint16_t>(io.theme->childGap1),
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP},
                .layoutDirection = CLAY_TOP_TO_BOTTOM
            },
            .backgroundColor = convert_vec4<Clay_Color>(io.theme->backColor1),
            .cornerRadius = CLAY_CORNER_RADIUS(io.theme->windowCorners1)
        }) {
            text_label_centered(gui, "Tree");
            gui.element<TreeViewGraphElement>("graph", &world);
        }
    });
}
