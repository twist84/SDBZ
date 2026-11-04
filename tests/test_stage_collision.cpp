#include "core/stage_collision.hpp"
#include "gameplay/player_controller.hpp"

#include <cmath>
#include <iostream>
#include <utility>

namespace
{
    int g_failures = 0;

    void require(const bool condition, const char* message)
    {
        if (!condition)
        {
            ++g_failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    sdbz::s_stage make_stage()
    {
        sdbz::s_stage stage;
        sdbz::s_stage_node collision_node;
        collision_node.m_index = 0;
        collision_node.m_kind = 3;
        collision_node.m_name = "floor_collision";
        collision_node.m_translation = { 5.0f, 2.0f, -3.0f };
        collision_node.m_references.push_back({ 0, 0, "floor" });
        stage.m_nodes.push_back(collision_node);
        return stage;
    }

    sdbz::s_hit make_hit()
    {
        sdbz::s_hit hit;
        hit.m_names.push_back("floor");
        sdbz::s_hit_object object;
        object.m_name = "floor";
        object.m_polygon_count = 1;
        object.m_first_polygon = 0;
        hit.m_objects.push_back(object);
        sdbz::s_hit_polygon polygon;
        polygon.m_vertex_count = 4;
        polygon.m_first_vertex = 0;
        hit.m_polygons.push_back(polygon);
        hit.m_vertices = {
            { { 0.0f, 0.0f, 0.0f, 1.0f } },
            { { 4.0f, 0.0f, 0.0f, 1.0f } },
            { { 4.0f, 0.0f, 4.0f, 1.0f } },
            { { 0.0f, 0.0f, 4.0f, 1.0f } }
        };
        return hit;
    }
}

int main()
{
    const auto stage = make_stage();
    const auto hit = make_hit();
    const std::vector<sdbz::mat4> worlds = {
        sdbz::trs_matrix(stage.m_nodes[0].m_translation,
            stage.m_nodes[0].m_rotation_radians, stage.m_nodes[0].m_scale)
    };
    const auto collision = sdbz::build_stage_collision_world(stage, hit, worlds);
    require(collision.m_polygons.size() == 1,
        "kind-3 stage reference builds one world collision polygon");
    require(collision.m_polygons[0].m_object_name == "floor",
        "collision polygon retains referenced HIT object name");
    require(collision.m_spatial_index_ready && !collision.m_spatial_cells.empty(),
        "built collision world includes its query spatial index");
    require(std::abs(collision.m_polygons[0].m_bounds_min[1] - 2.0f) < 0.001f,
        "HIT vertices use the STG node world transform");

    const auto contact = sdbz::query_stage_ground(collision, 6.0f, -2.0f, 2.1f);
    require(contact.has_value(), "ground query finds a transformed floor");
    if (contact)
    {
        require(std::abs(contact->m_position[1] - 2.0f) < 0.001f,
            "ground query returns transformed floor height");
        require(contact->m_normal[1] > 0.99f,
            "ground normal is oriented upward independent of HIT winding");
    }
    require(!sdbz::query_stage_ground(collision, 20.0f, 20.0f, 2.0f),
        "ground query rejects points outside collision polygon");

    // Only HIT nodes under the STAGE root are playable. A floor stored under a
    // template or EFFECT root (kept near the origin, never placed in the arena)
    // must not enter the collision world or become the spawn surface.
    {
        sdbz::s_stage rooted;
        const auto add_node = [&](std::uint32_t index, std::uint8_t kind,
            std::int32_t parent, const char* name, bool with_ref)
        {
            sdbz::s_stage_node node;
            node.m_index = index;
            node.m_kind = kind;
            node.m_parent = parent;
            node.m_name = name;
            if (with_ref) node.m_references.push_back({ 0, 0, "floor" });
            rooted.m_nodes.push_back(node);
        };
        add_node(0, 0, -1, "template_root", false);
        add_node(1, 3, 0, "template_floor", true);
        add_node(2, 0, -1, "EFFECT", false);
        add_node(3, 3, 2, "effect_floor", true);
        add_node(4, 0, -1, "STAGE", false);
        add_node(5, 3, 4, "stage_floor", true);
        rooted.m_template_root_count = 1;
        rooted.m_roots = { 0, 2, 4 };
        rooted.m_effect_root = 2;
        rooted.m_stage_root = 4;
        std::vector<sdbz::mat4> rooted_worlds;
        for (std::size_t i = 0; i < rooted.m_nodes.size(); ++i)
        {
            const float y = (i == 5) ? 30.0f : 1.5f;
            rooted_worlds.push_back(sdbz::trs_matrix({ 0.0f, y, 0.0f },
                { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }));
        }
        const auto rooted_world = sdbz::build_stage_collision_world(
            rooted, hit, rooted_worlds);
        require(rooted_world.m_polygons.size() == 1,
            "template and EFFECT HIT nodes are excluded from the collision world");
        require(std::abs(rooted_world.m_stage_origin_y - 1.5f) < 0.001f,
            "collision world records the live STAGE root's world-space origin height");
        const auto rooted_spawn = sdbz::find_stage_player_spawn(rooted_world);
        require(rooted_spawn.has_value() &&
            std::abs(rooted_spawn->m_position[1] - 30.05f) < 0.01f,
            "player spawns on the STAGE floor, not a template/effect surface near the origin");
    }

    // Recompute collision in world space after rotation and non-uniform scale.
    // This catches the common error of transforming a normal like a position
    // or leaving local-space bounds attached to world-space vertices.
    sdbz::s_stage rotated_stage;
    sdbz::s_stage_node rotated_node;
    rotated_node.m_index = 0;
    rotated_node.m_kind = 3;
    rotated_node.m_name = "rotated_floor_collision";
    rotated_node.m_translation = { 10.0f, 1.0f, 20.0f };
    rotated_node.m_rotation_radians = {
        0.0f, 1.57079632679f, 0.0f
    };
    rotated_node.m_scale = { 2.0f, 1.0f, 0.5f };
    rotated_node.m_references.push_back({ 0, 0, "floor" });
    rotated_stage.m_nodes.push_back(rotated_node);
    const std::vector<sdbz::mat4> rotated_worlds = {
        sdbz::trs_matrix(rotated_node.m_translation,
            rotated_node.m_rotation_radians, rotated_node.m_scale)
    };
    const auto rotated_collision = sdbz::build_stage_collision_world(
        rotated_stage, hit, rotated_worlds);
    require(rotated_collision.m_polygons.size() == 1,
        "rotated and scaled stage node builds a collision polygon");
    if (!rotated_collision.m_polygons.empty())
    {
        const auto& rotated_polygon = rotated_collision.m_polygons.front();
        require(std::abs(rotated_polygon.m_vertices[0][0] - 10.0f) < 0.001f &&
            std::abs(rotated_polygon.m_vertices[0][1] - 1.0f) < 0.001f &&
            std::abs(rotated_polygon.m_vertices[0][2] - 20.0f) < 0.001f,
            "first HIT vertex preserves vertex order and receives the world transform");
        require(std::abs(rotated_polygon.m_vertices[1][0] - 10.0f) < 0.001f &&
            std::abs(rotated_polygon.m_vertices[1][2] - 12.0f) < 0.001f,
            "rotated and non-uniformly scaled HIT edge is transformed correctly");
        require(std::abs(rotated_polygon.m_bounds_min[0] - 10.0f) < 0.001f &&
            std::abs(rotated_polygon.m_bounds_max[0] - 12.0f) < 0.001f &&
            std::abs(rotated_polygon.m_bounds_min[2] - 12.0f) < 0.001f &&
            std::abs(rotated_polygon.m_bounds_max[2] - 20.0f) < 0.001f,
            "transformed collision AABB is recomputed from world vertices");
        require(rotated_polygon.m_normal[1] > 0.99f,
            "world polygon normal is recomputed after rotation and scale");
        require(std::abs(rotated_polygon.m_projected_area - 16.0f) < 0.01f,
            "projected area reflects the world-space non-uniform scale");
    }
    const auto rotated_contact = sdbz::query_stage_ground(
        rotated_collision, 11.0f, 16.0f, 1.0f);
    require(rotated_contact && std::abs(rotated_contact->m_position[1] - 1.0f) < 0.001f,
        "ground query succeeds on a rotated and scaled HIT polygon");

    sdbz::s_stage tilted_stage;
    sdbz::s_stage_node tilted_node;
    tilted_node.m_index = 0;
    tilted_node.m_kind = 3;
    tilted_node.m_translation = { 0.0f, 3.0f, 0.0f };
    tilted_node.m_rotation_radians = {
        0.52359877559f, 0.0f, 0.0f
    };
    tilted_node.m_scale = { 1.0f, 2.0f, 0.5f };
    tilted_node.m_references.push_back({ 0, 0, "floor" });
    tilted_stage.m_nodes.push_back(tilted_node);
    const std::vector<sdbz::mat4> tilted_worlds = {
        sdbz::trs_matrix(tilted_node.m_translation,
            tilted_node.m_rotation_radians, tilted_node.m_scale)
    };
    const auto tilted_collision = sdbz::build_stage_collision_world(
        tilted_stage, hit, tilted_worlds);
    require(tilted_collision.m_polygons.size() == 1,
        "tilted stage node builds a walkable collision polygon");
    if (!tilted_collision.m_polygons.empty())
    {
        const auto& tilted_normal = tilted_collision.m_polygons.front().m_normal;
        require(std::abs(tilted_normal[0]) < 0.001f &&
            std::abs(tilted_normal[1] - 0.8660254f) < 0.002f &&
            std::abs(tilted_normal[2] - 0.5f) < 0.002f,
            "tilted non-uniformly scaled surface gets a correctly recomputed plane normal");
    }
    const auto tilted_contact = sdbz::query_stage_ground(
        tilted_collision, 2.0f, 0.4330127f, 2.75f);
    require(tilted_contact && std::abs(tilted_contact->m_position[1] - 2.75f) < 0.002f,
        "ground query interpolates height on a tilted world-space HIT surface");

    // Compare the indexed query path against the intentionally unindexed
    // linear reference path over many separated instances of the same HIT
    // object. This catches cell-edge omissions and duplicate candidate errors.
    sdbz::s_stage many_stage;
    std::vector<sdbz::mat4> many_worlds;
    constexpr std::size_t k_instance_count = 36;
    for (std::size_t index = 0; index < k_instance_count; ++index)
    {
        sdbz::s_stage_node node;
        node.m_index = static_cast<std::uint32_t>(index);
        node.m_kind = 3;
        node.m_name = "floor_instance";
        node.m_translation = {
            static_cast<float>(index % 6) * 32.0f,
            static_cast<float>(index % 4) * 0.5f,
            static_cast<float>(index / 6) * 32.0f
        };
        node.m_references.push_back({ 0, 0, "floor" });
        many_worlds.push_back(sdbz::trs_matrix(
            node.m_translation, node.m_rotation_radians, node.m_scale));
        many_stage.m_nodes.push_back(std::move(node));
    }
    const auto many_collision = sdbz::build_stage_collision_world(
        many_stage, hit, many_worlds);
    auto many_linear_reference = many_collision;
    many_linear_reference.m_spatial_index_ready = false;
    many_linear_reference.m_spatial_cells.clear();
    many_linear_reference.m_large_polygon_indices.clear();
    require(many_collision.m_polygons.size() == k_instance_count &&
        many_collision.m_spatial_cells.size() >= k_instance_count,
        "spatial index partitions separated stage collision instances");
    for (std::size_t index = 0; index < k_instance_count; ++index)
    {
        const float x = static_cast<float>(index % 6) * 32.0f + 2.0f;
        const float z = static_cast<float>(index / 6) * 32.0f + 2.0f;
        const float y = static_cast<float>(index % 4) * 0.5f;
        const auto indexed_contact = sdbz::query_stage_ground(
            many_collision, x, z, y);
        const auto linear_contact = sdbz::query_stage_ground(
            many_linear_reference, x, z, y);
        require(indexed_contact.has_value() == linear_contact.has_value(),
            "indexed and linear ground queries agree on contact existence");
        if (indexed_contact && linear_contact)
        {
            require(indexed_contact->m_stage_node_index ==
                linear_contact->m_stage_node_index &&
                std::abs(indexed_contact->m_position[1] -
                    linear_contact->m_position[1]) < 0.001f,
                "indexed and linear ground queries return the same support polygon");
        }
        require(sdbz::is_stage_capsule_supported(
            many_collision, x + 2.0f, z + 2.0f, y, 0.35f) ==
            sdbz::is_stage_capsule_supported(
                many_linear_reference, x + 2.0f, z + 2.0f, y, 0.35f),
            "indexed and linear capsule support checks agree");
        require(sdbz::is_stage_capsule_clear(
            many_collision, x + 2.0f, z + 2.0f, y) ==
            sdbz::is_stage_capsule_clear(
                many_linear_reference, x + 2.0f, z + 2.0f, y),
            "indexed and linear capsule clearance checks agree");
    }

    // Extremely large polygons are deliberately kept outside the grid and
    // checked globally, so a full arena floor cannot allocate millions of cells.
    sdbz::s_stage huge_stage;
    sdbz::s_stage_node huge_node;
    huge_node.m_index = 0;
    huge_node.m_kind = 3;
    huge_node.m_scale = { 200.0f, 1.0f, 200.0f };
    huge_node.m_references.push_back({ 0, 0, "floor" });
    huge_stage.m_nodes.push_back(huge_node);
    const std::vector<sdbz::mat4> huge_worlds = {
        sdbz::trs_matrix(huge_node.m_translation,
            huge_node.m_rotation_radians, huge_node.m_scale)
    };
    const auto huge_collision = sdbz::build_stage_collision_world(
        huge_stage, hit, huge_worlds);
    require(huge_collision.m_large_polygon_indices.size() == 1,
        "arena-sized HIT polygon uses the large-polygon fallback list");
    require(sdbz::query_stage_ground(huge_collision, 200.0f, 200.0f, 0.0f).has_value(),
        "large-polygon fallback remains queryable outside the spatial grid");

    // A concave HIT n-gon must be evaluated as a polygon, not a triangle fan.
    sdbz::s_hit concave_hit;
    sdbz::s_hit_object concave_object;
    concave_object.m_name = "concave_floor";
    concave_object.m_polygon_count = 1;
    concave_hit.m_objects.push_back(concave_object);
    sdbz::s_hit_polygon concave_polygon;
    concave_polygon.m_vertex_count = 6;
    concave_hit.m_polygons.push_back(concave_polygon);
    concave_hit.m_vertices = {
        { { 0.0f, 0.0f, 0.0f, 1.0f } },
        { { 4.0f, 0.0f, 0.0f, 1.0f } },
        { { 4.0f, 0.0f, 1.0f, 1.0f } },
        { { 1.0f, 0.0f, 1.0f, 1.0f } },
        { { 1.0f, 0.0f, 4.0f, 1.0f } },
        { { 0.0f, 0.0f, 4.0f, 1.0f } }
    };
    sdbz::s_stage concave_stage;
    sdbz::s_stage_node concave_node;
    concave_node.m_index = 0;
    concave_node.m_kind = 3;
    concave_node.m_references.push_back({ 0, 0, "concave_floor" });
    concave_stage.m_nodes.push_back(concave_node);
    const std::vector<sdbz::mat4> identity_world = {
        sdbz::trs_matrix(concave_node.m_translation,
            concave_node.m_rotation_radians, concave_node.m_scale)
    };
    const auto concave_collision = sdbz::build_stage_collision_world(
        concave_stage, concave_hit, identity_world);
    require(sdbz::query_stage_ground(concave_collision, 0.5f, 3.0f, 0.0f).has_value(),
        "point inside the arm of a concave HIT n-gon is supported");
    require(!sdbz::query_stage_ground(concave_collision, 3.0f, 3.0f, 0.0f),
        "point inside the concave notch is not incorrectly treated as floor");

    // A floor is commonly split into adjacent HIT triangles/quads. The full
    // player footprint must be allowed to overlap those seams.
    sdbz::s_stage_collision_world tiled_floor;
    for (int tile = 0; tile < 2; ++tile)
    {
        const float min_x = tile == 0 ? -2.0f : 0.0f;
        const float max_x = tile == 0 ? 0.0f : 2.0f;
        sdbz::s_stage_collision_polygon floor_tile;
        floor_tile.m_normal = { 0.0f, 1.0f, 0.0f };
        floor_tile.m_bounds_min = { min_x, 0.0f, -2.0f };
        floor_tile.m_bounds_max = { max_x, 0.0f, 2.0f };
        floor_tile.m_projected_area = 8.0f;
        floor_tile.m_vertices = {
            { min_x, 0.0f, -2.0f }, { max_x, 0.0f, -2.0f },
            { max_x, 0.0f, 2.0f }, { min_x, 0.0f, 2.0f }
        };
        tiled_floor.m_polygons.push_back(std::move(floor_tile));
    }
    require(sdbz::is_stage_capsule_supported(
        tiled_floor, -0.1f, 0.0f, 0.0f, 0.35f),
        "capsule footprint is supported across adjacent HIT floor polygons");
    require(!sdbz::is_stage_capsule_supported(
        tiled_floor, -1.9f, 0.0f, 0.0f, 0.35f),
        "capsule footprint is rejected when it extends beyond the combined floor");
    const auto tiled_spawn = sdbz::find_stage_player_spawn(tiled_floor);
    require(tiled_spawn.has_value(),
        "spawn finder accepts a floor assembled from adjacent HIT polygons");

    // Tiny gaps between authored HIT strips must not swallow a player's
    // bottom contact when the capsule footprint still overlaps the floor on
    // both sides of the seam.
    sdbz::s_stage_collision_world seamed_floor;
    for (int side = 0; side < 2; ++side)
    {
        const float min_x = side == 0 ? -2.0f : 0.01f;
        const float max_x = side == 0 ? -0.01f : 2.0f;
        sdbz::s_stage_collision_polygon floor_tile;
        floor_tile.m_normal = { 0.0f, 1.0f, 0.0f };
        floor_tile.m_bounds_min = { min_x, 0.0f, -2.0f };
        floor_tile.m_bounds_max = { max_x, 0.0f, 2.0f };
        floor_tile.m_projected_area = (max_x - min_x) * 4.0f;
        floor_tile.m_vertices = {
            { min_x, 0.0f, -2.0f }, { max_x, 0.0f, -2.0f },
            { max_x, 0.0f, 2.0f }, { min_x, 0.0f, 2.0f }
        };
        seamed_floor.m_polygons.push_back(std::move(floor_tile));
    }
    sdbz::build_stage_collision_spatial_index(seamed_floor);
    require(!sdbz::query_stage_ground(seamed_floor, 0.0f, 0.0f, 0.0f),
        "the deliberate tiny HIT seam has no direct centre polygon");
    require(sdbz::query_stage_ground_capsule(
        seamed_floor, 0.0f, 0.0f, 0.0f, 0.25f, 0.35f, 2.0f, 50.0f).has_value(),
        "capsule ground query bridges a tiny seam using consistent nearby floor samples");

    // With an untransformed stage root, the spawn finder prefers the
    // walkable surface closest to the root's Y=0 origin over an elevated one.
    sdbz::s_stage_collision_world two_floor_world;
    {
        sdbz::s_stage_collision_polygon platform;
        platform.m_normal = { 0.0f, 1.0f, 0.0f };
        platform.m_bounds_min = { -10.0f, 11.0f, -10.0f };
        platform.m_bounds_max = { 10.0f, 11.0f, 10.0f };
        platform.m_projected_area = 400.0f;
        platform.m_object_name = "high_platform";
        platform.m_vertices = {
            { -10.0f, 11.0f, -10.0f }, { 10.0f, 11.0f, -10.0f },
            { 10.0f, 11.0f, 10.0f }, { -10.0f, 11.0f, 10.0f }
        };
        two_floor_world.m_polygons.push_back(std::move(platform));
    }
    {
        sdbz::s_stage_collision_polygon floor;
        floor.m_normal = { 0.0f, 1.0f, 0.0f };
        floor.m_bounds_min = { -2.0f, 0.0f, -2.0f };
        floor.m_bounds_max = { 2.0f, 0.0f, 2.0f };
        floor.m_projected_area = 16.0f;
        floor.m_object_name = "ground_floor";
        floor.m_vertices = {
            { -2.0f, 0.0f, -2.0f }, { 2.0f, 0.0f, -2.0f },
            { 2.0f, 0.0f, 2.0f }, { -2.0f, 0.0f, 2.0f }
        };
        two_floor_world.m_polygons.push_back(std::move(floor));
    }
    sdbz::build_stage_collision_spatial_index(two_floor_world);
    const auto two_floor_spawn = sdbz::find_stage_player_spawn(two_floor_world);
    require(two_floor_spawn.has_value(),
        "spawn finder selects a surface on a two-floor world");
    if (two_floor_spawn)
    {
        require(std::abs(two_floor_spawn->m_position[1] - 0.05f) < 0.01f,
            "spawn selects the low ground floor, not the high platform");
        require(two_floor_spawn->m_object_name == "ground_floor",
            "spawn object name is the ground floor");
    }

    // Do not group different elevations into broad half-unit buckets: a
    // slightly raised but much larger platform must not beat the lower floor
    // merely because its polygon has more clearance.
    sdbz::s_stage_collision_world close_height_world;
    {
        sdbz::s_stage_collision_polygon raised_platform;
        raised_platform.m_normal = { 0.0f, 1.0f, 0.0f };
        raised_platform.m_bounds_min = { 5.0f, 0.4f, -10.0f };
        raised_platform.m_bounds_max = { 25.0f, 0.4f, 10.0f };
        raised_platform.m_projected_area = 400.0f;
        raised_platform.m_object_name = "raised_platform";
        raised_platform.m_vertices = {
            { 5.0f, 0.4f, -10.0f }, { 25.0f, 0.4f, -10.0f },
            { 25.0f, 0.4f, 10.0f }, { 5.0f, 0.4f, 10.0f }
        };
        close_height_world.m_polygons.push_back(std::move(raised_platform));
    }
    {
        sdbz::s_stage_collision_polygon ground_floor;
        ground_floor.m_normal = { 0.0f, 1.0f, 0.0f };
        ground_floor.m_bounds_min = { -18.0f, 0.0f, -3.0f };
        ground_floor.m_bounds_max = { -12.0f, 0.0f, 3.0f };
        ground_floor.m_projected_area = 36.0f;
        ground_floor.m_object_name = "ground_floor";
        ground_floor.m_vertices = {
            { -18.0f, 0.0f, -3.0f }, { -12.0f, 0.0f, -3.0f },
            { -12.0f, 0.0f, 3.0f }, { -18.0f, 0.0f, 3.0f }
        };
        close_height_world.m_polygons.push_back(std::move(ground_floor));
    }
    sdbz::build_stage_collision_spatial_index(close_height_world);
    const auto close_height_spawn = sdbz::find_stage_player_spawn(close_height_world);
    require(close_height_spawn.has_value() &&
        close_height_spawn->m_object_name == "ground_floor" &&
        std::abs(close_height_spawn->m_position[1] - 0.05f) < 0.01f,
        "spawn ranking prefers the truly lower floor over a larger platform within half a unit");

    // A collision polygon below the stage (e.g. world bounds) must not win
    // over the actual playable ground just because it is lower. Prefer the
    // surface closest to the stage-root reference height.
    sdbz::s_stage_collision_world below_ground_world;
    {
        sdbz::s_stage_collision_polygon below;
        below.m_normal = { 0.0f, 1.0f, 0.0f };
        below.m_bounds_min = { -20.0f, -64.0f, -20.0f };
        below.m_bounds_max = { 20.0f, -64.0f, 20.0f };
        below.m_projected_area = 1600.0f;
        below.m_object_name = "below_ground";
        below.m_vertices = {
            { -20.0f, -64.0f, -20.0f }, { 20.0f, -64.0f, -20.0f },
            { 20.0f, -64.0f, 20.0f }, { -20.0f, -64.0f, 20.0f }
        };
        below_ground_world.m_polygons.push_back(std::move(below));
    }
    {
        sdbz::s_stage_collision_polygon floor;
        floor.m_normal = { 0.0f, 1.0f, 0.0f };
        floor.m_bounds_min = { -2.0f, 0.0f, -2.0f };
        floor.m_bounds_max = { 2.0f, 0.0f, 2.0f };
        floor.m_projected_area = 16.0f;
        floor.m_object_name = "ground_floor";
        floor.m_vertices = {
            { -2.0f, 0.0f, -2.0f }, { 2.0f, 0.0f, -2.0f },
            { 2.0f, 0.0f, 2.0f }, { -2.0f, 0.0f, 2.0f }
        };
        below_ground_world.m_polygons.push_back(std::move(floor));
    }
    sdbz::build_stage_collision_spatial_index(below_ground_world);
    const auto below_spawn = sdbz::find_stage_player_spawn(below_ground_world);
    require(below_spawn.has_value(),
        "spawn finder selects a surface when geometry exists below ground");
    if (below_spawn)
    {
        require(std::abs(below_spawn->m_position[1] - 0.05f) < 0.01f,
            "spawn selects ground at Y=0, not geometry below the stage");
        require(below_spawn->m_object_name == "ground_floor",
            "spawn object name is the ground floor");
    }

    // Among surfaces at similar heights, the one with the deepest interior
    // clearance wins so the player does not spawn at a cliff edge or boundary
    // sliver.
    sdbz::s_stage_collision_world near_ground_world;
    {
        sdbz::s_stage_collision_polygon edge_marker;
        edge_marker.m_normal = { 0.0f, 1.0f, 0.0f };
        edge_marker.m_bounds_min = { -0.5f, 0.0f, -0.5f };
        edge_marker.m_bounds_max = { 0.5f, 0.0f, 0.5f };
        edge_marker.m_projected_area = 1.0f;
        edge_marker.m_object_name = "edge_marker";
        edge_marker.m_vertices = {
            { -0.5f, 0.0f, -0.5f }, { 0.5f, 0.0f, -0.5f },
            { 0.5f, 0.0f, 0.5f }, { -0.5f, 0.0f, 0.5f }
        };
        near_ground_world.m_polygons.push_back(std::move(edge_marker));
    }
    {
        sdbz::s_stage_collision_polygon floor;
        floor.m_normal = { 0.0f, 1.0f, 0.0f };
        floor.m_bounds_min = { -10.0f, 0.0f, -10.0f };
        floor.m_bounds_max = { 10.0f, 0.0f, 10.0f };
        floor.m_projected_area = 400.0f;
        floor.m_object_name = "ground_floor";
        floor.m_vertices = {
            { -10.0f, 0.0f, -10.0f }, { 10.0f, 0.0f, -10.0f },
            { 10.0f, 0.0f, 10.0f }, { -10.0f, 0.0f, 10.0f }
        };
        near_ground_world.m_polygons.push_back(std::move(floor));
    }
    sdbz::build_stage_collision_spatial_index(near_ground_world);
    const auto near_spawn = sdbz::find_stage_player_spawn(near_ground_world);
    require(near_spawn.has_value(),
        "spawn finder selects a surface among near-ground options");
    if (near_spawn)
    {
        require(std::abs(near_spawn->m_position[1] - 0.05f) < 0.01f,
            "spawn is at ground level Y=0");
        require(near_spawn->m_object_name == "ground_floor",
            "spawn selects the larger floor with deeper interior clearance");
    }

    // A walkable polygon must have enough interior space for the capsule
    // plus an edge margin; a thin strip or narrow platform rejects the spawn.
    sdbz::s_stage_collision_world edge_margin_world;
    {
        sdbz::s_stage_collision_polygon thin_strip;
        thin_strip.m_normal = { 0.0f, 1.0f, 0.0f };
        thin_strip.m_bounds_min = { -2.0f, 0.0f, -0.2f };
        thin_strip.m_bounds_max = { 2.0f, 0.0f, 0.2f };
        thin_strip.m_projected_area = 0.8f;
        thin_strip.m_object_name = "thin_strip";
        thin_strip.m_vertices = {
            { -2.0f, 0.0f, -0.2f }, { 2.0f, 0.0f, -0.2f },
            { 2.0f, 0.0f, 0.2f }, { -2.0f, 0.0f, 0.2f }
        };
        edge_margin_world.m_polygons.push_back(std::move(thin_strip));
    }
    sdbz::build_stage_collision_spatial_index(edge_margin_world);
    require(!sdbz::find_stage_player_spawn(edge_margin_world),
        "spawn finder rejects a walkable strip with no capsule-clear interior");

    // A large walkable polygon still accepts a spawn with adequate edge clearance.
    sdbz::s_stage_collision_world large_floor_world;
    {
        sdbz::s_stage_collision_polygon large_floor;
        large_floor.m_normal = { 0.0f, 1.0f, 0.0f };
        large_floor.m_bounds_min = { -20.0f, 0.0f, -20.0f };
        large_floor.m_bounds_max = { 20.0f, 0.0f, 20.0f };
        large_floor.m_projected_area = 1600.0f;
        large_floor.m_object_name = "large_floor";
        large_floor.m_vertices = {
            { -20.0f, 0.0f, -20.0f }, { 20.0f, 0.0f, -20.0f },
            { 20.0f, 0.0f, 20.0f }, { -20.0f, 0.0f, 20.0f }
        };
        large_floor_world.m_polygons.push_back(std::move(large_floor));
    }
    sdbz::build_stage_collision_spatial_index(large_floor_world);
    const auto large_spawn = sdbz::find_stage_player_spawn(large_floor_world);
    require(large_spawn.has_value(),
        "spawn finder accepts a large walkable floor with edge clearance");
    if (large_spawn)
    {
        require(std::abs(large_spawn->m_position[1] - 0.05f) < 0.01f,
            "spawn on large floor is at ground level");
        require(large_spawn->m_object_name == "large_floor",
            "spawn object name is the large floor");
    }

    sdbz::s_stage_collision_world wall_world;
    sdbz::s_stage_collision_polygon wall;
    wall.m_normal = { 1.0f, 0.0f, 0.0f };
    wall.m_bounds_min = { 1.0f, 0.0f, -2.0f };
    wall.m_bounds_max = { 1.0f, 2.0f, 2.0f };
    wall.m_vertices = {
        { 1.0f, 0.0f, -2.0f }, { 1.0f, 2.0f, -2.0f },
        { 1.0f, 2.0f, 2.0f }, { 1.0f, 0.0f, 2.0f }
    };
    wall_world.m_polygons.push_back(wall);
    sdbz::build_stage_collision_spatial_index(wall_world);
    auto wall_linear_reference = wall_world;
    wall_linear_reference.m_spatial_index_ready = false;
    wall_linear_reference.m_spatial_cells.clear();
    wall_linear_reference.m_large_polygon_indices.clear();
    const auto wall_resolved = sdbz::resolve_stage_horizontal_movement(
        wall_world, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f);
    require(wall_resolved[0] < 1.0f && wall_resolved[0] > 0.5f,
        "horizontal movement is stopped before a vertical HIT wall");
    const auto wall_slide = sdbz::resolve_stage_horizontal_movement(
        wall_world, 0.0f, 0.0f, 2.0f, 1.0f, 0.0f);
    require(wall_slide[0] < 1.0f && wall_slide[1] > 0.9f,
        "horizontal movement slides along a wall instead of stopping tangential movement");
    const auto wall_resolved_linear = sdbz::resolve_stage_horizontal_movement(
        wall_linear_reference, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f);
    require(std::abs(wall_resolved[0] - wall_resolved_linear[0]) < 0.001f &&
        std::abs(wall_resolved[1] - wall_resolved_linear[1]) < 0.001f,
        "indexed and linear wall movement queries agree");
    require(!sdbz::is_stage_capsule_clear(
        wall_world, 0.8f, 0.0f, 0.0f, 0.35f, 1.75f),
        "capsule clearance rejects a player intersecting a vertical wall");

    // A low barrier touching only the lower hemisphere should use that
    // hemisphere's narrower cross-section, not the full cylinder radius.
    sdbz::s_stage_collision_world low_barrier_world;
    sdbz::s_stage_collision_polygon low_barrier;
    low_barrier.m_normal = { 1.0f, 0.0f, 0.0f };
    low_barrier.m_bounds_min = { 0.2f, 0.0f, -1.0f };
    low_barrier.m_bounds_max = { 0.2f, 0.05f, 1.0f };
    low_barrier.m_vertices = {
        { 0.2f, 0.0f, -1.0f }, { 0.2f, 0.05f, -1.0f },
        { 0.2f, 0.05f, 1.0f }, { 0.2f, 0.0f, 1.0f }
    };
    low_barrier_world.m_polygons.push_back(low_barrier);
    sdbz::build_stage_collision_spatial_index(low_barrier_world);
    require(sdbz::is_stage_capsule_clear(
        low_barrier_world, 0.0f, 0.0f, 0.0f, 0.25f, 1.75f),
        "low barrier outside the capsule's bottom-hemisphere cross-section does not block it");

    sdbz::s_stage_collision_world ceiling_world = collision;
    sdbz::s_stage_collision_polygon ceiling;
    ceiling.m_normal = { 0.0f, 1.0f, 0.0f };
    ceiling.m_bounds_min = { 5.0f, 3.0f, -3.0f };
    ceiling.m_bounds_max = { 9.0f, 3.0f, 1.0f };
    ceiling.m_vertices = {
        { 5.0f, 3.0f, -3.0f }, { 9.0f, 3.0f, -3.0f },
        { 9.0f, 3.0f, 1.0f }, { 5.0f, 3.0f, 1.0f }
    };
    ceiling_world.m_polygons.push_back(ceiling);
    sdbz::build_stage_collision_spatial_index(ceiling_world);
    require(!sdbz::is_stage_capsule_clear(
        ceiling_world, 6.0f, -2.0f, 2.0f, 0.35f, 1.75f),
        "capsule clearance rejects a low overhead HIT surface");

    const auto spawn = sdbz::find_stage_player_spawn(collision);
    require(spawn.has_value(), "spawn finder selects an interior point on a walkable HIT polygon");
    if (spawn)
    {
        require(spawn->m_position[1] > 2.0f,
            "spawn has a small positive foot clearance above the HIT surface");
        const auto spawn_contact = sdbz::query_stage_ground(collision,
            spawn->m_position[0], spawn->m_position[2], spawn->m_position[1]);
        require(spawn_contact.has_value(), "spawn position is supported by its collision polygon");
    }
    require(!sdbz::find_stage_player_spawn(collision, 0.05f, 50.0f, 2.1f, 1.75f),
        "spawn finder rejects a support surface that cannot contain the configured capsule");

    // A player that starts above this stage must land on its authored HIT floor,
    // not on the old hard-coded world Y=0 plane.
    sdbz::s_player_controller controller;
    controller.m_position = { 6.0f, 5.0f, -2.0f };
    controller.m_grounded = false;
    sdbz::s_player_input input{};
    for (int frame = 0; frame < 180; ++frame)
    {
        sdbz::update_player_controller(controller, input, 1.0f / 60.0f, &collision);
    }
    require(controller.m_grounded, "collision controller lands on a stage HIT surface");
    require(controller.m_ground_polygon_index == 0,
        "controller retains the active HIT support polygon for diagnostics");
    require(std::abs(controller.m_position[1] - 2.05f) < 0.02f,
        "collision controller settles at transformed floor height plus foot offset");

    sdbz::s_player_controller landing_controller;
    landing_controller.m_position = { 6.0f, 2.15f, -2.0f };
    landing_controller.m_velocity[1] = -1.0f;
    landing_controller.m_grounded = false;
    sdbz::s_player_input held_jump_input{};
    held_jump_input.m_jump_pressed = true;
    sdbz::update_player_controller(landing_controller, held_jump_input, 0.1f,
        &collision);
    require(landing_controller.m_grounded && landing_controller.m_velocity[1] == 0.0f,
        "a jump edge received while falling is not queued and triggered on landing");

    sdbz::s_stage_collision_world low_ceiling_world = collision;
    sdbz::s_stage_collision_polygon low_ceiling;
    low_ceiling.m_normal = { 0.0f, 1.0f, 0.0f };
    low_ceiling.m_bounds_min = { 5.0f, 3.8f, -3.0f };
    low_ceiling.m_bounds_max = { 9.0f, 3.8f, 1.0f };
    low_ceiling.m_vertices = {
        { 5.0f, 3.8f, -3.0f }, { 9.0f, 3.8f, -3.0f },
        { 9.0f, 3.8f, 1.0f }, { 5.0f, 3.8f, 1.0f }
    };
    low_ceiling_world.m_polygons.push_back(low_ceiling);
    sdbz::build_stage_collision_spatial_index(low_ceiling_world);
    sdbz::s_player_controller jump_controller;
    jump_controller.m_position = { 6.0f, 2.05f, -2.0f };
    jump_controller.m_grounded = true;
    sdbz::s_player_input jump_input{};
    jump_input.m_jump_pressed = true;
    sdbz::update_player_controller(jump_controller, jump_input, 1.0f / 60.0f,
        &low_ceiling_world);
    require(jump_controller.m_velocity[1] == 0.0f && jump_controller.m_position[1] < 2.2f,
        "player jump is stopped by an overhead HIT surface before entering it");

    sdbz::s_player_controller edge_controller;
    edge_controller.m_position = { 8.5f, 2.05f, -2.0f };
    edge_controller.m_grounded = true;
    sdbz::s_player_input edge_input{};
    edge_input.m_move_x = 1.0f;
    for (int frame = 0; frame < 60; ++frame)
    {
        sdbz::update_player_controller(edge_controller, edge_input, 1.0f / 60.0f,
            &collision);
    }
    require(!edge_controller.m_grounded && edge_controller.m_position[1] < 2.0f,
        "player walks off a HIT platform and falls instead of remaining on an infinite plane");

    // Capsule support must report the centre contact on a slope, not the
    // highest ring sample (which would lift the actor by radius * slope).
    sdbz::s_stage_collision_world sloped_world;
    sdbz::s_stage_collision_polygon sloped_floor;
    sloped_floor.m_normal = { -0.4472136f, 0.8944272f, 0.0f };
    sloped_floor.m_bounds_min = { -5.0f, -2.5f, -5.0f };
    sloped_floor.m_bounds_max = { 5.0f, 2.5f, 5.0f };
    sloped_floor.m_projected_area = 100.0f;
    sloped_floor.m_vertices = {
        { -5.0f, -2.5f, -5.0f }, { 5.0f, 2.5f, -5.0f },
        { 5.0f, 2.5f, 5.0f }, { -5.0f, -2.5f, 5.0f }
    };
    sloped_world.m_polygons.push_back(sloped_floor);
    sdbz::build_stage_collision_spatial_index(sloped_world);
    const auto slope_contact = sdbz::query_stage_ground_capsule(
        sloped_world, 0.0f, 0.0f, 0.0f, 0.25f, 0.1f, 0.1f, 50.0f);
    require(slope_contact.has_value(),
        "capsule ground query accepts a fully supported sloped floor");
    if (slope_contact)
    {
        require(std::abs(slope_contact->m_position[0]) < 0.001f &&
            std::abs(slope_contact->m_position[1]) < 0.001f &&
            std::abs(slope_contact->m_position[2]) < 0.001f,
            "capsule ground query returns the centre height on a slope");
    }

    // A continuous 45-degree ramp meeting a flat floor must not lose support
    // when the capsule straddles the crease. Both polygons are independently
    // walkable, but their normals differ by 45 degrees.
    sdbz::s_stage_collision_world ramp_floor_seam;
    sdbz::s_stage_collision_polygon ramp_polygon;
    ramp_polygon.m_normal = { -0.70710678f, 0.70710678f, 0.0f };
    ramp_polygon.m_bounds_min = { -3.0f, -3.0f, -3.0f };
    ramp_polygon.m_bounds_max = { 0.0f, 0.0f, 3.0f };
    ramp_polygon.m_projected_area = 18.0f;
    ramp_polygon.m_vertices = {
        { -3.0f, -3.0f, -3.0f }, { 0.0f, 0.0f, -3.0f },
        { 0.0f, 0.0f, 3.0f }, { -3.0f, -3.0f, 3.0f }
    };
    ramp_floor_seam.m_polygons.push_back(ramp_polygon);

    sdbz::s_stage_collision_polygon flat_polygon;
    flat_polygon.m_normal = { 0.0f, 1.0f, 0.0f };
    flat_polygon.m_bounds_min = { 0.0f, 0.0f, -3.0f };
    flat_polygon.m_bounds_max = { 3.0f, 0.0f, 3.0f };
    flat_polygon.m_projected_area = 18.0f;
    flat_polygon.m_vertices = {
        { 0.0f, 0.0f, -3.0f }, { 3.0f, 0.0f, -3.0f },
        { 3.0f, 0.0f, 3.0f }, { 0.0f, 0.0f, 3.0f }
    };
    ramp_floor_seam.m_polygons.push_back(flat_polygon);
    sdbz::build_stage_collision_spatial_index(ramp_floor_seam);

    const auto crease_support = sdbz::query_stage_ground_capsule(
        ramp_floor_seam, -0.1f, 0.0f, -0.1f, 0.25f, 0.1f, 0.1f, 50.0f);
    require(crease_support.has_value(),
        "capsule remains supported across a continuous ramp-to-flat HIT crease");

    sdbz::s_player_controller ramp_controller;
    ramp_controller.m_position = { -0.1f, -0.05f, 0.0f };
    ramp_controller.m_grounded = true;
    sdbz::s_player_input ramp_movement{};
    ramp_movement.m_move_x = 1.0f;
    for (int frame = 0; frame < 24; ++frame)
    {
        sdbz::update_player_controller(ramp_controller, ramp_movement,
            1.0f / 60.0f, &ramp_floor_seam);
    }
    require(ramp_controller.m_grounded && ramp_controller.m_position[0] > 0.5f,
        "player crosses from a sloped HIT polygon onto the adjoining flat floor");

    sdbz::s_player_controller ramp_jump_controller;
    ramp_jump_controller.m_position = { -0.1f, -0.05f, 0.0f };
    ramp_jump_controller.m_grounded = true;
    sdbz::s_player_input ramp_jump{};
    ramp_jump.m_jump_pressed = true;
    sdbz::update_player_controller(ramp_jump_controller, ramp_jump,
        1.0f / 60.0f, &ramp_floor_seam);
    require(!ramp_jump_controller.m_grounded && ramp_jump_controller.m_velocity[1] > 0.0f,
        "player can jump while standing at a walkable ramp-to-flat crease");

    // Slopes steeper than the former 50-degree limit were being classified as
    // walls, leaving the actor neither grounded nor able to jump. The default
    // controller limit now permits a 60-degree arena ramp while still treating
    // near-vertical HIT faces as blocking walls.
    sdbz::s_stage_collision_world steep_ramp_world;
    sdbz::s_stage_collision_polygon steep_ramp;
    constexpr float k_steep_ramp_angle = 60.0f;
    constexpr float k_steep_ramp_radians = k_steep_ramp_angle *
        0.01745329251994329577f;
    const float steep_ramp_gradient = std::tan(k_steep_ramp_radians);
    steep_ramp.m_normal = { -std::sin(k_steep_ramp_radians),
        std::cos(k_steep_ramp_radians), 0.0f };
    steep_ramp.m_bounds_min = { -4.0f, -4.0f * steep_ramp_gradient, -3.0f };
    steep_ramp.m_bounds_max = { 4.0f, 4.0f * steep_ramp_gradient, 3.0f };
    steep_ramp.m_projected_area = 48.0f;
    steep_ramp.m_vertices = {
        { -4.0f, -4.0f * steep_ramp_gradient, -3.0f },
        { 4.0f, 4.0f * steep_ramp_gradient, -3.0f },
        { 4.0f, 4.0f * steep_ramp_gradient, 3.0f },
        { -4.0f, -4.0f * steep_ramp_gradient, 3.0f }
    };
    steep_ramp_world.m_polygons.push_back(steep_ramp);
    sdbz::build_stage_collision_spatial_index(steep_ramp_world);

    sdbz::s_player_controller steep_ramp_controller;
    steep_ramp_controller.m_position = { -1.0f,
        -steep_ramp_gradient + steep_ramp_controller.m_collision_foot_offset, 0.0f };
    steep_ramp_controller.m_grounded = true;
    sdbz::s_player_input steep_ramp_input{};
    steep_ramp_input.m_move_x = 1.0f;
    steep_ramp_input.m_run = true;
    for (int frame = 0; frame < 24; ++frame)
    {
        sdbz::update_player_controller(steep_ramp_controller, steep_ramp_input,
            1.0f / 60.0f, &steep_ramp_world);
    }
    require(steep_ramp_controller.m_grounded && steep_ramp_controller.m_position[0] > 0.0f,
        "player can run up a 60-degree HIT ramp without sticking or losing ground contact");

    sdbz::s_player_input steep_ramp_jump{};
    steep_ramp_jump.m_jump_pressed = true;
    const float steep_ramp_y_before_jump = steep_ramp_controller.m_position[1];
    sdbz::update_player_controller(steep_ramp_controller, steep_ramp_jump,
        1.0f / 60.0f, &steep_ramp_world);
    require(!steep_ramp_controller.m_grounded && steep_ramp_controller.m_velocity[1] > 0.0f &&
        steep_ramp_controller.m_position[1] > steep_ramp_y_before_jump,
        "player can jump from a 60-degree ramp while uphill momentum is still present");

    // A centre contact is not enough at a ledge: the full footprint must be
    // supported across the whole ring, including the outer edge.
    sdbz::s_stage_collision_world partial_edge_world;
    sdbz::s_stage_collision_polygon partial_edge_floor;
    partial_edge_floor.m_normal = { 0.0f, 1.0f, 0.0f };
    partial_edge_floor.m_bounds_min = { -2.0f, 0.0f, -2.0f };
    partial_edge_floor.m_bounds_max = { 2.0f, 0.0f, 2.0f };
    partial_edge_floor.m_projected_area = 16.0f;
    partial_edge_floor.m_vertices = {
        { -2.0f, 0.0f, -2.0f }, { 2.0f, 0.0f, -2.0f },
        { 2.0f, 0.0f, 2.0f }, { -2.0f, 0.0f, 2.0f }
    };
    partial_edge_world.m_polygons.push_back(partial_edge_floor);
    sdbz::build_stage_collision_spatial_index(partial_edge_world);
    require(sdbz::query_stage_ground(partial_edge_world, 1.9f, 0.0f, 0.0f).has_value(),
        "the centre point remains over the floor near the edge");
    require(!sdbz::query_stage_ground_capsule(
        partial_edge_world, 1.9f, 0.0f, 0.0f, 0.25f, 0.1f, 0.1f, 50.0f),
        "capsule ground query rejects a footprint that hangs over a ledge");
    sdbz::s_player_controller partial_edge_controller;
    partial_edge_controller.m_position = { 1.9f, 0.05f, 0.0f };
    partial_edge_controller.m_grounded = true;
    sdbz::s_player_input partial_edge_input{};
    partial_edge_input.m_move_x = 1.0f;
    sdbz::update_player_controller(partial_edge_controller, partial_edge_input,
        1.0f / 60.0f, &partial_edge_world);
    require(!partial_edge_controller.m_grounded &&
        partial_edge_controller.m_position[1] < 0.05f,
        "controller starts falling when the capsule footprint no longer fits on the floor");

    // Spawn height must be measured relative to the live STAGE root origin,
    // not the world's absolute Y=0. This represents a stage translated upward.
    sdbz::s_stage_collision_world translated_spawn_world;
    translated_spawn_world.m_stage_origin_y = 100.0f;
    sdbz::s_stage_collision_polygon world_origin_floor;
    world_origin_floor.m_normal = { 0.0f, 1.0f, 0.0f };
    world_origin_floor.m_bounds_min = { -10.0f, 0.0f, -10.0f };
    world_origin_floor.m_bounds_max = { 10.0f, 0.0f, 10.0f };
    world_origin_floor.m_projected_area = 400.0f;
    world_origin_floor.m_object_name = "world_origin_floor";
    world_origin_floor.m_vertices = {
        { -10.0f, 0.0f, -10.0f }, { 10.0f, 0.0f, -10.0f },
        { 10.0f, 0.0f, 10.0f }, { -10.0f, 0.0f, 10.0f }
    };
    translated_spawn_world.m_polygons.push_back(world_origin_floor);
    sdbz::s_stage_collision_polygon translated_floor = world_origin_floor;
    translated_floor.m_bounds_min[1] = 100.0f;
    translated_floor.m_bounds_max[1] = 100.0f;
    translated_floor.m_object_name = "translated_stage_floor";
    for (auto& vertex : translated_floor.m_vertices)
    {
        vertex[1] = 100.0f;
    }
    translated_spawn_world.m_polygons.push_back(translated_floor);
    sdbz::build_stage_collision_spatial_index(translated_spawn_world);
    const auto translated_spawn = sdbz::find_stage_player_spawn(translated_spawn_world);
    require(translated_spawn.has_value() &&
        std::abs(translated_spawn->m_position[1] - 100.05f) < 0.01f &&
        translated_spawn->m_object_name == "translated_stage_floor",
        "spawn selection follows the translated STAGE root instead of world Y=0");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " stage-collision test(s) failed\n";
        return 1;
    }
    std::cout << "Stage-collision tests passed\n";
    return 0;
}