/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#pragma once

#include "clientenvironment.h"
#include "irrlichttypes_extrabloated.h"
#include <ostream>
#include <map>
#include <memory>
#include <set>
#include <vector>
#include <unordered_set>
#include "clientobject.h"
#include "gamedef.h"
#include "inventorymanager.h"
#include "client/hud.h"
#include "tileanimation.h"
#include "network/address.h"
#include "network/peerhandler.h"
#include "gameparams.h"
#include "clientdynamicinfo.h"
#include <fstream>
#include "util/numeric.h"
#include "mesh_buffer_handler.h"

#define CLIENT_CHAT_MESSAGE_LIMIT_PER_10S 10.0f

struct ClientEvent;
struct MeshMakeData;
struct ChatMessage;
class MapBlock;
class MapBlockMesh;
class RenderingEngine;
class IWritableTextureSource;
class IWritableShaderSource;
class IWritableItemDefManager;
class ISoundManager;
class NodeDefManager;
//class IWritableCraftDefManager;
class ClientMediaDownloader;
class SingleMediaDownloader;
struct MapDrawControl;
class ModChannelMgr;
class MtEventManager;
struct PointedThing;
struct MapNode;
class MapDatabase;
class Minimap;
struct MinimapMapblock;
class MeshUpdateManager;
class ParticleManager;
class Camera;
struct PlayerControl;
class NetworkPacket;
struct FpsControl;
class InputHandler;
class StreamPacketHandler;

namespace con {
	class Connection;
}
using sound_handle_t = int;

enum LocalClientState {
	LC_Created,
	LC_Init,
	LC_Ready
};

/*
	Packet counter
*/

class PacketCounter
{
public:
	PacketCounter() = default;

	void add(u16 command)
	{
		auto n = m_packets.find(command);
		if (n == m_packets.end())
			m_packets[command] = 1;
		else
			n->second++;
	}

	void clear()
	{
		m_packets.clear();
	}

	u32 sum() const;
	void print(std::ostream& o) const;

private:
	// command, count
	std::map<u16, u32> m_packets;
};

class ClientScripting;
class GameUI;

class Client : public con::PeerHandler, public InventoryManager, public IGameDef
{
public:
	/*
		NOTE: Nothing is thread-safe here.
	*/

	Client(
			const char *playername,
			const std::string &password,
			const std::string ai_class,
			const std::string token,
			InputHandler* input,
			MapDrawControl &control,
			IWritableTextureSource *tsrc,
			IWritableShaderSource *shsrc,
			IWritableItemDefManager *itemdef,
			NodeDefManager *nodedef,
			ISoundManager *sound,
			MtEventManager *event,
			RenderingEngine *rendering_engine,
			GameUI *game_ui,
			ELoginRegister allow_login_or_register
	);

	~Client();
	DISABLE_CLASS_COPY(Client);

	// Load local mods into memory
	void scanModSubfolder(const std::string& mod_name, const std::string& mod_path,
		std::string mod_subpath);
	inline void scanModIntoMemory(const std::string& mod_name, const std::string& mod_path)
	{
		scanModSubfolder(mod_name, mod_path, "");
	}

	/*
	 request all threads managed by client to be stopped
	 */
	void Stop();


	bool isShutdown();

	void connect(const Address &address, const std::string &address_name,
		bool is_local_server);

	/*
		Stuff that references the environment is valid only as
		long as this is not called. (eg. Players)
		If this throws a PeerNotFoundException, the connection has
		timed out.
	*/
	void step(float dtime);

	/*
	 * Command Handlers
	 */

	void handleCommand(NetworkPacket* pkt);

