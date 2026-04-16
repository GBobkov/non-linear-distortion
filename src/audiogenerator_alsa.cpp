#include "audiogenerator_alsa.h"
#include <iostream>
#include <math.h>


TAlsaGenerator::TAlsaGenerator()
{
    FDevices.resize(CMaxAudioInterfaces);
    FHandle = nullptr;
    FParams = nullptr;

    FDelayFrames = 0;
}


TAlsaGenerator::~TAlsaGenerator()
{
    if(FWorkingSoundCardNumber != -1)
        snd_pcm_close(FHandle);

}

void TAlsaGenerator::GetInterfaces(void)
{
    int icard = -1;

    unsigned int card_cnt=0;
    for(unsigned int i=0; i<CMaxAudioInterfaces; i++)
    {
        snd_card_next(&icard);
        if (icard == -1)
            break;

        //char scard[1][32];
        char **pscard = (char**)malloc(32*sizeof(char));
        snd_card_get_longname(icard, pscard);
        std::cout<<icard<<") "<<*pscard<<std::endl;

        FDevices[card_cnt].FCardNumber = icard;
        FDevices[card_cnt].FCardName = "hw:";
        FDevices[card_cnt].FCardName += std::to_string(icard);
        FDevices[card_cnt].FLongName = std::string(*pscard);


        snd_ctl_t *sctl = NULL;
        snd_ctl_open(&sctl, FDevices[i].FCardName.c_str(), 0);

        int idev = -1;
        for (unsigned int j=0; j<CMaxAudioInterfaces; j++)
        {
            if (0 != snd_ctl_pcm_next_device(sctl, &idev)|| idev == -1)
                    break;
            TPcmDevice dev;
            dev.FNumber = idev;
            dev.FSndCtl = sctl;
            char device_id[64];
            snprintf(device_id, sizeof(device_id), "plughw:%u,%u", icard, idev);
            dev.FDeviceId = std::string(device_id);
            FDevices[card_cnt].FPcmDevices.push_back(dev);
        }
        card_cnt++;


//        snd_ctl_close(sctl);
        free(pscard);
    }



    FDevices.resize(card_cnt);

    // temporary check
    }

bool TAlsaGenerator::GetUMC202Interface(void)
{
    for(unsigned int i=0; i<FDevices.size(); i++)
    {
        if(FDevices[i].FLongName.find("BEHRINGER UMC202HD") != std::string::npos)
        {
            const char *name = FDevices[i].FPcmDevices[0].FDeviceId.c_str();
            int rc = snd_pcm_open(&FHandle, name, SND_PCM_STREAM_PLAYBACK, 0);  // 0);
            if (rc < 0)
            {
                //std::cout<<"unable to open pcm device: "<<snd_strerror(rc)<<std::endl;
                return false;
            }
            FWorkingSoundCardNumber = i;

            rc = snd_pcm_set_params(FHandle,
                                    SND_PCM_FORMAT_U24,
                                    SND_PCM_ACCESS_RW_INTERLEAVED,  // SND_PCM_ACCESS_RW_NONINTERLEAVED , SND_PCM_ACCESS_RW_INTERLEAVED
                                    1,
                                    CSampleRate,
                                    0,
                                    CFrameBufferSize);  // 1.5 sec
            if (rc < 0)
            {
                return false;
            }

            if(0 != snd_pcm_delay(FHandle, &FDelayFrames) )
                return false;

            return true;
        }
    }

    return false;
}


void TAlsaGenerator::PlayAudioSample(void)
{
    int err;
    unsigned int i;
    snd_pcm_sframes_t frames;
    const int digit_num = 8*1024;

    uint32_t buffer[digit_num];

    for (i = 0; i < digit_num; i++)
    {
        float phase = 2*M_PI*((float)i)/1024.0;
        float val = 8388608 + 8388500*sin(phase);
        buffer[i] = (uint32_t)val;
    }

    snd_pcm_state_t state = snd_pcm_state(FHandle);
    if(SND_PCM_STATE_SETUP == state)
        snd_pcm_prepare(FHandle);

    for (i = 0; i < 26; i++)
    {
       snd_pcm_sframes_t avail = snd_pcm_avail_update(FHandle);
       while(digit_num>avail)
       {
           avail = 0;
           err = snd_pcm_start(FHandle);
           avail = snd_pcm_avail_update(FHandle);
           usleep(1000);
       }


        {
            frames = snd_pcm_writei(FHandle, buffer, digit_num);

            if (frames < 0)
                frames = snd_pcm_recover(FHandle, frames, 0);
            if (frames < 0)
            {
                printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
                break;
            }
            if (frames > 0 && frames < digit_num )
                printf("Short write (expected %li, wrote %li)\n", (long)sizeof(buffer), frames);
        }
    }

    // pass the remaining samples, otherwise they're dropped in close
    err = snd_pcm_drain(FHandle);
    if (err < 0)
        printf("snd_pcm_drain failed: %s\n", snd_strerror(err));

    for(unsigned int i=0; i<10; i++)
        usleep(1000);

    //snd_pcm_close(FHandle);

}

