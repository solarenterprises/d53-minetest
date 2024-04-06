#pragma once

#include "localplayer.h"

class PlayerAI {
public:
	PlayerAI(LocalPlayer* local_player);

	void step(float dtime);

private:
	LocalPlayer* local_player;
};
