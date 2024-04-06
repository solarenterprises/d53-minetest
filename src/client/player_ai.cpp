#include "player_ai.h"
#include "client.h"

PlayerAI::PlayerAI(LocalPlayer* local_player) {
	this->local_player = local_player;
}

void PlayerAI::step(float dtime) {
	if (!local_player)
		return;

	if (local_player->isDead())
		local_player->getClient()->sendRespawn();
}
