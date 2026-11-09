#include "daisy_seed.h"
#include "daisysp.h"
#include <stdint.h>

using namespace daisy;
using namespace daisysp;


DaisySeed hw;
const uint16_t maxBlockSize = 1024;
uint16_t audioBlockSize = 16;
uint16_t currentAlgorithm = 0;
float storedAudio[maxBlockSize];
uint8_t encodedAudio[maxBlockSize]; // don't think we'll get more data than this per block but TBC
float decodedAudio[maxBlockSize];
TimerHandle timer;
bool tempDoEncode = false;
uint encodedAudioIndex = maxBlockSize;

// declare functions, temp
void decodeAudio();
void timerCallback(void* data);

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	// store audio in current window
	for (size_t i = 0; i < size; i++)
	{
		storedAudio[i] = in[0][i];
	}

	decodeAudio();
	for (size_t i = 0; i < size; i++)
	{
		out[0][i] = decodedAudio[i];
		out[1][i] = decodedAudio[i];
	}

	encodedAudioIndex = 0;
}

void decodeAudio() {
	for(int i=0; i<audioBlockSize; i++) {
		decodedAudio[i] = ((float)encodedAudio[i] / 127.5f) - 1.0f;
	}
}

int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(audioBlockSize); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

	// start timer to simulate LED data transception
	TimerHandle::Config timerConfig;
	timerConfig.periph = TimerHandle::Config::Peripheral::TIM_5;
	timerConfig.enable_irq = true;
	const uint32_t timerHz = System::GetPClk2Freq();
    timerConfig.period = timerHz / 50000; // 50kHz currently, needs to come down to 2Khz or less for LED functionality
	timer.Init(timerConfig);
	timer.SetCallback(timerCallback);
	timer.Start();

	hw.StartAudio(AudioCallback);
	while(1) {}
}

bool ledStatus = false;
void timerCallback(void* data) {
	// if(tempDoEncode) {
	// 	for(int i=0; i<audioBlockSize; i++) {
	// 		encodedAudio[i] = (uint8_t)((storedAudio[i] + 1.0f) * 127.5f);
	// 	}
	// }
	if(encodedAudioIndex < audioBlockSize) {
		encodedAudio[encodedAudioIndex] = (uint8_t)((storedAudio[encodedAudioIndex] + 1.0f) * 127.5f);
	}
	encodedAudioIndex++;
}