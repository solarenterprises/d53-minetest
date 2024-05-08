#include "stream_packet.h"
#include "../server.h"
#include "../client/client.h"

StreamPacketHandler::PacketChain::~PacketChain() {
	for (auto c : chunks)
		delete c;
	chunks.clear();
}

//
// Stream packet
//
u32 streamPacketId = 0;
NetworkStreamPacket::NetworkStreamPacket(u16 command, size_t max_packet_size) {
	this->id = streamPacketId++;
	this->command = command;
	this->max_packet_size = max_packet_size;
}

void NetworkStreamPacket::init(const char* data, std::streamsize size) {
	if (is_flushed)
		return;

	if (size >= max_packet_size)
		throw "init too large";

	write(data, size);
	write_and_send(false);
}

void NetworkStreamPacket::write(const char* data, std::streamsize size) {
	if (is_flushed)
		return;

	putRawString(data, size);

	while (getSize() >= max_packet_size)
		write_and_send(false);
}

void NetworkStreamPacket::flush() {
	if (is_flushed)
		return;

	while (getSize())
		write_and_send(getSize() <= max_packet_size);

	is_flushed = true;
}

void NetworkStreamPacket::write_and_send(bool eof) {
	u16 send_size = std::min((size_t)getSize(), max_packet_size);
	NetworkPacket packet(command, sizeof(u32)+sizeof(1)+sizeof(u16)+send_size);
	packet << id;
	packet << eof;
	packet << num_packet;
	packet.putRawString(substring(send_size));

	num_packet++;

	send(&packet);
}

void StreamPacketHandler::handle(NetworkPacket* _in_packet, StreamPacketHandler::HandleCallback& callback) {
	NetworkPacket* packet = new NetworkPacket(*_in_packet);

	u32 id;
	(*packet) >> id;
	bool eof;
	(*packet) >> eof;
	u16 num_packet;
	(*packet) >> num_packet;

	session_t peer_id = packet->getPeerId();
	
	if (chains.find(peer_id) == chains.end())
		chains[peer_id] = new PeerPacketsChains();

	PeerPacketsChains* session = chains[peer_id];
	PacketChain* chain;
	if (session->find(id) == session->end()) {
		chain = new PacketChain();
		chain->id = id;
		chain->eof = eof;
		(*session)[id] = chain;
	}
	else
		chain = session->at(id);

	chain->eof |= eof;

	if (chain->chunks.size() <= num_packet+1)
		chain->chunks.resize(num_packet+1);

	chain->chunks[num_packet] = packet;

	flush_chunks(peer_id, id, callback);
}

void StreamPacketHandler::flush_chunks(session_t peer_id, u32 chain_id, StreamPacketHandler::HandleCallback& callback) {
	PeerPacketsChains* session = chains[peer_id];
	PacketChain* chain = session->at(chain_id);
	for (; chain->chunks_flushed < chain->chunks.size(); chain->chunks_flushed++) {
		NetworkPacket* chunk = chain->chunks[chain->chunks_flushed];
		if (!chunk)
			return;

		callback(peer_id, chain_id, chain->chunks_flushed, chunk, chain->user_data);
		delete chunk;
		chain->chunks[chain->chunks_flushed] = nullptr;
	}

	if (!chain->eof || chain->chunks_flushed < chain->chunks.size())
		return;

	callback(peer_id, chain_id, chain->chunks_flushed, nullptr, chain->user_data);

	//
	// Done. Cleanup.
	delete chain;
	session->erase(chain_id);
}

void StreamPacketHandler::erase_session(session_t peer_id) {
	if (chains.find(peer_id) == chains.end())
		return;

	delete chains[peer_id];
	chains.erase(peer_id);
}

void StreamPacketHandler::set_user_data(session_t peer_id, u32 id, void* user_data) {
	if (chains.find(peer_id) == chains.end())
		return;

	chains[peer_id]->at(id)->user_data = user_data;
}

//
// Server
//
StreamPacketServer::StreamPacketServer(Server* server, std::vector<session_t>& peer_ids, u16 command, size_t max_packet_size) : NetworkStreamPacket(command, max_packet_size) {
	this->server = server;
	this->peer_ids = std::move(peer_ids);
}

void StreamPacketServer::send(NetworkPacket* packet) {
	for (auto peer_id : peer_ids)
		server->Send(peer_id, packet);
}

//
// Client
//
StreamPacketClient::StreamPacketClient(Client* client, u16 command, size_t max_packet_size) : NetworkStreamPacket(command, max_packet_size) {
	this->client = client;
}

void StreamPacketClient::send(NetworkPacket* packet) {
	client->Send(packet);
}

