#include "wield.h"
#include "localplayer.h"
#include "util/numeric.h"
#include "client/renderingengine.h"
#include "noise.h"         // easeCurve
#include "client/client.h"
#include "mtevent.h"
#include "camera.h"
#include "script/scripting_client.h"

#define WIELDMESH_OFFSET_X 55.0f
#define WIELDMESH_OFFSET_Y -35.0f
#define WIELDMESH_AMPLITUDE_X 7.0f
#define WIELDMESH_AMPLITUDE_Y 10.0f

// Returns the fractional part of x
inline f32 my_modf(f32 x)
{
	float dummy;
	return std::modf(x, &dummy);
}

static inline v2f dir(const v2f &pos_dist)
{
	f32 x = pos_dist.X - WIELDMESH_OFFSET_X;
	f32 y = pos_dist.Y - WIELDMESH_OFFSET_Y;

	f32 x_abs = std::fabs(x);
	f32 y_abs = std::fabs(y);

	if (x_abs >= y_abs) {
		y *= (1.0f / x_abs);
		x /= x_abs;
	}

	if (y_abs >= x_abs) {
		x *= (1.0f / y_abs);
		y /= y_abs;
	}

	return v2f(std::fabs(x), std::fabs(y));
}

Wield::Wield(RenderingEngine *rendering_engine, Client *client, Camera* camera) : m_client(client), m_camera(camera) {
	auto smgr = rendering_engine->get_scene_manager();

	// This needs to be in its own scene manager. It is drawn after
	// all other 3D scene nodes and before the GUI.
	m_wieldmgr = smgr->createNewSceneManager();
	m_wieldmgr->addCameraSceneNode();
	m_wieldnode = new WieldMeshSceneNode(m_wieldmgr, -1, false);
	m_wieldnode->setItem(ItemStack(), m_client);
	m_wieldnode->drop(); // m_wieldmgr grabbed it

	m_arm_inertia               = g_settings->getBool("arm_inertia");
}

Wield::~Wield() {
	m_wieldmgr->drop();
}