	void handleCommand_Null(NetworkPacket* pkt) {};
	void handleCommand_Deprecated(NetworkPacket* pkt);
	void handleCommand_Hello(NetworkPacket* pkt);
	void handleCommand_AuthAccept(NetworkPacket* pkt);
	void handleCommand_AcceptSudoMode(NetworkPacket* pkt);
	void handleCommand_DenySudoMode(NetworkPacket* pkt);
	void handleCommand_AccessDenied(NetworkPacket* pkt);
	void handleCommand_RemoveNode(NetworkPacket* pkt);
	void handleCommand_AddNode(NetworkPacket* pkt);
	void handleCommand_NodemetaChanged(NetworkPacket* pkt);
	void handleCommand_BlockData(NetworkPacket* pkt);
	void handleCommand_Inventory(NetworkPacket* pkt);
	void handleCommand_TimeOfDay(NetworkPacket* pkt);
	void handleCommand_ChatMessage(NetworkPacket* pkt);
	void handleCommand_ActiveObjectRemoveAdd(NetworkPacket* pkt);
	void handleCommand_ActiveObjectMessages(NetworkPacket* pkt);
	void handleCommand_Movement(NetworkPacket* pkt);
	void handleCommand_Fov(NetworkPacket* pkt);
	void handleCommand_HP(NetworkPacket* pkt);
	void handleCommand_Breath(NetworkPacket* pkt);
	void handleCommand_MovePlayer(NetworkPacket* pkt);
	void handleCommand_MovePlayerRel(NetworkPacket* pkt);
	void handleCommand_DeathScreen(NetworkPacket* pkt);
	void handleCommand_AnnounceMedia(NetworkPacket* pkt);
	void handleCommand_Media(NetworkPacket* pkt);
	void handleCommand_NodeDef(NetworkPacket* pkt);
	void handleCommand_ItemDef(NetworkPacket* pkt);
	void handleCommand_PlaySound(NetworkPacket* pkt);
	void handleCommand_StopSound(NetworkPacket* pkt);
	void handleCommand_FadeSound(NetworkPacket* pkt);
	void handleCommand_Privileges(NetworkPacket* pkt);
	void handleCommand_InventoryFormSpec(NetworkPacket* pkt);
	void handleCommand_DetachedInventory(NetworkPacket* pkt);
	void handleCommand_ShowFormSpec(NetworkPacket* pkt);
	void handleCommand_SpawnParticle(NetworkPacket* pkt);
	void handleCommand_AddParticleSpawner(NetworkPacket* pkt);
	void handleCommand_DeleteParticleSpawner(NetworkPacket* pkt);
	void handleCommand_HudAdd(NetworkPacket* pkt);
	void handleCommand_HudRemove(NetworkPacket* pkt);
	void handleCommand_HudChange(NetworkPacket* pkt);
	void handleCommand_HudSetFlags(NetworkPacket* pkt);
	void handleCommand_HudSetParam(NetworkPacket* pkt);
	void handleCommand_HudSetSky(NetworkPacket* pkt);
	void handleCommand_HudSetSun(NetworkPacket* pkt);
	void handleCommand_HudSetMoon(NetworkPacket* pkt);
	void handleCommand_HudSetStars(NetworkPacket* pkt);
	void handleCommand_CloudParams(NetworkPacket* pkt);
	void handleCommand_OverrideDayNightRatio(NetworkPacket* pkt);
	void handleCommand_LocalPlayerAnimations(NetworkPacket* pkt);
	void handleCommand_EyeOffset(NetworkPacket* pkt);
	void handleCommand_UpdatePlayerList(NetworkPacket* pkt);
	void handleCommand_ModChannelMsg(NetworkPacket* pkt);
	void handleCommand_ModChannelSignal(NetworkPacket* pkt);
	void handleCommand_SrpBytesSandB(NetworkPacket* pkt);
	void handleCommand_FormspecPrepend(NetworkPacket* pkt);
	void handleCommand_CSMRestrictionFlags(NetworkPacket* pkt);
	void handleCommand_PlayerSpeed(NetworkPacket* pkt);
	void handleCommand_MediaPush(NetworkPacket* pkt);
	void handleCommand_MinimapModes(NetworkPacket* pkt);
	void handleCommand_SetLighting(NetworkPacket* pkt);
	void handleCommand_Lua_Packet(NetworkPacket* pkt);
	void handleCommand_Lua_Packet_Stream(NetworkPacket* pkt);

	void ProcessData(NetworkPacket* pkt);

	void Send(NetworkPacket* pkt);

	void interact(InteractAction action, const PointedThing& pointed);

	void sendNodemetaFields(v3s16 p, const std::string& formname,
		const StringMap& fields);
	void sendInventoryFields(const std::string& formname,
		const StringMap& fields);
	void sendInventoryAction(InventoryAction* a);
	void sendChatMessage(const std::wstring& message);
	void clearOutChatQueue();
	void sendChangePassword(const std::string& oldpassword,
		const std::string& newpassword);
	void sendDamage(u16 damage);
	void sendRespawn();
	void sendReady();
	void sendHaveMedia(const std::vector<u32> &tokens);
	void sendUpdateClientInfo(const ClientDynamicInfo &info);

