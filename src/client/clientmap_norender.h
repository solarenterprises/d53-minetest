#pragma once

#include "map.h"
#include "clientmap.h"
#include "tile.h"
#include <set>
#include <map>

class Client;

/*
	ClientMapNoRender
*/

class ClientMapNoRender : public Map
{
public:
	ClientMapNoRender(
		Client* client,
		MapDrawControl& control,
		s32 id
	);

	virtual ~ClientMapNoRender();

	bool maySaveBlocks() override
	{
		return false;
	}

	/*
		Forcefully get a sector from somewhere
	*/
	MapSector* emergeSector(v2s16 p) override;

	// @brief Calculate statistics about the map and keep the blocks alive
	void touchMapBlocks();

	// For debug printing
	void PrintInfo(std::ostream& out) override;

	const MapDrawControl& getControl() const { return m_control; }
	f32 getWantedRange() const { return m_control.wanted_range; }
	f32 getCameraFov() const { return m_camera_fov; }

	void onSettingChanged(const std::string& name);

	void getBlocksInViewRange(v3s16 cam_pos_nodes,
		v3s16* p_blocks_min, v3s16* p_blocks_max, float range = -1.0f);

protected:
	void reportMetrics(u64 save_time_us, u32 saved_blocks, u32 all_blocks) override;

private:
	Client* m_client;

	MapDrawControl& m_control;

	v3f m_camera_position = v3f(0, 0, 0);
	v3f m_camera_direction = v3f(0, 0, 1);
	f32 m_camera_fov = M_PI;
	v3s16 m_camera_offset;
};
