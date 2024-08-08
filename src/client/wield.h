#pragma once

#include "irrlichttypes_extrabloated.h"
#include "wieldmesh.h"
#include "inventory.h"

class RenderingEngine;
class Camera;
class LocalPlayer;
class WieldMeshSceneNode;

class Wield
{
public:
	Wield(RenderingEngine *rendering_engine, Client *client, Camera* camera);
	~Wield();

	// Draw the wielded tool.
	// This has to happen *after* the main scene is drawn.
	// Warning: This clears the Z buffer.
	void drawWieldedTool(irr::core::matrix4* translation=NULL);
	void update(LocalPlayer* player, f32 dtime, f32 tool_reload_ratio);
	
	// Start digging animation
	// Pass 0 for left click, 1 for right click
	void setDigging(s32 button);
	// Replace the wielded item mesh
	void set_item(const ItemStack &item);

private:
	void addArmInertia(f32 player_yaw);

private:
	scene::ISceneManager *m_wieldmgr = nullptr;
	WieldMeshSceneNode *m_wieldnode = nullptr;

	Client* m_client;
	Camera* m_camera;

	v2f m_wieldmesh_offset = v2f(55.0f, -35.0f);

	v2f m_arm_dir;
	v2f m_cam_vel_old;
	v2f m_last_cam_pos;

	// Digging animation frame (0 <= m_digging_anim < 1)
	f32 m_digging_anim = 0.0f;

	// Animation when changing wielded item
	f32 m_wield_change_timer = 0.125f;
	ItemStack m_wield_item_next;

	bool m_arm_inertia;

	// If -1, no digging animation
	// If 0, left-click digging animation
	// If 1, right-click digging animation
	s32 m_digging_button = -1;
};
