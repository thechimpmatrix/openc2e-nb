#pragma once

#include "common/audio/AudioState.h"

#include <cstdint>

class Sound {
  public:
	Sound();
	operator bool();
	void fadeOut();
	void stop();
	void setPosition(float x, float y, float width, float height);
	AudioState getState();

  private:
	friend class SoundManager;
	uint32_t id = ~0;
};