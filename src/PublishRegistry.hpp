#pragma once
#include <filesystem>
#include <optional>
#include <string>

// DISTRIBUTION-PHASE1.md §4 — tagged-file auto-hosting registry.
//
// Single-slot persistent record of "the canvas this Inkternity install
// auto-hosts in the background on launch." Cap-1 by structure (the JSON
// shape carries one entry, not a list) until a future NetLibrary refactor
// supports multiple simultaneous SUBSCRIPTION hosts (§4.3 + §8.5).
//
// File: <configPath>/inkternity_published.json
//   {
//     "version": 1,
//     "published": {                       <-- absent when nothing published
//       "path": "<absolute file path>",
//       "publishedAt": "2026-05-14T18:30:00Z"
//     }
//   }
//
// The path is stored as the absolute filesystem path string. If the file
// is later moved/renamed/deleted on disk, the registry entry becomes
// stale; callers must defensively check existence before acting.
class PublishRegistry {
    public:
        struct Entry {
            std::filesystem::path path;
            std::string publishedAt;  // ISO 8601 UTC, e.g. "2026-05-14T18:30:00Z"
        };

        // Read inkternity_published.json. Empty/absent file → nothing
        // published (published_path() returns nullopt). Parse failure
        // is treated as empty + logged.
        bool load(const std::filesystem::path& configPath);

        // Returns the published file path if one is set, else nullopt.
        // Does NOT verify the file still exists on disk — the caller
        // does that (so we can render "publishing was set on a now-
        // missing file" UX rather than silently dropping the entry).
        std::optional<Entry> published() const { return entry; }

        // Convenience: just the path, when the timestamp doesn't matter.
        std::optional<std::filesystem::path> published_path() const {
            if (entry) return entry->path;
            return std::nullopt;
        }

        // True if `path` is the currently-published file. Path comparison
        // uses lexically_normal() to be robust to slash-direction differences
        // on Windows.
        bool is_published(const std::filesystem::path& path) const;

        // Set this path as the published file. Replaces any previous entry.
        // Stamps publishedAt to now in UTC. Writes the file. Returns true
        // on success (false on write failure).
        bool set_published(const std::filesystem::path& path,
                            const std::filesystem::path& configPath);

        // Clear the published entry. Writes the file. Returns true on
        // success.
        bool clear(const std::filesystem::path& configPath);

    private:
        std::optional<Entry> entry;

        bool save(const std::filesystem::path& configPath);
};
