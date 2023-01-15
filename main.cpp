#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

/**
 * Pin connections
 *
 * D0 to D7     LFOs out
 * A0           Rate encoder in                                 5V tolerant
 * A1           Rate CV in                                      5V tolerant
 * A2           Attenuverter in     (not implemented yet)       3.3V tolerant
 * A2           Waveform switch     (not implemented yet)       3.3V tolerant
 *
*/

// TODO Write code to show LFOs on OLED screen
// TODO Implement attenuverter
// TODO Implement waveform switch

const size_t kOutputCount = 8;

static DaisySeed hw;
static Oscillator lfo[kOutputCount];

const size_t kLoopDelay = 1;
const float kLfoMinFreq = 1.0f/27;      // 13 sec (is actually 27 sec but check why lfo freq is doubled at output)
const float kLfoMaxFreq = 45;           // 90 Hz (is actually 45Hz but check why lfo freq is doubled at output)
const size_t kBlockSize = 4;

const Pin kPins[kOutputCount] = {seed::D0, seed::D1, seed::D2, seed::D3, seed::D4, seed::D5, seed::D6, seed::D7};
const float kLfoFreqRatios[kOutputCount] = {1.0f, 0.551f, 0.271f, 0.16f, 0.084f, 0.041f, 0.018f, 0.009f};  // Measured from VCV ochd

float lfoValues[kOutputCount];
size_t blockIndex;

void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                    AudioHandle::InterleavingOutputBuffer out,
                    size_t size)
{
    for (blockIndex = 0 ; blockIndex < size; blockIndex++)
    {
        for (size_t i = 0; i < kOutputCount; i++)
            lfoValues[i] = lfo[i].Process();
    }
}

int main(void)
{
    // Initialize seed hardware
    hw.Init();
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.SetAudioBlockSize(kBlockSize);

    float sampleRate = hw.AudioSampleRate();
    for (size_t i = 0; i < kOutputCount; i++)
    {
        lfo[i].Init(sampleRate);
        lfo[i].SetAmp(0.5f);
        lfo[i].SetFreq(kLfoMaxFreq * kLfoFreqRatios[i]);
        lfo[i].SetWaveform(Oscillator::WAVE_TRI);
        lfoValues[i] = 0.0f;
    }

    AdcChannelConfig config[2];
    config[0].InitSingle(seed::A0);
    config[1].InitSingle(seed::A1);
    hw.adc.Init(config, 2);
    hw.adc.Start();

    GPIO hwPin[8];
    for (size_t i = 0; i < kOutputCount; i++)
        hwPin[i].Init(kPins[i], GPIO::Mode::OUTPUT, GPIO::Pull::NOPULL, GPIO::Speed::VERY_HIGH);

    hw.StartAudio(AudioCallback);

    size_t pwmThresholds[kOutputCount];
    size_t i, j;
    uint32_t now;
    uint32_t timeSinceLast = 0;
    for (;;)
	{
        for (i = 0; i < kOutputCount; i++)
            pwmThresholds[i] = static_cast<size_t>((lfoValues[i] + 0.5f) * 256); // 8bit dac

        for (i = 0; i < 256 * 8; i++)
        {
            j = i & 7;
            hwPin[j].Write((i >> 3) < pwmThresholds[j]);
        }

        now = System::GetNow();
        if (now > timeSinceLast) {
            timeSinceLast = now + 10;
            float adcValue = fclamp(hw.adc.GetFloat(0) + hw.adc.GetFloat(1), 0, 1);
            float mapped = fmap(adcValue, kLfoMinFreq, kLfoMaxFreq);
            for (i = 0; i < kOutputCount; i++)
                lfo[i].SetFreq(mapped * kLfoFreqRatios[i]);
        }
	}
}