TAlsaGenerator::TLoadBufferResult TAlsaGenerator::LoadStreamBuffer(std::vector<float> *samples)
{
    if(nullptr == samples)
        return LoadBufferBadInput;

    unsigned int in_size = samples->size();
    if(CMaxStreamBufferSize < in_size)
        return LoadBufferBadInput;

    snd_pcm_sframes_t avaliable_smples = snd_pcm_avail_update(FHandle);
    if(avaliable_smples < 0 )
        return LoadBufferError;


    snd_pcm_sframes_t frames;


    for (unsigned int i = 0; i < in_size; i++)
    {
        float val = 8388608 + 8388500*samples->at(i);
        FStreamBuffer1[i] = (uint32_t)val;
    }


    snd_pcm_state_t state = snd_pcm_state(FHandle);
    if(SND_PCM_STATE_SETUP == state)
        snd_pcm_prepare(FHandle);

    frames = -EAGAIN;
    while(-EAGAIN == frames)
    {
        frames = snd_pcm_writei(FHandle, FStreamBuffer1, in_size);
        if(-EAGAIN == frames)
        {
            for(unsigned int i=0; i<10; i++)
                usleep(1000);
        }
    }

    if (frames < 0)
        frames = snd_pcm_recover(FHandle, frames, 0);




    //snd_pcm_drain(FHandle);

    return LoadBufferOk;
}

bool TAlsaGenerator::ReleaseStreamBuffer(void)
{
    int err;
    err = snd_pcm_drain(FHandle);
    while(-EAGAIN == err)
        err = snd_pcm_drain(FHandle);
    if (err < 0)
        return false;

    for(unsigned int i=0; i<10; i++)
        usleep(1000);

    //ESTRPIPE


    return true;
}



void TAlsaGenerator::MinimalTest(void)
{

    const char *device = "plughw:2,0";//"default";            /* playback device */
    const int digit_num = 8*1024;
    uint32_t buffer[digit_num];              /* some random data */

    int err;
    unsigned int i;
    snd_pcm_t *handle;
    snd_pcm_sframes_t frames;

    for (i = 0; i < digit_num; i++)
    {
        float phase = 2*M_PI*((float)i)/1024.0;
        float val = 8388608 + 8388500*sin(phase);
        buffer[i] = (uint32_t)val;
    }

    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
    if ((err = snd_pcm_set_params(handle,
                      SND_PCM_FORMAT_U24,
                      SND_PCM_ACCESS_RW_INTERLEAVED,
                      1,
                      192000,
                      1,
                      1500000)) < 0)
    {   /* 1.5 sec */
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }



    for (i = 0; i < 26; i++)
    {
        snd_pcm_sframes_t avail = snd_pcm_avail_update(handle);
        while(digit_num>avail)
        {
            avail = 0;
            err = snd_pcm_start(handle);
            avail = snd_pcm_avail_update(handle);
            usleep(1000);
        }


        {
            frames = snd_pcm_writei(handle, buffer, digit_num);

            if (frames < 0)
                frames = snd_pcm_recover(handle, frames, 0);
            if (frames < 0)
            {
                printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
                break;
            }
            if (frames > 0 && frames < digit_num )
                printf("Short write (expected %li, wrote %li)\n", (long)sizeof(buffer), frames);
        }
    }




    /* pass the remaining samples, otherwise they're dropped in close */
    err = snd_pcm_drain(handle);
    if (err < 0)
        printf("snd_pcm_drain failed: %s\n", snd_strerror(err));

    for(unsigned int i=0; i<10; i++)
        usleep(1000);

    snd_pcm_close(handle);

}
