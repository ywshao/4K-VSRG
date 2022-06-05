#pragma once

#include <portaudio.h>	// Have the ability to select low-latency driver
#include <sndfile.h>
#include <vector>
#include <list>

#define soundVolume 0.2
#define maxSound 1536

class Audio {
private:
	struct Sound {
		std::vector<float> left;
		std::vector<float> right;
	};
	struct PlayingSound {
		PlayingSound(Sound *sound) : playing_pos(0) {
			this->sound = sound;
		}
		Sound *sound;
		int playing_pos;
	};
	static int callback(const void *inputBuffer,
		void *outputBuffer,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo *timeInfo,
		PaStreamCallbackFlags statusFlags,
		void *userData);
	PaStream* stream;
	const int framesPerBuffer = 256;
	std::list<PlayingSound> playingSounds;
	Sound sound[maxSound];
public:
	void portAudioInit(PaDeviceIndex device);
	void portAudioExit();
	void playSound(int index);
	void loadSound(int index, const char *path);
	void offloadSound(int index);
	void stopSound();
	int getCurrentSoundNum();
};