void Wield::update(LocalPlayer* player, f32 dtime, f32 tool_reload_ratio)
{
	const v3f default_wield_position = v3f(m_wieldmesh_offset.X, m_wieldmesh_offset.Y, 65);
	const v3f default_wield_rotation = v3f(-100, 120, -100);
	// Position the wielded item
	v3f wield_position = default_wield_position;
	v3f wield_rotation = default_wield_rotation;
	wield_position.Y += std::abs(m_wield_change_timer)*320 - 40;
	if(m_digging_anim < 0.05 || m_digging_anim > 0.5)
	{
		f32 frac = 1.0;
		if(m_digging_anim > 0.5)
			frac = 2.0 * (m_digging_anim - 0.5);
		// This value starts from 1 and settles to 0
		f32 ratiothing = std::pow((1.0f - tool_reload_ratio), 0.5f);
		f32 ratiothing2 = (easeCurve(ratiothing*0.5))*2.0;
		wield_position.Y -= frac * 25.0f * std::pow(ratiothing2, 1.7f);
		wield_position.X -= frac * 35.0f * std::pow(ratiothing2, 1.1f);
		wield_rotation.Y += frac * 70.0f * std::pow(ratiothing2, 1.4f);
	}

	if (m_digging_button != -1)
	{
		f32 digfrac = m_digging_anim;
		wield_position.X -= 50 * std::sin(std::pow(digfrac, 0.8f) * M_PI);
		wield_position.Y += 24 * std::sin(digfrac * 1.8 * M_PI);
		wield_position.Z += 25 * 0.5;

		// Euler angles are PURE EVIL, so why not use quaternions?
		core::quaternion quat_begin(wield_rotation * core::DEGTORAD);
		core::quaternion quat_end(v3f(80, 30, 100) * core::DEGTORAD);
		core::quaternion quat_slerp;
		quat_slerp.slerp(quat_begin, quat_end, std::sin(digfrac * M_PI));
		quat_slerp.toEuler(wield_rotation);
		wield_rotation *= core::RADTODEG;
	} else {
		f32 bobfrac = my_modf(m_camera->get_view_bobbing_anim());
		wield_position.X -= std::sin(bobfrac*M_PI*2.0) * 3.0;
		wield_position.Y += std::sin(my_modf(bobfrac*2.0)*M_PI) * 3.0;
	}

	v3f script_wield_position = m_wieldnode->getPosition();
	v3f script_wield_rotation = m_wieldnode->getRotation();

	v3f world_script_wield_position;
	v3f world_script_wield_rotation;
	v3f camera_dir;
	{
		scene::ICameraSceneNode* cam = m_wieldmgr->getActiveCamera();
		matrix4 mat_to_screen = cam->getProjectionMatrix() * cam->getViewMatrix();
	
		matrix4 mat_screen_to_world = m_camera->getCameraNode()->getProjectionMatrix() * m_camera->getCameraNode()->getViewMatrix();
		matrix4 mat_screen_to_world_inv;
		mat_screen_to_world.getInverse(mat_screen_to_world_inv);

		//v3f cameraPos = m_camera->getPosition();

		v3f clipPos;
		mat_to_screen.transformVect(clipPos, script_wield_position);
		// Convert from clip space to NDC (Normalized Device Coordinates)
		if (clipPos.Z != 0.0f) {
			clipPos.X /= clipPos.Z;
			clipPos.Y /= clipPos.Z;
			clipPos.Z = 0.0f;
		}

		if (clipPos.X != clipPos.X)
			return;
		if (clipPos.Y != clipPos.Y)
			return;
		if (clipPos.Z != clipPos.Z)
			return;

		f32 vec[4];
		mat_screen_to_world_inv.transformVect(vec, clipPos);

		if (vec[3] != 0) {
			world_script_wield_position.X = (vec[0] / vec[3]) / BS;
			world_script_wield_position.Y = (vec[1] / vec[3]) / BS;
			world_script_wield_position.Z = (vec[2] / vec[3]) / BS;
		}

		if (world_script_wield_position.X != world_script_wield_position.X)
			return;
		if (world_script_wield_position.Y != world_script_wield_position.Y)
			return;
		if (world_script_wield_position.Z != world_script_wield_position.Z)
			return;

		v3s16 cam_offset = m_camera->getOffset();
		world_script_wield_position += v3f(cam_offset.X, cam_offset.Y, cam_offset.Z);

		world_script_wield_rotation = script_wield_rotation;
		matrix4 mat_inv_view;
		m_camera->getCameraNode()->getViewMatrix().getInverse(mat_inv_view);
		mat_inv_view.transformVect(world_script_wield_rotation);

		camera_dir = m_camera->getDirection();
	}
	m_wieldnode->setPosition(wield_position);
	m_wieldnode->setRotation(wield_rotation);
	
	v3f* ptr_script_wield_position = &script_wield_position;
	v3f* ptr_script_wield_rotation = &script_wield_rotation;

	if (m_client->getScript()->on_wield_animation(
		m_wield_item_next,
		dtime,
		&ptr_script_wield_position,
		&ptr_script_wield_rotation,	
		wield_position,
		wield_rotation,
		default_wield_position,
		default_wield_rotation,
		world_script_wield_position,
		world_script_wield_rotation,
		camera_dir)) {

		if (ptr_script_wield_position)
			m_wieldnode->setPosition(script_wield_position);
		if (ptr_script_wield_rotation)
			m_wieldnode->setRotation(script_wield_rotation);
	}

	m_wieldnode->setNodeLightColor(player->light_color);

	//
	// Arm inertia
	if (m_arm_inertia)
		addArmInertia(player->getYaw());

	//
	// wield_change
	//
	bool was_under_zero = m_wield_change_timer < 0;
	m_wield_change_timer = MYMIN(m_wield_change_timer + dtime, 0.125);

	if (m_wield_change_timer >= 0 && was_under_zero) {
		m_wieldnode->setItem(m_wield_item_next, m_client);
	}

	if (m_digging_button != -1) {
		f32 offset = dtime * 3.5f;
		float m_digging_anim_was = m_digging_anim;
		m_digging_anim += offset;
		if (m_digging_anim >= 1)
		{
			m_digging_anim = 0;
			m_digging_button = -1;
		}
		float lim = 0.15;
		if(m_digging_anim_was < lim && m_digging_anim >= lim)
		{
			if (m_digging_button == 0) {
				m_client->getEventManager()->put(new SimpleTriggerEvent(MtEvent::CAMERA_PUNCH_LEFT));
			} else if(m_digging_button == 1) {
				m_client->getEventManager()->put(new SimpleTriggerEvent(MtEvent::CAMERA_PUNCH_RIGHT));
			}
		}
	}
}


