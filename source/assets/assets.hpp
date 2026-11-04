#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL_opengl.h>

#include "core/binary_reader.hpp"
#include "core/container.hpp"
#include "core/info_archive.hpp"
#include "core/texture.hpp"

namespace fs = std::filesystem;

// FFON font loader (see research/FFON/5-24-2_FFON_format.md)
struct s_ffon_font
{
    std::uint8_t width = 24;
    std::uint8_t height = 24;
    std::uint8_t bytes_per_row = 12;
    std::uint32_t glyph_count = 0;
    std::uint32_t bytes_per_glyph = 0;
    std::vector<std::uint16_t> char_map;     // char_code -> glyph_id (0 = none)
    std::vector<std::vector<std::uint8_t>> glyphs; // glyph_id -> raw 4bpp pixels
};

[[nodiscard]] bool load_ffon(const std::vector<std::uint8_t>& data, s_ffon_font& font);

// Horizontal ink bounds [left, right) of a glyph in pixels; (0,0) if blank.
[[nodiscard]] std::pair<int, int> glyph_ink_bounds(const s_ffon_font& font,
    const std::vector<std::uint8_t>& data);

// Upload an s_texture_image to OpenGL and return the texture name.
[[nodiscard]] GLuint upload_texture(const sdbz::s_texture_image& image);

// Load every XPIX texture chunk from an FPIX container into OpenGL textures,
// returning texture names indexed by texture slot.
[[nodiscard]] std::vector<GLuint> load_fpix_textures(const std::vector<std::uint8_t>& data);
[[nodiscard]] std::vector<GLuint> load_fpix_textures(const fs::path& path);

// ---- Asset discovery: given a model, find its texture, motions and the game font ----

std::string lower_copy(std::string text);
bool has_extension(const fs::path& path, std::string_view extension);
fs::path directory_of(const fs::path& path);

// Regular files directly inside `dir` with the given (lowercase) extension, sorted by name.
std::vector<fs::path> list_files(const fs::path& dir, std::string_view extension);

// "p01a" -> "p01": drop trailing variant letters that follow a digit.
std::string character_prefix(const std::string& stem);

// p01a.mdl -> p01a.pix beside it (case-insensitive), else any .pix sharing the character prefix.
[[nodiscard]] fs::path find_texture_for(const fs::path& model_path);

// Motion packages (.pmt) beside the model: those sharing the character prefix
// (p01a.mdl -> p01sel.pmt, p01act.pmt), or every .pmt in the folder if none match.
[[nodiscard]] std::vector<fs::path> find_motions_for(const fs::path& model_path);

// Archive equivalents of the disk companion lookups above. Returned pointers
// refer to entries owned by `archive` and remain valid while it is unchanged.
[[nodiscard]] const sdbz::s_archive_entry* find_archive_texture_for(
    const sdbz::s_archive& archive, std::string_view model_name);
[[nodiscard]] std::vector<const sdbz::s_archive_entry*> find_archive_motions_for(
    const sdbz::s_archive& archive, std::string_view model_name);

// Look for a .fon near the model: its folder and a few parents, a few levels deep each.
[[nodiscard]] fs::path find_font_near(const fs::path& start);
