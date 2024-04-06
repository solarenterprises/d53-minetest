/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "clientmap_norender.h"
#include "client.h"
#include <matrix4.h>
#include "mapsector.h"
#include "mapblock.h"
#include "nodedef.h"
#include "profiler.h"
#include "settings.h"
#include "util/basic_macros.h"

#include <queue>

static void on_settings_changed(const std::string& name, void* data)
{
	static_cast<ClientMapNoRender*>(data)->onSettingChanged(name);
}
// ClientMapNoRender

ClientMapNoRender::ClientMapNoRender(
	Client* client,
	MapDrawControl& control,
	s32 id
) :
	Map(client),
	m_client(client),
	m_control(control)
{
	/* TODO: Add a callback function so these can be updated when a setting
	 *       changes.  At this point in time it doesn't matter (e.g. /set
	 *       is documented to change server settings only)
	 *
	 * TODO: Local caching of settings is not optimal and should at some stage
	 *       be updated to use a global settings object for getting thse values
	 *       (as opposed to the this local caching). This can be addressed in
	 *       a later release.
	 */
	g_settings->registerChangedCallback("occlusion_culler", on_settings_changed, this);
	g_settings->registerChangedCallback("enable_raytraced_culling", on_settings_changed, this);
}

void ClientMapNoRender::onSettingChanged(const std::string& name)
{
}

ClientMapNoRender::~ClientMapNoRender()
{
	g_settings->deregisterChangedCallback("occlusion_culler", on_settings_changed, this);
	g_settings->deregisterChangedCallback("enable_raytraced_culling", on_settings_changed, this);
}

MapSector* ClientMapNoRender::emergeSector(v2s16 p2d)
{
	// Check that it doesn't exist already
	MapSector* sector = getSectorNoGenerate(p2d);

	// Create it if it does not exist yet
	if (!sector) {
		sector = new MapSector(this, p2d, m_gamedef);
		m_sectors[p2d] = sector;
	}

	return sector;
}

void ClientMapNoRender::getBlocksInViewRange(v3s16 cam_pos_nodes,
	v3s16* p_blocks_min, v3s16* p_blocks_max, float range)
{
	if (range <= 0.0f)
		range = m_control.wanted_range;

	v3s16 box_nodes_d = range * v3s16(1, 1, 1);
	// Define p_nodes_min/max as v3s32 because 'cam_pos_nodes -/+ box_nodes_d'
	// can exceed the range of v3s16 when a large view range is used near the
	// world edges.
	v3s32 p_nodes_min(
		cam_pos_nodes.X - box_nodes_d.X,
		cam_pos_nodes.Y - box_nodes_d.Y,
		cam_pos_nodes.Z - box_nodes_d.Z);
	v3s32 p_nodes_max(
		cam_pos_nodes.X + box_nodes_d.X,
		cam_pos_nodes.Y + box_nodes_d.Y,
		cam_pos_nodes.Z + box_nodes_d.Z);
	// Take a fair amount as we will be dropping more out later
	// Umm... these additions are a bit strange but they are needed.
	*p_blocks_min = v3s16(
		p_nodes_min.X / MAP_BLOCKSIZE - 3,
		p_nodes_min.Y / MAP_BLOCKSIZE - 3,
		p_nodes_min.Z / MAP_BLOCKSIZE - 3);
	*p_blocks_max = v3s16(
		p_nodes_max.X / MAP_BLOCKSIZE + 1,
		p_nodes_max.Y / MAP_BLOCKSIZE + 1,
		p_nodes_max.Z / MAP_BLOCKSIZE + 1);
}

class MapBlockFlags
{
public:
	static constexpr u16 CHUNK_EDGE = 8;
	static constexpr u16 CHUNK_MASK = CHUNK_EDGE - 1;
	static constexpr std::size_t CHUNK_VOLUME = CHUNK_EDGE * CHUNK_EDGE * CHUNK_EDGE; // volume of a chunk

	MapBlockFlags(v3s16 min_pos, v3s16 max_pos)
		: min_pos(min_pos), volume((max_pos - min_pos) / CHUNK_EDGE + 1)
	{
		chunks.resize(volume.X * volume.Y * volume.Z);
	}

