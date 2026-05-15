#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// DISTRIBUTION-PHASE1.md §4 — tagged-file auto-hosting.
//
// Per-canvas state lives next to the canvas file:
//
//   <name>.inkternity         the canvas itself
//   <name>.inkternity.jpg     thumbnail
//   <name>.inkternity.publish JSON marker — present iff "published"
//                             { "publishedAt": "2026-05-14T18:30:00Z" }
//   <name>.inkternity.lock    runtime PID file — present while some
//                             Inkternity instance is actively hosting
//                             this canvas. Format: { "pid": <int> }
//
// Why per-canvas, not per-install:
//
//   The original §4 design used a single inkternity_published.json
//   under configPath. That broke for the multi-instance scenario:
//   `configPath` is shared across all running Inkternity processes
//   on the machine, but each process can host at most one canvas
//   (NetLibrary is process-singleton on globalID + signaling ws).
//   A shared single-slot registry can't represent "instance A hosts
//   canvas X, instance B hosts canvas Y."
//
//   Per-canvas marker + per-canvas lock fixes both:
//
//   - The "published" intent is a property of the *canvas* (travels
//     with file moves, backups, copies). One artist publishes N
//     canvases; the markers live with them.
//   - The lock is the per-instance runtime claim. First instance to
//     launch picks the first published canvas it can lock; second
//     instance picks the next; etc. No central registry, no cap.
//   - Cap-of-1-per-process is structural (NetLibrary). Cap-of-N
//     globally falls out naturally as the artist launches N
//     instances.
//
// Lock semantics:
//
//   - Acquisition is atomic via O_CREAT|O_EXCL (POSIX) /
//     CREATE_NEW (Windows). First writer wins.
//   - Lock content is the holder PID. On scan, a lock whose PID is
//     no longer alive (process crashed, killed, or closed without
//     RAII release) is considered stale and reclaimable.
//   - Release happens on graceful shutdown (RAII) or stale-detection
//     by another instance on next scan.

namespace PublishedCanvases {

// ---- per-canvas "published" marker --------------------------------------

// Returns true if the marker file exists for this canvas (file presence
// is the sole authoritative signal; contents are advisory).
bool is_published(const std::filesystem::path& canvasPath);

// Create the marker (with current UTC publishedAt timestamp). Idempotent.
// Returns false on filesystem write error.
bool set_published(const std::filesystem::path& canvasPath);

// Remove the marker. Idempotent (returns true if marker already absent).
bool clear_published(const std::filesystem::path& canvasPath);

// ---- per-canvas runtime lock --------------------------------------------

// Try to acquire an exclusive lock on the canvas. Returns true iff this
// instance now owns the lock. Fails (returns false) if another live
// instance holds it. Stale locks (PID no longer alive) are silently
// reclaimed during the attempt.
bool try_acquire_lock(const std::filesystem::path& canvasPath);

// Release a lock owned by this instance. Idempotent. Safe to call even
// if we never held it (no-op).
void release_lock(const std::filesystem::path& canvasPath);

// True iff a live (non-stale) lock currently exists for this canvas.
// Used by UI to show "hosting (this instance)" vs "hosting (another
// instance)" vs "published, nobody hosting yet".
bool is_locked_by_anyone(const std::filesystem::path& canvasPath);

// True iff *this* process owns the lock. Trivial wrapper around an
// in-memory set of locks-we-hold; not based on file inspection.
bool is_locked_by_us(const std::filesystem::path& canvasPath);

// ---- scanning -----------------------------------------------------------

// Walk a directory, return all canvas files (.inkternity / legacy
// .infpnt) that have a publish marker. Order: filesystem natural.
std::vector<std::filesystem::path> scan_published(
    const std::filesystem::path& savesDir);

// Convenience: scan, then try to lock each one in order, return the
// first one we successfully locked (or nullopt if none could be locked).
// Used at app startup to claim "this instance's hosted canvas."
std::optional<std::filesystem::path> claim_first_available(
    const std::filesystem::path& savesDir);

// Release every lock this process currently holds. Call on graceful
// shutdown so the lock files don't linger as stale-by-PID.
void release_all_held();

}  // namespace PublishedCanvases
