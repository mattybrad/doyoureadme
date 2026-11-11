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
uint8_t encodedAudioToSend[maxBlockSize]; // don't think we'll get more data than this per block but TBC
uint8_t encodedAudioReceived[maxBlockSize];
float decodedAudio[maxBlockSize];
TimerHandle timer;
uint encodedAudioIndex = maxBlockSize;

// declare functions, temp
void encodeAudio();
void decodeAudio();
void timerCallback(void* data);

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	decodeAudio();
	for (size_t i = 0; i < size; i++)
	{
		out[0][i] = decodedAudio[i];
		out[1][i] = storedAudio[i];
	}
	
	// store audio in current window
	for (size_t i = 0; i < size; i++)
	{
		storedAudio[i] = in[0][i]; // could probably just pass "in" straight to encodeAudio() instead of copying
	}
	encodeAudio();
	encodedAudioIndex = 0;
}

void encodeAudio() {
	for(int i=0; i<audioBlockSize; i++) {
		encodedAudioToSend[i] = (((uint8_t)((storedAudio[i] + 1.0f) * 127.5f)) >> 4) << 4;
	}
}

void decodeAudio() {
	for(int i=0; i<audioBlockSize; i++) {
		decodedAudio[i] = ((float)encodedAudioReceived[i] / 127.5f) - 1.0f;
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
	if(encodedAudioIndex < audioBlockSize) {
		encodedAudioReceived[encodedAudioIndex] = encodedAudioToSend[encodedAudioIndex];
		encodedAudioIndex++;
	}
}