	class Chunk
	{
	public:
		inline u8& getBits(v3s16 pos)
		{
			std::size_t address = getAddress(pos);
			return bits[address];
		}

	private:
		inline std::size_t getAddress(v3s16 pos) {
			std::size_t address = (pos.X & CHUNK_MASK) + (pos.Y & CHUNK_MASK) * CHUNK_EDGE + (pos.Z & CHUNK_MASK) * (CHUNK_EDGE * CHUNK_EDGE);
			return address;
		}

		std::array<u8, CHUNK_VOLUME> bits;
	};

	Chunk& getChunk(v3s16 pos)
	{
		v3s16 delta = (pos - min_pos) / CHUNK_EDGE;
		std::size_t address = delta.X + delta.Y * volume.X + delta.Z * volume.X * volume.Y;
		Chunk* chunk = chunks[address].get();
		if (!chunk) {
			chunk = new Chunk();
			chunks[address].reset(chunk);
		}
		return *chunk;
	}
private:
	std::vector<std::unique_ptr<Chunk>> chunks;
	v3s16 min_pos;
	v3s16 volume;
};

void ClientMapNoRender::touchMapBlocks()
{
	if (m_control.range_all)
		return;

	ScopeProfiler sp(g_profiler, "CM::touchMapBlocks()", SPT_AVG);

	v3s16 cam_pos_nodes = floatToInt(m_camera_position, BS);

	v3s16 p_blocks_min;
	v3s16 p_blocks_max;
	getBlocksInViewRange(cam_pos_nodes, &p_blocks_min, &p_blocks_max);

	// Number of blocks currently loaded by the client
	u32 blocks_loaded = 0;
	// Number of blocks with mesh in rendering range
	u32 blocks_in_range_with_mesh = 0;

	for (const auto& sector_it : m_sectors) {
		const MapSector* sector = sector_it.second;
		v2s16 sp = sector->getPos();

		blocks_loaded += sector->size();
		if (!m_control.range_all) {
			if (sp.X < p_blocks_min.X || sp.X > p_blocks_max.X ||
				sp.Y < p_blocks_min.Z || sp.Y > p_blocks_max.Z)
				continue;
		}

		/*
			Loop through blocks in sector
		*/

		for (const auto& entry : sector->getBlocks()) {
			MapBlock* block = entry.second.get();
			MapBlockMesh* mesh = block->mesh.get();

			// Calculate the coordinates for range and frustum culling
			v3f mesh_sphere_center;
			f32 mesh_sphere_radius;

			v3s16 block_pos_nodes = block->getPosRelative();

			if (mesh) {
				mesh_sphere_center = intToFloat(block_pos_nodes, BS)
					+ mesh->getBoundingSphereCenter();
				mesh_sphere_radius = mesh->getBoundingRadius();
			}
			else {
				mesh_sphere_center = intToFloat(block_pos_nodes, BS)
					+ v3f((MAP_BLOCKSIZE * 0.5f - 0.5f) * BS);
				mesh_sphere_radius = 0.0f;
			}

			// First, perform a simple distance check.
			if (!m_control.range_all &&
				mesh_sphere_center.getDistanceFrom(m_camera_position) >
				m_control.wanted_range * BS + mesh_sphere_radius)
				continue; // Out of range, skip.

			// Keep the block alive as long as it is in range.
			block->resetUsageTimer();
			blocks_in_range_with_mesh++;
		}
	}

	g_profiler->avg("MapBlock meshes in range [#]", blocks_in_range_with_mesh);
	g_profiler->avg("MapBlocks loaded [#]", blocks_loaded);
}

void ClientMapNoRender::PrintInfo(std::ostream& out)
{
	out << "ClientMapNoRender: ";
}

void ClientMapNoRender::reportMetrics(u64 save_time_us, u32 saved_blocks, u32 all_blocks)
{
	g_profiler->avg("CM::reportMetrics loaded blocks [#]", all_blocks);
}