void Wield::addArmInertia(f32 player_yaw)
{
	v3f m_camera_direction = m_camera->getDirection();

	v2f m_cam_vel;

	m_cam_vel.X = std::fabs(rangelim(m_last_cam_pos.X - player_yaw,
		-100.0f, 100.0f) / 0.016f) * 0.01f;
	m_cam_vel.Y = std::fabs((m_last_cam_pos.Y - m_camera_direction.Y) / 0.016f);
	f32 gap_X = std::fabs(WIELDMESH_OFFSET_X - m_wieldmesh_offset.X);
	f32 gap_Y = std::fabs(WIELDMESH_OFFSET_Y - m_wieldmesh_offset.Y);

	if (m_cam_vel.X > 1.0f || m_cam_vel.Y > 1.0f) {
		/*
		    The arm moves relative to the camera speed,
		    with an acceleration factor.
		*/

		if (m_cam_vel.X > 1.0f) {
			if (m_cam_vel.X > m_cam_vel_old.X)
				m_cam_vel_old.X = m_cam_vel.X;

			f32 acc_X = 0.12f * (m_cam_vel.X - (gap_X * 0.1f));
			m_wieldmesh_offset.X += m_last_cam_pos.X < player_yaw ? acc_X : -acc_X;

			if (m_last_cam_pos.X != player_yaw)
				m_last_cam_pos.X = player_yaw;

			m_wieldmesh_offset.X = rangelim(m_wieldmesh_offset.X,
				WIELDMESH_OFFSET_X - (WIELDMESH_AMPLITUDE_X * 0.5f),
				WIELDMESH_OFFSET_X + (WIELDMESH_AMPLITUDE_X * 0.5f));
		}

		if (m_cam_vel.Y > 1.0f) {
			if (m_cam_vel.Y > m_cam_vel_old.Y)
				m_cam_vel_old.Y = m_cam_vel.Y;

			f32 acc_Y = 0.12f * (m_cam_vel.Y - (gap_Y * 0.1f));
			m_wieldmesh_offset.Y +=
				m_last_cam_pos.Y > m_camera_direction.Y ? acc_Y : -acc_Y;

			if (m_last_cam_pos.Y != m_camera_direction.Y)
				m_last_cam_pos.Y = m_camera_direction.Y;

			m_wieldmesh_offset.Y = rangelim(m_wieldmesh_offset.Y,
				WIELDMESH_OFFSET_Y - (WIELDMESH_AMPLITUDE_Y * 0.5f),
				WIELDMESH_OFFSET_Y + (WIELDMESH_AMPLITUDE_Y * 0.5f));
		}

		m_arm_dir = dir(m_wieldmesh_offset);
	} else {
		/*
		    Now the arm gets back to its default position when the camera stops,
		    following a vector, with a smooth deceleration factor.
		*/

		f32 dec_X = 0.35f * (std::min(15.0f, m_cam_vel_old.X) * (1.0f +
			(1.0f - m_arm_dir.X))) * (gap_X / 20.0f);

		f32 dec_Y = 0.25f * (std::min(15.0f, m_cam_vel_old.Y) * (1.0f +
			(1.0f - m_arm_dir.Y))) * (gap_Y / 15.0f);

		if (gap_X < 0.1f)
			m_cam_vel_old.X = 0.0f;

		m_wieldmesh_offset.X -=
			m_wieldmesh_offset.X > WIELDMESH_OFFSET_X ? dec_X : -dec_X;

		if (gap_Y < 0.1f)
			m_cam_vel_old.Y = 0.0f;

		m_wieldmesh_offset.Y -=
			m_wieldmesh_offset.Y > WIELDMESH_OFFSET_Y ? dec_Y : -dec_Y;
	}
}

void Wield::setDigging(s32 button)
{
	if (m_digging_button == -1)
		m_digging_button = button;
}

void Wield::set_item(const ItemStack &item)
{
	if (item.name != m_wield_item_next.name ||
			item.metadata != m_wield_item_next.metadata) {

		m_wield_item_next = item;
		if (m_wield_change_timer > 0)
			m_wield_change_timer = -m_wield_change_timer;
		else if (m_wield_change_timer == 0)
			m_wield_change_timer = -0.001;
	}
}

void Wield::drawWieldedTool(irr::core::matrix4* translation)
{
	scene::ICameraSceneNode* m_cameranode = m_camera->getCameraNode();

	// Clear Z buffer so that the wielded tool stays in front of world geometry
	m_wieldmgr->getVideoDriver()->clearBuffers(video::ECBF_DEPTH);

	// Draw the wielded node (in a separate scene manager)
	scene::ICameraSceneNode* cam = m_wieldmgr->getActiveCamera();
	cam->setAspectRatio(m_cameranode->getAspectRatio());
	cam->setFOV(72.0*M_PI/180.0);
	cam->setNearValue(10);
	cam->setFarValue(1000);
	if (translation != NULL)
	{
		irr::core::matrix4 startMatrix = cam->getAbsoluteTransformation();
		irr::core::vector3df focusPoint = (cam->getTarget()
				- cam->getAbsolutePosition()).setLength(1)
				+ cam->getAbsolutePosition();

		irr::core::vector3df camera_pos =
				(startMatrix * *translation).getTranslation();
		cam->setPosition(camera_pos);
		cam->updateAbsolutePosition();
		cam->setTarget(focusPoint);
	}
	m_wieldmgr->drawAll();
}
