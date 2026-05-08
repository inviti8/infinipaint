<div align="center">
	<img alt="Inkternity Logo" src="logo.svg" width=150/>
	<h3>Comics on an infinite, infinitely zoomable canvas</h3>
	<p>
		<a href="docs/MANUAL.md">📕 Usage Manual</a> -
		<a href="docs/BUILDING.md">⚒️ Build Manual</a> -
		<a href="docs/design/PHASE1.md">🗺️ Phase 1 Design</a>
	</p>
	<p>
		<a href="https://opensource.org/license/mit"><img alt="MIT License" src="https://img.shields.io/badge/license-MIT-blue"/></a>
	</p>
</div>

## Inkternity

Inkternity is an infinite-canvas app for **producing and reading comics**. The canvas has no zoom-in or zoom-out limit, so a comic can range from a wide map of an entire story down to per-panel ink detail without crossing a tile boundary or losing context.

On top of that canvas Inkternity adds a directed *waypoint graph* that captures reading order: each waypoint is a named camera + framing snapshot, and edges between waypoints define the path a reader follows. Branching panels give multiple outgoing edges, which the reader navigates through skinnable per-waypoint nav buttons.

### A fork of InfiniPaint

Inkternity is a fork of [ErrorAtLine0/infinipaint](https://github.com/ErrorAtLine0/infinipaint) — the infinite-canvas drawing app it inherits everything else from. The canvas, the rendering pipeline, layers, collaboration, the file format, the existing tool set: all of it is InfiniPaint's work. Inkternity layers a comic-production workflow on top, and stays MIT just like upstream.

Files saved by Inkternity use the `.inkternity` extension; existing `.infpnt` files from InfiniPaint load read-only-on-disk, and the next save migrates them to `.inkternity` (the original `.infpnt` is left in place — no destructive auto-rename).

## What Inkternity adds on top of InfiniPaint

- **Waypoints** — droppable canvas markers that capture camera state, panel framing, and a position in a directed reading graph
- **Tree-view editor** — collapsible side panel for connecting waypoints into a reading order with optional branches; bidirectional sync with the canvas
- **Reader mode** — chrome-free presentation that follows the waypoint graph; arrow-key navigation; per-branch choice UI
- **Waypoint skins** — capture a rectangle of the canvas (`ButtonSelectTool`) as a waypoint's skin, used as the artwork for nav buttons in reader mode and as node visuals in the tree view
- **libmypaint ink/marker brushes** — curated brush set built on [libmypaint](https://github.com/mypaint/libmypaint) (technical pen, fine inker, brush pen, fine/broad markers, wet ink) with persistent tile data per layer

## Inherited from InfiniPaint

- Infinite canvas, infinite zoom (no zoom limit until memory)
- Online collaborative lobbies — text chat, see-each-other-draw, jump-to-player
- Graphics tablet support with pressure sensitivity
- Layers with blend modes
- Saveable color palettes; right-click quick menu (color swap, canvas rotate)
- Undo / redo
- PNG / JPG / WEBP / SVG export of canvas regions
- Transform (move, scale, rotate) selections (rectangle / lasso select)
- Embed images and animated GIFs on the canvas
- Hide UI with Tab; remappable keybinds; custom UI themes
- Square grids on the canvas as drawing guides
- Rich-text textboxes (bold, italics, underline, fonts, color, alignment, direction)
- Other tools: rectangle, ellipse, line, eye-dropper, edit cursor
- Copy/paste between canvases and tabs
- Drop arbitrary files onto the canvas

## Installation

Inkternity is in active Phase 1 development. Build from source via [BUILDING.md](docs/BUILDING.md). Pre-built installers will land with the Phase 1 release.

## Contribution

Issue reports (bugs and feature requests) welcome. For pull requests of any meaningful scope, please open an issue first to align — Phase 1 is moving fast and large parts of the code are still being shaped.

## License

Inkternity is MIT licensed, matching InfiniPaint upstream. Third-party components retain their respective licenses; see the `About` menu in-app for the full list.
