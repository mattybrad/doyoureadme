#include "daisy_seed.h"
#include "daisysp.h"
#include <stdint.h>

using namespace daisy;
using namespace daisysp;


DaisySeed hw;
int tempInt = 0;
uint8_t ledOut;
uint8_t currentBitMask = 0;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	float runningSum = 0;
	for (size_t i = 0; i < size; i++)
	{
		float left = in[0][i];
		float right = in[1][i];
		runningSum += left + right;
	}
	float average = runningSum / (size * 2);
	ledOut = static_cast<uint8_t>(average * 255 + 128);
	if(rand() % 10000 == 0) {
		currentBitMask = rand() % 256;
	}
	ledOut &= currentBitMask;
	
	out[0][0] = static_cast<float>(ledOut) / 255.0f - 0.5f;
	out[1][0] = static_cast<float>(ledOut) / 255.0f - 0.5f;
}

int main(void)
{
	hw.Init();
	//hw.StartLog(true);
	//hw.PrintLine("Initializing hardware...");
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

	hw.StartAudio(AudioCallback);
	while(1) {}
}
