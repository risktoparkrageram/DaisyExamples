#include "daisy_field.h"
#include "daisysp.h"
#include <string>

using namespace daisy;
using namespace daisysp;

DaisyField  hw;
MidiHandler midi;

class Voice
{
  public:
    Voice() {}
    ~Voice() {}
    void Init(float samplerate)
    {
        active_ = false;
        osc_.Init(samplerate);
        osc_.SetAmp(0.75f);
        osc_.SetWaveform(Oscillator::WAVE_SIN);
        env_.Init(samplerate);
        env_.SetSustainLevel(0.5f);
        env_.SetTime(ADSR_SEG_ATTACK, 0.005f);
        env_.SetTime(ADSR_SEG_DECAY, 0.005f);
        env_.SetTime(ADSR_SEG_RELEASE, 0.2f);
        filt_.Init(samplerate);
        filt_.SetFreq(6000.f);
        filt_.SetRes(0.6f);
        filt_.SetDrive(0.8f);
    }

    float Process()
    {
        if(active_)
        {
            float sig, amp;
            amp = env_.Process(env_gate_);
            if(!env_.IsRunning())
                active_ = false;
            sig = osc_.Process();
            filt_.Process(sig);
            return filt_.Low() * (velocity_ / 127.f) * amp;
        }
        return 0.f;
    }

    void OnNoteOn(float note, float velocity)
    {
        note_     = note;
        velocity_ = velocity;
        osc_.SetFreq(mtof(note_));
        active_   = true;
        env_gate_ = true;
    }

    void OnNoteOff()
    {
        env_gate_ = false;
    }

    // Envelop
    void SetEnvAttack(float val) { env_.SetTime(ADSR_SEG_ATTACK, val); }
    void SetEnvDecay(float val) { env_.SetTime(ADSR_SEG_DECAY, val); }
    void SetEnvSustain(float val) {env_.SetSustainLevel(val); }
    void SetEnvRelease(float val) { env_.SetTime(ADSR_SEG_RELEASE, val); }
    // Filter
    void SetCutoff(float val) { filt_.SetFreq(val); }
    void SetResonance(float val) { filt_.SetRes(val); }
    void SetResDrive(float val) { filt_.SetDrive(val); }
    // Waveform selection
    void IncrementWaveform()
        {
            waveidx = (waveidx + 1) % 5;
            osc_.SetWaveform(waveforms[waveidx]);
        }

    uint8_t waveforms[5] = {
        Oscillator::WAVE_SIN,
        Oscillator::WAVE_TRI,
        Oscillator::WAVE_POLYBLEP_TRI,
        Oscillator::WAVE_POLYBLEP_SAW,
        Oscillator::WAVE_POLYBLEP_SQUARE,
    };
    uint8_t waveidx = 0;

    inline bool  IsActive() const { return active_; }
    inline float GetNote() const { return note_; }

  private:
    Oscillator osc_;
    Svf        filt_;
    Adsr       env_;
    float      note_, velocity_;
    bool       active_;
    bool       env_gate_;
};

template <size_t max_voices>
class VoiceManager
{
  public:
    VoiceManager() {}
    ~VoiceManager() {}

