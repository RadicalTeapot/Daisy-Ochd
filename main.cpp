#include <stdio.h>
#include "daisy_seed.h"
#include "daisysp.h"
#include "displayDriver.h"
#include "dev/oled_ssd130x.h"
#if DEBUG == 1
#include <algorithm>
#endif

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
 * D11          OLED SCL                                        I2C1 SCL
 * D12          OLED SDA                                        I2C1 SDA
 *
*/

// TODO Fix OLED drawing (first column is off to last column and higher up)
// TODO Try running PWM in AudioCallback at 16 or 32kHz with a block size of 1
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

OledDisplay<DisplayDriver> display;
static uint8_t DMA_BUFFER_MEM_SECTION displayBuffer[1025]; // 128 * 64 / 8 + 1

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

void DrawingCallback(void *context, I2CHandle::Result result)
{
    display.Fill(false);
    display.DrawRect(0, 0, 127, 63, true, false);
    display.DrawLine(0, 32, 123, 32, true);
    display.DrawLine(32, 0, 32, 63, true);
    display.DrawLine(63, 0, 63, 63, true);
    display.DrawLine(94, 0, 94, 63, true);

    uint_fast32_t height = 2 + static_cast<uint_fast32_t>((1 - (lfoValues[0] + 0.5f)) * 29);
    display.DrawRect(7, height, 27, 30, true, true);
    height = 2 + static_cast<uint_fast32_t>((1 - (lfoValues[1] + 0.5f)) * 29);
    display.DrawRect(38, height, 58, 30, true, true);
    height = 2 + static_cast<uint_fast32_t>((1 - (lfoValues[2] + 0.5f)) * 29);
    display.DrawRect(69, height, 89, 30, true, true);
    height = 2 + static_cast<uint_fast32_t>((1 - (lfoValues[3] + 0.5f)) * 29);
    display.DrawRect(100, height, 120, 30, true, true);

    height = 34 + static_cast<uint_fast32_t>((1 - (lfoValues[4] + 0.5f)) * 29);
    display.DrawRect(7, height, 27, 61, true, true);
    height = 34 + static_cast<uint_fast32_t>((1 - (lfoValues[5] + 0.5f)) * 29);
    display.DrawRect(38, height, 58, 61, true, true);
    height = 34 + static_cast<uint_fast32_t>((1 - (lfoValues[6] + 0.5f)) * 29);
    display.DrawRect(69, height, 89, 61, true, true);
    height = 34 + static_cast<uint_fast32_t>((1 - (lfoValues[7] + 0.5f)) * 29);
    display.DrawRect(100, height, 120, 61, true, true);

    display.Update();
}

int main(void)
{
    // Initialize seed hardware
    hw.Init();
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.SetAudioBlockSize(kBlockSize);

    #if DEBUG == 1
    hw.StartLog(true);
    hw.PrintLine("Debug start");
    hw.PrintLine("Tick freq %dHz", System::GetTickFreq());
    #endif

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

    // Configure display
    OledDisplay<DisplayDriver>::Config cfg;
    cfg.driver_config.buffer = displayBuffer;
    cfg.driver_config.bufferSize = sizeof(displayBuffer);
    cfg.driver_config.drawingCallback = DrawingCallback;
    display.Init(cfg);

    hw.StartAudio(AudioCallback);

    size_t pwmThresholds[kOutputCount];
    size_t i, j;
    uint32_t now;
    uint32_t timeSinceLastAdcRead = 0;
    float adcValue, mappedAdcValue = 0;

    display.Fill(false);
    display.Update();

    #if DEBUG == 1
    uint32_t computing[4] = {0, 0, UINT32_MAX, 0}, writing[4] = {0, 0, UINT32_MAX, 0}, current = 0;
    uint32_t timeSinceLastReport = 0;
    #endif
    for (;;)
	{
        #if DEBUG == 1
        computing[0] = System::GetTick();
        #endif
        for (i = 0; i < kOutputCount; i++)
          pwmThresholds[i] = static_cast<size_t>((lfoValues[i] + 0.5f) * 256); // 8bit dac

        #if DEBUG == 1
        writing[0] = System::GetTick();
        #endif
        for (i = 0; i < 256 * 8; i++)
        {
            j = i & 7;
            hwPin[j].Write((i >> 3) < pwmThresholds[j]);
        }

        #if DEBUG == 1
        current = System::GetTick();

        computing[1] = std::max(writing[0] - computing[0], computing[1]);
        computing[2] = std::min(writing[0] - computing[0], computing[2]);
        computing[3] = static_cast<uint32_t>(computing[3] * 0.75f + (writing[0] - computing[0]) * 0.25f);

        writing[1] = std::max(current - writing[0], writing[1]);
        writing[2] = std::min(current - writing[0], writing[2]);
        writing[3] = static_cast<uint32_t>(writing[3] * 0.75f + (current - writing[0]) * 0.25f);
        #endif

        now = System::GetNow();
        if (now > timeSinceLastAdcRead) {
            timeSinceLastAdcRead = now + 10; // 10Hz
            adcValue = fclamp(hw.adc.GetFloat(0) + hw.adc.GetFloat(1), 0, 1);
            mappedAdcValue = fmap(adcValue, kLfoMinFreq, kLfoMaxFreq);
            for (i = 0; i < kOutputCount; i++)
                lfo[i].SetFreq(mappedAdcValue * kLfoFreqRatios[i]);
        }

        #if DEBUG == 1
        if (now > timeSinceLastReport) {
            timeSinceLastReport = now + 500; // 2Hz
            hw.PrintLine("Computing: Min: %d, Max: %d, Avg: %d", computing[2], computing[1], computing[3]);
            hw.PrintLine("Writing: Min: %d, Max: %d, Avg: %d", writing[2], writing[1], writing[3]);
        }
        #endif
	}
}
