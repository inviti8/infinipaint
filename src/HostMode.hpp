#pragma once

// Two explicit hosting modes (DISTRIBUTION-PHASE0.md §C):
//   COLLAB       — anyone with the lobby address joins with full write
//                  access; no token check.
//   SUBSCRIPTION — every joiner must present a portal-issued (or dev-
//                  minted) subscriber token; valid joiners are flagged
//                  isViewer = true and gated on the wire.
// Declared at namespace scope so both World.hpp and the UI headers
// (Toolbar.hpp, PhoneDrawingProgramScreen.hpp) can reference it without
// pulling in World.hpp transitively.
enum class HostMode { COLLAB, SUBSCRIPTION };