	ClientEnvironment& getEnv() { return m_env; }
	ITextureSource* tsrc() { return getTextureSource(); }
	ISoundManager* sound() { return getSoundManager(); }
	static const std::string& getBuiltinLuaPath();
	static const std::string& getClientModsLuaPath();

	const std::vector<ModSpec>& getMods() const override;
	const ModSpec* getModSpec(const std::string& modname) const override;

	// Causes urgent mesh updates (unlike Map::add/removeNodeWithEvent)
	void removeNode(v3s16 p);

	// helpers to enforce CSM restrictions
	MapNode CSMGetNode(v3s16 p, bool* is_valid_position);
	int CSMClampRadius(v3s16 pos, int radius);
	v3s16 CSMClampPos(v3s16 pos);

	void addNode(v3s16 p, MapNode n, bool remove_metadata = true);

	void setPlayerControl(PlayerControl& control);

	// Returns true if the inventory of the local player has been
	// updated from the server. If it is true, it is set to false.
	bool updateWieldedItem();

	/* InventoryManager interface */
	Inventory* getInventory(const InventoryLocation& loc) override;
	void inventoryAction(InventoryAction* a) override;

	// Send the item number 'item' as player item to the server
	void setPlayerItem(u16 item);

	const std::set<std::string> &getConnectedPlayerNames()
	{
		return m_env.getPlayerNames();
	}

	float getAnimationTime();

	int getCrackLevel();
	v3s16 getCrackPos();
	void setCrack(int level, v3s16 pos);
	std::unordered_map<v3s16, int>& getCracks() { return cracks; }

	u16 getHP();

	bool checkPrivilege(const std::string& priv) const
	{
		return (m_privileges.count(priv) != 0);
	}

	const std::unordered_set<std::string>& getPrivilegeList() const
	{
		return m_privileges;
	}

	bool getChatMessage(std::wstring& message);
	void typeChatMessage(const std::wstring& message);

	u64 getMapSeed() { return m_map_seed; }

	void addUpdateMeshTask(v3s16 blockpos, bool ack_to_server = false, bool urgent = false);
	// Including blocks at appropriate edges
	void addUpdateMeshTaskWithEdge(v3s16 blockpos, bool ack_to_server = false, bool urgent = false);
	void addUpdateMeshTaskForNode(v3s16 nodepos, bool ack_to_server = false, bool urgent = false);

	void updateCameraOffset(v3s16 camera_offset);

	bool hasClientEvents() const { return !m_client_event_queue.empty(); }
	// Get event from queue. If queue is empty, it triggers an assertion failure.
	ClientEvent* getClientEvent();

	bool accessDenied() const { return m_access_denied; }

	bool reconnectRequested() const { return m_access_denied_reconnect; }

	void setFatalError(const std::string& reason)
	{
		m_access_denied = true;
		m_access_denied_reason = reason;
	}
	inline void setFatalError(const LuaError& e)
	{
		setFatalError(std::string("Lua: ") + e.what());
	}

	// Renaming accessDeniedReason to better name could be good as it's used to
	// disconnect client when CSM failed.
	const std::string& accessDeniedReason() const { return m_access_denied_reason; }

	bool itemdefReceived() const
	{
		return m_itemdef_received;
	}
	bool nodedefReceived() const
	{
		return m_nodedef_received;
	}
	bool mediaReceived() const
	{
		return !m_media_downloader;
	}
	bool activeObjectsReceived() const
	{
		return m_activeobjects_received;
	}

	u16 getProtoVersion() const
	{ return m_proto_ver; }

	bool m_simple_singleplayer_mode;

	float mediaReceiveProgress();

	void afterContentReceived();
	void showUpdateProgressTexture(void* args, u32 progress, u32 max_progress);

	float getRTT();
	float getCurRate();
	// has the server ever replied to us, used for connection retry/fallback
	bool hasServerReplied() const {
		return getProtoVersion() != 0; // (set in TOCLIENT_HELLO)
	}

	FpsControl& getFpsControl();

	Minimap* getMinimap() { return m_minimap; }
	void setCamera(Camera* camera) { m_camera = camera; }

	Camera* getCamera() { return m_camera; }
	scene::ISceneManager* getSceneManager();