    void Init(float samplerate)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].Init(samplerate);
        }
    }

    float Process()
    {
        float sum;
        sum = 0.f;
        for(size_t i = 0; i < max_voices; i++)
        {
            sum += voices[i].Process();
        }
        return sum;
    }

    void OnNoteOn(float notenumber, float velocity)
    {
        Voice *v = FindFreeVoice();
        if(v == NULL)
            return;
        v->OnNoteOn(notenumber, velocity);
    }

    void OnNoteOff(float notenumber, float velocity)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            Voice *v = &voices[i];
            if(v->IsActive() && v->GetNote() == notenumber)
            {
                v->OnNoteOff();
            }
        }
    }

    void FreeAllVoices()
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].OnNoteOff();
        }
    }

    void SetCutoff(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetCutoff(all_val);
        }
    }

    void SetResonance(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetResonance(all_val);
        }
    }

    void SetResDrive(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetResDrive(all_val);
        }
    }

    void SetEnvAttack(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetEnvAttack(all_val);
        }
    }

    void SetEnvDecay(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetEnvDecay(all_val);
        }
    }

    void SetEnvRelease(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetEnvRelease(all_val);
        }
    }

    void SetEnvSustain(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetEnvSustain(all_val);
        }
    }

    void IncrementWaveform()
    {
        for(size_t i =0; i < max_voices; i++)
        {
            voices[i].IncrementWaveform();
        }
    }

  private:
    Voice  voices[max_voices];
    Voice *FindFreeVoice()
    {
        Voice *v = NULL;
        for(size_t i = 0; i < max_voices; i++)
        {
            if(!voices[i].IsActive())
            {
                v = &voices[i];
                break;
            }
        }
        return v;
    }
};

static VoiceManager<24> voice_handler;

float kvals[8];
float cvvals[4];

void AudioCallback(float *in, float *out, size_t size)
{
    float sum = 0.f;
    hw.UpdateDigitalControls();
    hw.ProcessAnalogControls();

    // get knob values
    for(int i = 0; i < 8; i++)
    {
        kvals[i] = hw.GetKnobValue(i);
        if(i < 4)
        {
            cvvals[i] = hw.GetCvValue(i);
        }
    }

    // Free all voices
    if(hw.GetSwitch(hw.SW_1)->FallingEdge())
    {
        voice_handler.FreeAllVoices();
    }

    // Waveform
    if(hw.GetSwitch(hw.SW_2)->FallingEdge())
    {
        voice_handler.IncrementWaveform();
    }

    // Filter
    voice_handler.SetCutoff(250.f + kvals[0] * 8000.f);
    voice_handler.SetResonance(kvals[1]);
    voice_handler.SetResDrive(kvals[2]);

    // Envelope
    voice_handler.SetEnvAttack(kvals[3] * 0.5f);
    voice_handler.SetEnvDecay(kvals[4] * 3.f);
    voice_handler.SetEnvSustain(kvals[5] * 0.5f);
    voice_handler.SetEnvRelease(kvals[6] * 1.0f);

    for(size_t i = 0; i < size; i += 2)
    {
        sum = 0.f;
        sum        = voice_handler.Process() * 0.5f;
        out[i]     = sum;
        out[i + 1] = sum;
    }
}

// Typical Switch case for Message Type.
void HandleMidiMessage(MidiEvent m)
{
    // hardcoded midi Channel for now (midi channel 1)
    if(m.channel == 0)
    {
        switch(m.type)
        {
            case NoteOn:
            {
                NoteOnEvent p = m.AsNoteOn();
                // Note Off can come in as Note On w/ 0 Velocity
                //  && m.channel == 6
                if(p.velocity == 0.f)
                {
                    voice_handler.OnNoteOff(p.note, p.velocity);
                }
                else
                {
                    voice_handler.OnNoteOn(p.note, p.velocity);
                }
            }
            break;
            case NoteOff:
            {
                NoteOnEvent p = m.AsNoteOn();
                voice_handler.OnNoteOff(p.note, p.velocity);
            }
            break;
            default: break;
        }
    }
}

// Main -- Init, and Midi Handling
int main(void)
{
    // Init
    float samplerate;
    hw.Init();
    samplerate = hw.AudioSampleRate();
    midi.Init(MidiHandler::INPUT_MODE_UART1, MidiHandler::OUTPUT_MODE_NONE);
    voice_handler.Init(samplerate);

    //display
    const char  str[] = "Midi";
    char *      cstr = (char*)str;
    hw.display.WriteString(cstr, Font_7x10, true);
    hw.display.Update();

    // Start stuff.
    midi.StartReceive();
    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    for(;;)
    {
        midi.Listen();
        // Handle MIDI Events
        while(midi.HasEvents())
        {
            HandleMidiMessage(midi.PopEvent());
        }
    }
}
