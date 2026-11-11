#include "daisy_seed.h"
#include "daisysp.h"
#include <stdint.h>

using namespace daisy;
using namespace daisysp;


DaisySeed hw;
const uint16_t maxBlockSize = 1024;
uint16_t audioBlockSize = 256;
uint16_t currentAlgorithm = 1;
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
		out[1][i] = decodedAudio[i];
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
	if(currentAlgorithm == 0) {
		// 8-bit PCM with 4-bit crush
		for(int i=0; i<audioBlockSize; i++) {
			encodedAudioToSend[i] = (((uint8_t)((storedAudio[i] + 1.0f) * 127.5f)) >> 4) << 4;
		}
	} else if(currentAlgorithm == 1) {
		// pretty ugly/inefficient, plenty of AI, but this algorithm extracts the biggest peaks/troughs and lerps between them (straight lines, will be curves in future)

		float prevDiff = storedAudio[1] - storedAudio[0];
		float nextDiff = 0.0f;
		struct TurningPoint {
			int index;
			bool isPeak;
		};
		TurningPoint peaks[audioBlockSize];
		TurningPoint troughs[audioBlockSize];
		int numPointsToEncode = 30;
		TurningPoint allPoints[2 + 2 * numPointsToEncode];
		int peakCount = 0;
		int troughCount = 0;
		for(int i=1; i<audioBlockSize-1; i++) {
			nextDiff = storedAudio[i+1] - storedAudio[i];
			if(prevDiff > 0 && nextDiff < 0) {
				// peak
				peaks[peakCount].index = i;
				peaks[peakCount].isPeak = true;
				peakCount++;
			} else if(prevDiff < 0 && nextDiff > 0) {
				// trough
				troughs[troughCount].index = i;
				troughs[troughCount].isPeak = false;
				troughCount++;
			}
			prevDiff = nextDiff;
		}
		// sort peaks in order of value
		for(int i=0; i<peakCount-1; i++) {
			for(int j=i+1; j<peakCount; j++) {
				if(storedAudio[peaks[i].index] < storedAudio[peaks[j].index]) {
					TurningPoint temp = peaks[i];
					peaks[i] = peaks[j];
					peaks[j] = temp;
				}
			}
		}
		// sort troughs in order of value
		for(int i=0; i<troughCount-1; i++) {
			for(int j=i+1; j<troughCount; j++) {
				if(storedAudio[troughs[i].index] < storedAudio[troughs[j].index]) {
					TurningPoint temp = troughs[i];
					troughs[i] = troughs[j];
					troughs[j] = temp;
				}
			}
		}

		// combine peaks and troughs into allPoints array
		allPoints[0].index = 0;
		allPoints[1].index = audioBlockSize - 1;
		int allPointsCount = 2;
		for(int i=0; i<numPointsToEncode && i<peakCount; i++) {
			allPoints[allPointsCount++] = peaks[i];
		}
		for(int i=0; i<numPointsToEncode && i<troughCount; i++) {
			allPoints[allPointsCount++] = troughs[i];
		}

		// order allPoints by index
		for(int i=0; i<allPointsCount-1; i++) {
			for(int j=i+1; j<allPointsCount; j++) {
				if(allPoints[i].index > allPoints[j].index) {
					TurningPoint temp = allPoints[i];
					allPoints[i] = allPoints[j];
					allPoints[j] = temp;
				}
			}
		}

		// temporarily, render full audio as PCM from peaks and troughs
		for(int i=0; i<audioBlockSize; i++) {
			// lerp between the nearest turning points
			int prevIndex = 0;
			int nextIndex = 0;
			for(int j=0; j<allPointsCount; j++) {
				if(allPoints[j].index <= i) {
					prevIndex = allPoints[j].index;
				}
				if(allPoints[j].index > i) {
					nextIndex = allPoints[j].index;
					break;
				}
			}
			float t = (float)(i - prevIndex) / (nextIndex - prevIndex);
			encodedAudioToSend[i] = (uint8_t)(((1.0f - t) * storedAudio[prevIndex] + t * storedAudio[nextIndex]) * 127.5f + 127.5f);
		}
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
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_16KHZ);

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