	// IGameDef interface
	IItemDefManager* getItemDefManager() override;
	const NodeDefManager* getNodeDefManager() override;
	ICraftDefManager* getCraftDefManager() override;
	ITextureSource* getTextureSource();
	virtual IWritableShaderSource* getShaderSource();
	u16 allocateUnknownNodeId(const std::string& name) override;
	virtual ISoundManager* getSoundManager();
	MtEventManager* getEventManager();
	virtual ParticleManager* getParticleManager();
	bool checkLocalPrivilege(const std::string& priv)
	{
		return checkPrivilege(priv);
	}
	virtual scene::IAnimatedMesh* getMesh(const std::string& filename, bool cache = false);
	const std::string* getModFile(std::string filename);
	ModStorageDatabase *getModStorageDatabase() override { return m_mod_storage_database; }

	// Migrates away old files-based mod storage if necessary
	void migrateModStorage();

	// Mods that was sent by server to client
	void cleanupServerMods();

	// The following set of functions is used by ClientMediaDownloader
	// Insert a media file appropriately into the appropriate manager
	bool loadMedia(const std::string& data, const std::string& filename,
		bool from_media_push = false);

	// Send a request for conventional media transfer
	void request_media(const std::vector<std::string>& file_requests);

	LocalClientState getState() { return m_state; }

	void makeScreenshot();

	inline void pushToChatQueue(ChatMessage* cec)
	{
		m_chat_queue.push(cec);
	}

	ClientScripting *getScript() { return m_script; }
	bool modsLoaded() const { return m_mods_loaded; }

	void pushToEventQueue(ClientEvent* event);

	inline InputHandler* getInputHandler() {
		return m_input;
	}
	std::string input_key_to_id_string(int index);
	void register_input_key(std::string id, std::string display_name, std::string defaultKey);

	// IP and port we're connected to
	const Address getServerAddress();

	// Hostname of the connected server (but can also be a numerical IP)
	const std::string &getAddressName() const
	{
		return m_address_name;
	}

	inline u64 getCSMRestrictionFlags() const
	{
		return m_csm_restriction_flags;
	}

	inline bool checkCSMRestrictionFlag(CSMRestrictionFlags flag) const
	{
		return m_csm_restriction_flags & flag;
	}

	bool joinModChannel(const std::string& channel) override;
	bool leaveModChannel(const std::string& channel) override;
	bool sendModChannelMessage(const std::string& channel,
		const std::string& message) override;
	ModChannel* getModChannel(const std::string& channel) override;

	const std::string &getFormspecPrepend() const;

	inline MeshGrid getMeshGrid()
	{
		return m_mesh_grid;
	}

	bool inhibit_inventory_revert = false;

	struct SendGotBlocksData {
		v3s16 pos;
		u16 modified_version;
	};

public:
	std::unique_ptr<MeshBufferHandler> m_mesh_buffer_handler;

private:
	void loadMods();

	// Virtual methods from con::PeerHandler
	void peerAdded(con::Peer* peer) override;
	void deletingPeer(con::Peer* peer, bool timeout) override;

	void initLocalMapSaving(const Address& address,
		const std::string& hostname,
		bool is_local_server);

	void ReceiveAll();

	void sendPlayerPos();

	void deleteAuthData();
	// helper method shared with clientpackethandler
	static AuthMechanism choseAuthMech(const u32 mechs);

	void sendInit(const std::string &playerName);
	void startAuth(AuthMechanism chosen_auth_mechanism);
	void sendDeletedBlocks(std::vector<v3s16> &blocks);
	void sendGotBlocks(const std::vector<SendGotBlocksData> &blocks);
	void sendRemovedSounds(const std::vector<s32> &soundList);

	bool canSendChatMessage() const;

	float m_packetcounter_timer = 0.0f;
	float m_connection_reinit_timer = 0.1f;
	float m_avg_rtt_timer = 0.0f;
	float m_playerpos_send_timer = 0.0f;
	IntervalLimiter m_map_timer_and_unload_interval;

	IWritableTextureSource* m_tsrc;
	IWritableShaderSource* m_shsrc;
	IWritableItemDefManager* m_itemdef;
	NodeDefManager* m_nodedef;
	ISoundManager* m_sound;
	MtEventManager* m_event;
	RenderingEngine* m_rendering_engine;


	std::unique_ptr<MeshUpdateManager> m_mesh_update_manager;
	
	ClientEnvironment m_env;
	std::unique_ptr<ParticleManager> m_particle_manager;
	std::unique_ptr<con::Connection> m_con;
	std::string m_address_name;
	ELoginRegister m_allow_login_or_register = ELoginRegister::Any;
	Camera *m_camera = nullptr;
	Minimap *m_minimap = nullptr;

