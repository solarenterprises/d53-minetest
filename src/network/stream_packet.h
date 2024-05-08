#pragma once

#include "../util/stream.h"
#include "irrlichttypes.h"
#include "connection.h"
#include "networkpacket.h"

class Client;
class Server;

class NetworkStreamPacket : public Stream, public NetworkPacket
{
public:
	NetworkStreamPacket(u16 command, size_t max_packet_size = 1024);

	void init(const char* data, std::streamsize size);
	virtual void write(const char* data, std::streamsize size) override;
	virtual void flush() override;

	inline const u32& get_id() {
		return id;
	}

protected:
	virtual void send(NetworkPacket* packet) = 0;

private:
	void write_and_send(bool eof);

private:
	u16 command;
	u32 id;
	u16 num_packet = 0;
	size_t max_packet_size;
	bool is_flushed = false;
};

class StreamPacketHandler {
public:
	typedef std::function<void(session_t peer_id, u32 id, u16 chunk_id, NetworkPacket* networkPacket, void* user_data)> HandleCallback;

public:
	StreamPacketHandler() = default;

	void handle(NetworkPacket* packet, HandleCallback& callback);
	void erase_session(session_t peer_id);
	void set_user_data(session_t peer_id, u32 id, void* user_data);

private:
	 void flush_chunks(session_t peer_id, u32 chain_id, HandleCallback& callback);

private:
	struct PacketChain {
		u32 id = 0;
		bool eof = false;
		std::vector<NetworkPacket*> chunks;
		u16 chunks_flushed = 0;

		void* user_data = nullptr;

		~PacketChain();
	};

	typedef std::unordered_map<u32, PacketChain*> PeerPacketsChains;

	std::unordered_map<session_t, PeerPacketsChains*> chains;
};

class StreamPacketClient : public NetworkStreamPacket {
public:
	StreamPacketClient(Client* client, u16 command, size_t max_packet_size = 1024);

protected:
	virtual void send(NetworkPacket* packet) override;

private:
	Client* client;
};

class StreamPacketServer : public NetworkStreamPacket {
public:
	StreamPacketServer(Server* server, std::vector<session_t>& peer_ids, u16 command, size_t max_packet_size = 1024);

protected:
	virtual void send(NetworkPacket* packet) override;

private:
	Server* server;

	std::vector<session_t> peer_ids;
};
