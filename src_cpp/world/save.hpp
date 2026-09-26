#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "core/nbt.hpp"

namespace craftpp::entity {
class Player;
}  // namespace craftpp::entity

namespace craftpp::world {

class LiveWorld;

// level.dat Data compound (mirrors WorldInfo.updateTagCompound; 1.0
// save version 19132). Player persistence lands with GUI/inventory.
struct WorldInfoData {
  std::int64_t seed = 0;
  int game_type = 0;  // survival
  bool map_features = true;
  int spawn_x = 0, spawn_y = 64, spawn_z = 0;
  std::int64_t time = 0;
  std::string level_name = "Craft++";
  int version = 19132;
  // Optional level.dat Player compound (absent when no player was saved).
  std::optional<nbt::Tag> player;
};

nbt::Tag level_dat_tag(const WorldInfoData& info);
WorldInfoData world_info_from_tag(const nbt::Tag& root);

// Chunk <-> root-"" {"Level": {...}} NBT (mirrors
// ChunkLoader.storeChunkInCompound / loadChunkIntoWorldFromCompound).
// Entities/TileEntities/TileTicks serialize in a follow-up; empty lists
// are valid on disk and load in Java 1.0.
nbt::Tag chunk_to_tag(const LiveWorld& world, int cx, int cz);
bool chunk_from_tag(const nbt::Tag& root, LiveWorld& world, int cx, int cz);

// Player <-> level.dat Player compound (mirrors EntityPlayer.writeEntityToNBT
// + Entity.writeToNBT/readFromNBT for the fields we simulate; capabilities
// are omitted and reload as Java defaults). Feet convention for Pos.
nbt::Tag player_tag(const entity::Player& p);
void apply_player_tag(entity::Player& p, const nbt::Tag& tag);

std::optional<WorldInfoData> read_level_dat(const std::string& save_dir);
void write_level_dat(const std::string& save_dir, const WorldInfoData& info);

}  // namespace craftpp::world
