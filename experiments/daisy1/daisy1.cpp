#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;
Oscillator osc1;
Oscillator osc2;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		out[0][i] = osc1.Process();
		out[1][i] = osc2.Process();
	}
}

int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

	osc1.Init(hw.AudioSampleRate());
	osc1.SetFreq(110.0f); // Set oscillator frequency to 440 Hz (A4)
	osc2.Init(hw.AudioSampleRate());
	osc2.SetFreq(165.0f); // Set oscillator frequency to 440 Hz (A4)
	hw.StartAudio(AudioCallback);
	while(1) {}
}
