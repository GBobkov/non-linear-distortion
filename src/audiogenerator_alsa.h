#ifndef AUDIOGENERATOR_ALSA_H
#define AUDIOGENERATOR_ALSA_H


#include <alsa/asoundlib.h>
#include <string>
#include <vector>

// https://habr.com/ru/articles/663352/#linux-and-alsa
// https://www.alsa-project.org/alsa-doc/alsa-lib/control.html#control_cards_id

class TAlsaGenerator  // alsa based generator
{
    public:
        TAlsaGenerator();
        ~TAlsaGenerator();

    public:
        static const unsigned int CMaxAudioInterfaces = 10;  // max avaliable interfaces

        typedef struct    // pcm device in sound card info
        {
            unsigned int FNumber;
            snd_ctl_t* FSndCtl;  // handle to pcm sound device

            std::string FDeviceId;
        } TPcmDevice;

        typedef struct           // generic audio device description
        {
            unsigned int FCardNumber;
            std::string FCardName;

            std::string FLongName;

            std::vector<TPcmDevice> FPcmDevices;  // handle to pcm sound devices

        } TDevice;

    public:
        void GetInterfaces(void); // display accesible interfaces

        bool GetUMC202Interface(void);  // get steinberg UR12 sound card and connect

        static void MinimalTest(void);  // temp - remove later

        //bool SetAudioBuffer(void);  // set access to audio buffer

        void PlayAudioSample(void);  // play ~ 1.2 s test sine audio sample (f ~ 192 Hz)

        typedef enum
        {
            LoadBufferOk,
            LoadBufferError,
            LoadBufferFull,
            LoadBufferBadInput
        } TLoadBufferResult;
        TLoadBufferResult LoadStreamBuffer(std::vector<float> *samples);
        bool ReleaseStreamBuffer(void);


    public:
        static const unsigned int CSampleRate = 192000;
        static const unsigned int CFrameBufferSize = 1800000; // 1.5 s  reserved buffer time in sound card
        static const unsigned int CSignalDuration = 1100000;  // 1.1 s  stimul signal duration

        static const unsigned int CMaxStreamBufferSize = 310000;
        static const unsigned int CMinimumAlsabufferReadyToDrain = 300000;
        static const unsigned int CMinimumAlsabufferReadyToLoad = 5000;

    protected:
        std::vector<TDevice> FDevices; // avaliable devices list

        snd_pcm_t *FHandle;
        snd_pcm_hw_params_t *FParams;
        int FWorkingSoundCardNumber = -1;  // number of working sound card

        int FFrameSize;
        int FBufferSize;

        uint32_t FStreamBuffer1[CMaxStreamBufferSize];

        snd_pcm_sframes_t FDelayFrames;

        void UpdateBuffers(std::vector<float> *input);

};

#endif // AUDIOGENERATOR_ALSA_H