	// Server serialization version
	u8 m_server_ser_ver;

	// Used version of the protocol with server
	// Values smaller than 25 only mean they are smaller than 25,
	// and aren't accurate. We simply just don't know, because
	// the server didn't send the version back then.
	// If 0, server init hasn't been received yet.
	u16 m_proto_ver = 0;

	bool m_update_wielded_item = false;
	Inventory* m_inventory_from_server = nullptr;
	float m_inventory_from_server_age = 0.0f;
	PacketCounter m_packetcounter;
	// Block mesh animation parameters
	float m_animation_time = 0.0f;
	int m_crack_level = -1;
	v3s16 m_crack_pos;
	std::unordered_map<v3s16, int> cracks;
	// 0 <= m_daynight_i < DAYNIGHT_CACHE_COUNT
	//s32 m_daynight_i;
	//u32 m_daynight_ratio;
	std::queue<std::wstring> m_out_chat_queue;
	u32 m_last_chat_message_sent;
	float m_chat_message_allowance = 5.0f;
	std::queue<ChatMessage*> m_chat_queue;

	// The authentication methods we can use to enter sudo mode (=change password)
	u32 m_sudo_auth_methods;

	// The seed returned by the server in TOCLIENT_INIT is stored here
	u64 m_map_seed = 0;

	// Auth data
	std::string m_playername;
	std::string m_password;
	std::string m_token;
	// If set, this will be sent (and cleared) upon a TOCLIENT_ACCEPT_SUDO_MODE
	std::string m_new_password;
	// Usable by auth mechanisms.
	AuthMechanism m_chosen_auth_mech;
	void* m_auth_data = nullptr;

	bool m_access_denied = false;
	bool m_access_denied_reconnect = false;
	std::string m_access_denied_reason = "";
	std::queue<ClientEvent*> m_client_event_queue;
	bool m_itemdef_received = false;
	bool m_nodedef_received = false;
	bool m_activeobjects_received = false;
	bool m_mods_loaded = false;
	bool m_load_mods_after_content_received = false;

	std::vector<std::string> m_remote_media_servers;
	// Media downloader, only exists during init
	ClientMediaDownloader *m_media_downloader;
	// Pending downloads of dynamic media (key: token)
	std::vector<std::pair<u32, std::shared_ptr<SingleMediaDownloader>>> m_pending_media_downloads;

	// time_of_day speed approximation for old protocol
	bool m_time_of_day_set = false;
	float m_last_time_of_day_f = -1.0f;
	float m_time_of_day_update_timer = 0.0f;

	// An interval for generally sending object positions and stuff
	float m_recommended_send_interval = 0.1f;

	// Sounds
	float m_removed_sounds_check_timer = 0.0f;
	// Mapping from server sound ids to our sound ids
	std::unordered_map<s32, sound_handle_t> m_sounds_server_to_client;
	// And the other way!
	// This takes ownership for the sound handles.
	std::unordered_map<sound_handle_t, s32> m_sounds_client_to_server;
	// Relation of client id to object id
	std::unordered_map<sound_handle_t, u16> m_sounds_to_objects;

	// Privileges
	std::unordered_set<std::string> m_privileges;

	// Detached inventories
	// key = name
	std::unordered_map<std::string, Inventory*> m_detached_inventories;

	// Storage for mesh data for creating multiple instances of the same mesh
	StringMap m_mesh_data;

	// own state
	LocalClientState m_state;

	GameUI* m_game_ui;

	// Used for saving server map to disk client-side
	MapDatabase* m_localdb = nullptr;
	IntervalLimiter m_localdb_save_interval;
	u16 m_cache_save_interval;

	// Client modding
	ClientScripting *m_script = nullptr;
	ModStorageDatabase *m_mod_storage_database = nullptr;
	float m_mod_storage_save_timer = 10.0f;
	std::vector<ModSpec> m_mods;
	StringMap m_mod_vfs;

	bool m_shutdown = false;

	// CSM restrictions byteflag
	u64 m_csm_restriction_flags = CSMRestrictionFlags::CSM_RF_NONE;
	u32 m_csm_restriction_noderange = 8;

	std::unique_ptr<ModChannelMgr> m_modchannel_mgr;

	std::unique_ptr<StreamPacketHandler> m_streamPacketHandler;

	// The number of blocks the client will combine for mesh generation.
	MeshGrid m_mesh_grid;

	FpsControl fps_control;

	InputHandler* m_input;
};
