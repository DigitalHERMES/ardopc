
#define TRUE 1
#define true 1
#define FALSE 0
#define false 0
#define bool int

#ifdef WIN32
#define DllExport __declspec(dllexport)
#include "Windows.h"
#include <mmsystem.h>

#else
#define DllExport 
#include <alsa/asoundlib.h>
#include <signal.h>
#endif

//HRESULT (WINAPI *pDirectSoundCreate)(GUID FAR *lpGUID, LPDIRECTSOUND FAR *lplpDS, IUnknown FAR *pUnkOuter);

snd_pcm_t *	playhandle = NULL;
snd_pcm_t *	rechandle = NULL;

int m_playchannels = 1;
int m_recchannels = 1;

char SavedCaptureDevice[256];	// Saved so we can reopen
char SavedPlaybackDevice[256];

int SavedCaptureRate;
int SavedPlaybackRate;

// This rather convoluted process simplifies marshalling from Managed Code

char ** WriteDevices = NULL;
int WriteDeviceCount = 0;

char ** ReadDevices = NULL;
int ReadDeviceCount = 0;

// Routine to check that library is available

int CheckifLoaded()
{
	// Prevent CTRL/C from closing the TNC
	// (This causes problems if the TNC is started by LinBPQ)

	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	return TRUE;
}

DllExport int GetOutputDeviceCollection()
{
	// Get all the suitable devices and put in a list for GetNext to return

	snd_ctl_t *handle= NULL;
	snd_pcm_t *pcm= NULL;
	snd_ctl_card_info_t *info;
	snd_pcm_info_t *pcminfo;
	snd_pcm_hw_params_t *pars;
	snd_pcm_format_mask_t *fmask;
	char NameString[256];

	printf("getWriteDevices\n");

	CloseSoundCard();

	// free old struct if called again

//	while (WriteDeviceCount)
//	{
//		WriteDeviceCount--;
//		free(WriteDevices[WriteDeviceCount]);
//	}

//	if (WriteDevices)
//		free(WriteDevices);

	WriteDevices = NULL;
	WriteDeviceCount = 0;

	//	Add virtual device ARDOP so ALSA plugins can be used if needed

	WriteDevices = realloc(WriteDevices,(WriteDeviceCount + 1) * 4);
	WriteDevices[WriteDeviceCount++] = strdup("ARDOP");

	//	Get Device List  from ALSA
	
	snd_ctl_card_info_alloca(&info);
	snd_pcm_info_alloca(&pcminfo);
	snd_pcm_hw_params_alloca(&pars);
	snd_pcm_format_mask_alloca(&fmask);

	char hwdev[80];
	unsigned min, max;
	int card, err, dev, nsubd;
	snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
	
	card = -1;

	if (snd_card_next(&card) < 0)
	{
		printf("No Devices\n");
		return 0;
	}

	if (playhandle)
		snd_pcm_close(playhandle);

	playhandle = NULL;

	while (card >= 0)
	{
		sprintf(hwdev, "hw:%d", card);
		err = snd_ctl_open(&handle, hwdev, 0);
		err = snd_ctl_card_info(handle, info);
    
		printf("Card %d, ID `%s', name `%s'\n", card, snd_ctl_card_info_get_id(info),
                snd_ctl_card_info_get_name(info));


		dev = -1;

		if(snd_ctl_pcm_next_device(handle, &dev) < 0)
		{
			// Card has no devices

			snd_ctl_close(handle);
			goto nextcard;      
		}

		while (dev >= 0)
		{
			snd_pcm_info_set_device(pcminfo, dev);
			snd_pcm_info_set_subdevice(pcminfo, 0);
			snd_pcm_info_set_stream(pcminfo, stream);
	
			err = snd_ctl_pcm_info(handle, pcminfo);

			
			if (err == -ENOENT)
				goto nextdevice;

			nsubd = snd_pcm_info_get_subdevices_count(pcminfo);
		
			printf("  Device %d, ID `%s', name `%s', %d subdevices (%d available)\n",
				dev, snd_pcm_info_get_id(pcminfo), snd_pcm_info_get_name(pcminfo),
				nsubd, snd_pcm_info_get_subdevices_avail(pcminfo));

			sprintf(hwdev, "hw:%d,%d", card, dev);

			err = snd_pcm_open(&pcm, hwdev, stream, SND_PCM_NONBLOCK);

			if (err)
			{
				printf("Error %d opening output device\n", err);
				goto nextdevice;
			}

			//	Get parameters for this device

			err = snd_pcm_hw_params_any(pcm, pars);
 
			snd_pcm_hw_params_get_channels_min(pars, &min);
			snd_pcm_hw_params_get_channels_max(pars, &max);
			
			if( min == max )
				if(min == 1)
					printf("    1 channel, ");
				else
					printf("    %d channels, ", min);
			else
				printf("    %u..%u channels, ", min, max);
			
			snd_pcm_hw_params_get_rate_min(pars, &min, NULL);
			snd_pcm_hw_params_get_rate_max(pars, &max, NULL);
			printf("sampling rate %u..%u Hz\n", min, max);

			// Add device to list

			sprintf(NameString, "hw:%d,%d %s(%s)", card, dev,
				snd_pcm_info_get_name(pcminfo), snd_ctl_card_info_get_name(info));

			printf("%s\n", NameString);

			WriteDevices = realloc(WriteDevices,(WriteDeviceCount + 1) * 4);
			WriteDevices[WriteDeviceCount++] = strdup(NameString);

			snd_pcm_close(pcm);
			pcm= NULL;

nextdevice:

			if (snd_ctl_pcm_next_device(handle, &dev) < 0)
				break;
	    }

		snd_ctl_close(handle);

nextcard:

		if (snd_card_next(&card) < 0)		// No more cards
			break;
	}

	return WriteDeviceCount;
}

DllExport int GetNextOutputDevice(char * dest, int max, int n)
{
	if (n >= WriteDeviceCount)
		return 0;

	strcpy(dest, WriteDevices[n]);
	return strlen(dest);
}


DllExport int GetInputDeviceCollection()
{
	// Get all the suitable devices and put in a list for GetNext to return

	snd_ctl_t *handle= NULL;
	snd_pcm_t *pcm= NULL;
	snd_ctl_card_info_t *info;
	snd_pcm_info_t *pcminfo;
	snd_pcm_hw_params_t *pars;
	snd_pcm_format_mask_t *fmask;
	char NameString[256];

	printf("getReadDevices\n");

	// free old struct if called again

//	while (ReadDeviceCount)
//	{
//ReadDeviceCount--;
//		free(ReadDevices[ReadDeviceCount]);
//	}

//	if (ReadDevices)
//		free(ReadDevices);

	ReadDevices = NULL;
	ReadDeviceCount = 0;

	//	Add virtual device ARDOP so ALSA plugins can be used if needed

	ReadDevices = realloc(ReadDevices,(ReadDeviceCount + 1) * 4);
	ReadDevices[ReadDeviceCount++] = strdup("ARDOP");

	//	Get Device List  from ALSA
	
	snd_ctl_card_info_alloca(&info);
	snd_pcm_info_alloca(&pcminfo);
	snd_pcm_hw_params_alloca(&pars);
	snd_pcm_format_mask_alloca(&fmask);

	char hwdev[80];
	unsigned min, max;
	int card, err, dev, nsubd;
	snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;
	
	card = -1;

	if(snd_card_next(&card) < 0)
	{
		printf("No Devices\n");
		return 0;
	}

	if (rechandle)
		snd_pcm_close(rechandle);

	rechandle = NULL;

	while(card >= 0)
	{
		sprintf(hwdev, "hw:%d", card);
		err = snd_ctl_open(&handle, hwdev, 0);
		err = snd_ctl_card_info(handle, info);
    
		printf("Card %d, ID `%s', name `%s'\n", card, snd_ctl_card_info_get_id(info),
                snd_ctl_card_info_get_name(info));

		dev = -1;
			
		if (snd_ctl_pcm_next_device(handle, &dev) < 0)		// No Devicdes
		{
			snd_ctl_close(handle);
			goto nextcard;      
		}

		while(dev >= 0)
		{
			snd_pcm_info_set_device(pcminfo, dev);
			snd_pcm_info_set_subdevice(pcminfo, 0);
			snd_pcm_info_set_stream(pcminfo, stream);
			err= snd_ctl_pcm_info(handle, pcminfo);
	
			if (err == -ENOENT)
				goto nextdevice;
	
			nsubd= snd_pcm_info_get_subdevices_count(pcminfo);
			printf("  Device %d, ID `%s', name `%s', %d subdevices (%d available)\n",
				dev, snd_pcm_info_get_id(pcminfo), snd_pcm_info_get_name(pcminfo),
				nsubd, snd_pcm_info_get_subdevices_avail(pcminfo));

			sprintf(hwdev, "hw:%d,%d", card, dev);

			err = snd_pcm_open(&pcm, hwdev, stream, SND_PCM_NONBLOCK);
	
			if (err)
			{	
				printf("Error %d opening input device\n", err);
				goto nextdevice;
			}

			err = snd_pcm_hw_params_any(pcm, pars);
 
			snd_pcm_hw_params_get_channels_min(pars, &min);
			snd_pcm_hw_params_get_channels_max(pars, &max);
	
			if( min == max )
				if( min == 1 )
					printf("    1 channel, ");
				else
					printf("    %d channels, ", min);
			else
				printf("    %u..%u channels, ", min, max);
			
			snd_pcm_hw_params_get_rate_min(pars, &min, NULL);
			snd_pcm_hw_params_get_rate_max(pars, &max, NULL);
			printf("sampling rate %u..%u Hz\n", min, max);

			sprintf(NameString, "hw:%d,%d %s(%s)", card, dev,
				snd_pcm_info_get_name(pcminfo), snd_ctl_card_info_get_name(info));

			printf("%s\n", NameString);

			ReadDevices = realloc(ReadDevices,(ReadDeviceCount + 1) * 4);
			ReadDevices[ReadDeviceCount++] = strdup(NameString);

			snd_pcm_close(pcm);
			pcm= NULL;

nextdevice:
		
			if (snd_ctl_pcm_next_device(handle, &dev) < 0)
				break;
	    }

		snd_ctl_close(handle);

nextcard:

		if (snd_card_next(&card) < 0 )
			break;
	}

	return ReadDeviceCount;
}

DllExport int GetNextInputDevice(char * dest, int max, int n)
{
	if (n >= ReadDeviceCount)
		return 0;

	strcpy(dest, ReadDevices[n]);
	return strlen(dest);
}
DllExport bool OpenSoundCard(char * CaptureDevice, char * PlaybackDevice, int c_sampleRate, int p_sampleRate)
{
	printf("Opening Playback Device %s Rate %d\n", PlaybackDevice, p_sampleRate);

	if (OpenSoundPlayback(PlaybackDevice, p_sampleRate))
	{
		printf("Opening Capture Device %s Rate %d\n", CaptureDevice, c_sampleRate);
		return OpenSoundCapture(CaptureDevice, c_sampleRate);
	}
	else
		return false;
}

DllExport bool OpenSoundPlayback(char * PlaybackDevice, int m_sampleRate)
{
	int err = 0;

	char buf1[100];
	char * ptr;

	if (playhandle)
	{
		snd_pcm_close(playhandle);
		playhandle = NULL;
	}

	strcpy(SavedPlaybackDevice, PlaybackDevice);	// Saved so we can reopen in error recovery
	SavedPlaybackRate = m_sampleRate;

	strcpy(buf1, PlaybackDevice);

	ptr = strchr(buf1, ' ');
	if (ptr) *ptr = 0;				// Get Device part of name

	snd_pcm_hw_params_t *hw_params;
	
	if ((err = snd_pcm_open(&playhandle, buf1, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) {
		fprintf (stderr, "cannot open playback audio device %s (%s)\n",  buf1, snd_strerror(err));
		return false;
	}
		   
	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
		return false;
	}
				 
	if ((err = snd_pcm_hw_params_any (playhandle, hw_params)) < 0) {
		fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
		return false;
	}
	
	if ((err = snd_pcm_hw_params_set_access (playhandle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
			fprintf (stderr, "cannot set playback access type (%s)\n", snd_strerror (err));
		return false;
	}
	if ((err = snd_pcm_hw_params_set_format (playhandle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		fprintf (stderr, "cannot setplayback  sample format (%s)\n", snd_strerror(err));
		return false;
	}
	
	if ((err = snd_pcm_hw_params_set_rate (playhandle, hw_params, m_sampleRate, 0)) < 0) {
		fprintf (stderr, "cannot set playback sample rate (%s)\n", snd_strerror(err));
		return false;
	}

	m_playchannels = 1;
	
	if ((err = snd_pcm_hw_params_set_channels (playhandle, hw_params, 1)) < 0)
	{
		fprintf (stderr, "cannot set play channel count to 1 (%s)\n", snd_strerror(err));
		m_playchannels = 2;

		if ((err = snd_pcm_hw_params_set_channels (playhandle, hw_params, 2)) < 0)
		{
			fprintf (stderr, "cannot play set channel count to 2 (%s)\n", snd_strerror(err));
				return false;
		}
		fprintf (stderr, "Play channel count set to 2 (%s)\n", snd_strerror(err));
	}
	
	if ((err = snd_pcm_hw_params (playhandle, hw_params)) < 0) {
		fprintf (stderr, "cannot set parameters (%s)\n", snd_strerror(err));
		return false;
	}
	
	snd_pcm_hw_params_free(hw_params);
	
	if ((err = snd_pcm_prepare (playhandle)) < 0) {
		fprintf (stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror(err));
		return false;
	}

	return true;
}

DllExport bool OpenSoundCapture(char * CaptureDevice, int m_sampleRate)
{
	int err = 0;

	char buf1[100];
	char * ptr;
	snd_pcm_hw_params_t *hw_params;

	if (rechandle)
	{
		snd_pcm_close(rechandle);
		rechandle = NULL;
	}

	strcpy(SavedCaptureDevice, CaptureDevice);	// Saved so we can reopen in error recovery
	SavedCaptureRate = m_sampleRate;

	strcpy(buf1, CaptureDevice);

	ptr = strchr(buf1, ' ');
	if (ptr) *ptr = 0;				// Get Device part of name
	
	if ((err = snd_pcm_open (&rechandle, buf1, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf (stderr, "cannot open capture audio device %s (%s)\n",  buf1, snd_strerror(err));
		return false;
	}
	   
	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		fprintf (stderr, "cannot allocate capture hardware parameter structure (%s)\n", snd_strerror(err));
		return false;
	}
				 
	if ((err = snd_pcm_hw_params_any (rechandle, hw_params)) < 0) {
		fprintf (stderr, "cannot initialize capture hardware parameter structure (%s)\n", snd_strerror(err));
		return false;
	}
	
	if ((err = snd_pcm_hw_params_set_access (rechandle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
			fprintf (stderr, "cannot set capture access type (%s)\n", snd_strerror (err));
		return false;
	}
	if ((err = snd_pcm_hw_params_set_format (rechandle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		fprintf (stderr, "cannot set capture sample format (%s)\n", snd_strerror(err));
		return false;
	}
	
	if ((err = snd_pcm_hw_params_set_rate (rechandle, hw_params, m_sampleRate, 0)) < 0) {
		fprintf (stderr, "cannot set capture sample rate (%s)\n", snd_strerror(err));
		return false;
	}
	
	m_recchannels = 1;
	
	if ((err = snd_pcm_hw_params_set_channels (rechandle, hw_params, 1)) < 0)
	{
		fprintf (stderr, "cannot set rec channel count to 1 (%s)\n", snd_strerror(err));
		m_recchannels = 2;

		if ((err = snd_pcm_hw_params_set_channels (rechandle, hw_params, 2)) < 0)
		{
			fprintf (stderr, "cannot rec set channel count to 2 (%s)\n", snd_strerror(err));
			return false;
		}
		fprintf (stderr, "Record channel count set to 2 (%s)\n", snd_strerror(err));
	}
	
	if ((err = snd_pcm_hw_params (rechandle, hw_params)) < 0) {
		fprintf (stderr, "cannot set parameters (%s)\n", snd_strerror(err));
		return false;
	}
	
	snd_pcm_hw_params_free(hw_params);
	
	if ((err = snd_pcm_prepare (rechandle)) < 0) {
		fprintf (stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror(err));
		return FALSE;
	}

	int i;
	short buf[256];

	for (i = 0; i < 10; ++i)
	{
		if ((err = snd_pcm_readi (rechandle, buf, 128)) != 128)
		{
			fprintf (stderr, "read from audio interface failed (%s)\n",
				 snd_strerror (err));
		}
	}

//	printf("Read got %d\n", err);

 	return TRUE;
}

int CloseSoundCard()
{
	if (rechandle)
	{
		snd_pcm_close(rechandle);
		rechandle = NULL;
	}

	if (playhandle)
	{
		snd_pcm_close(playhandle);
		playhandle = NULL;
	}
	return 0;
}


int SoundCardWrite(unsigned char * input, unsigned int nSamples)
{
	unsigned int i = 0, n;
	int ret, err, res;
	snd_pcm_sframes_t avail, maxavail;
	snd_pcm_status_t *status = NULL;

	if (playhandle == NULL)
		return 0;

	//	Stop Capture

	if (rechandle)
	{
		snd_pcm_close(rechandle);
		rechandle = NULL;
	}


	avail = snd_pcm_avail_update(playhandle);
//	printf("avail before play returned %d\n", (int)avail);

	if (avail < 0)
	{
		if (avail != -32)
			printf("Playback Avail Recovering from %d ..\n", (int)avail);
		snd_pcm_recover(playhandle, avail, 1);

		avail = snd_pcm_avail_update(playhandle);

		if (avail < 0)
			printf("avail play after recovery returned %d\n", (int)avail);
	}
	
	maxavail = avail;

	nSamples /= 2;

//	printf("Tosend %d Avail %d\n", nSamples, (int)avail);

	while (avail < nSamples)
	{
		// Send Next bit 

//		printf("Too much to send at once, sending %d\n", (int)avail);
		
		if (avail <= 0)
			return;					// Something gone wrong

		PackSamplesAndSend(input, avail);
		nSamples -= avail;
		input += (avail * 2);		// Two bytes per sample

		// Wait a bit. Try half the time to send the maximum, to allow a bit of latency

		usleep((maxavail * 1000) / 24);		// 12000 samples per second

		avail = snd_pcm_avail_update(playhandle);

		if (avail < 0)
		{
//			if (avail != -32)
//				printf("Playback Avail Recovering from %d ..\n", (int)avail);
			snd_pcm_recover(playhandle, avail, 1);

			avail = snd_pcm_avail_update(playhandle);
			if (avail < 0)
				printf("avail play after recovery returned %d\n", (int)avail);
		}
	}

	//	Send last bit
	
//	printf("sending last %d\n", (int)avail);
	ret = PackSamplesAndSend(input, nSamples);

	// Wait till it completes before returning

	// See how much is left to send. We don't want to sleep too long, or turnround
	//	will be affected, but polling too often wastes resources...

	int initialsleep = maxavail - snd_pcm_avail_update(playhandle); // samples

	initialsleep = (initialsleep * 1000 / 12);	
	initialsleep -= 50000;

//	printf("Waiting %d \n", initialsleep);
	usleep(initialsleep);

	while (1)
	{
//		printf("Waiting... \n");
		
		snd_pcm_status_alloca(&status);					// alloca allocates once per function, does not need a free

		if ((err=snd_pcm_status(playhandle, status))!=0)
		{
    		printf("snd_pcm_status() failed: %s",snd_strerror(err));
			break;
		}
	 
		res = snd_pcm_status_get_state(status);

		if (res != SND_PCM_STATE_RUNNING)				// If sound system is not running then it needs data
		{
			// Send complete - Restart Capture

			OpenSoundCapture(SavedCaptureDevice, SavedCaptureRate);	
			return ret;
		}
		usleep(50000);
	}

	return ret;
}

int PackSamplesAndSend(unsigned char * input, int nSamples)
{
	unsigned short samples[256000];
	unsigned short * sampptr = samples;
	unsigned int n;
	int ret;
	snd_pcm_sframes_t avail;

	// Convert byte stream to int16 (watch endianness)

	if (m_playchannels == 1)
	{
		for (n = 0; n < nSamples; n++)
		{
			*(sampptr++) = input[1] << 8 | input[0];
			input += 2;
		}
	}
	else
	{
		int i = 0;
		for (n = 0; n < nSamples; n++)
		{
			*(sampptr) = input[1] << 8 | input[0];
			*(sampptr + 1) = *(sampptr);		// same to both channels
			*(sampptr) += 2;
			input += 2;
		}
	}

	ret = snd_pcm_writei(playhandle, samples, nSamples);

	if (ret < 0)
	{
//		printf("Write Recovering from %d ..\n", ret);
		snd_pcm_recover(playhandle, ret, 1);
		ret = snd_pcm_writei(playhandle, samples, nSamples);
//		printf("Write after recovery returned %d\n", ret);
	}

	avail = snd_pcm_avail_update(playhandle);
	return ret;

}

int SoundCardClearInput()
{
	short samples[65536];
	int n;
	int ret;
	int avail;

	if (rechandle == NULL)
		return 0;

	// Clear queue 
	
	avail = snd_pcm_avail_update(rechandle);

	if (avail < 0)
	{
		printf("Discard Recovering from %d ..\n", avail);
		if (rechandle)
		{
			snd_pcm_close(rechandle);
			rechandle = NULL;
		}
		OpenSoundCapture(SavedCaptureDevice, SavedCaptureRate);
		avail = snd_pcm_avail_update(rechandle);
	}

	while (avail)
	{
		if (avail > 65536)
			avail = 65536;

			ret = snd_pcm_readi(rechandle, samples, avail);
//			printf("Discarded %d samples from card\n", ret);
			avail = snd_pcm_avail_update(rechandle);

//			printf("Discarding %d samples from card\n", avail);
	}
	return 0;
}


int SoundCardRead(unsigned char * input, unsigned int nSamples)
{
	short samples[65536];
	int n;
	int ret;
	int avail;

	if (rechandle == NULL)
		return 0;

	avail = snd_pcm_avail_update(rechandle);

	if (avail < 0)
	{
		printf("Read Recovering from %d ..\n", avail);
		if (rechandle)
		{
			snd_pcm_close(rechandle);
			rechandle = NULL;
		}

		OpenSoundCapture(SavedCaptureDevice, SavedCaptureRate);
//		snd_pcm_recover(rechandle, avail, 0);
		avail = snd_pcm_avail_update(rechandle);
		printf("Read After recovery %d ..\n", avail);
	}

//	if (avail < 960)
//		return 0;
//
	if (avail > nSamples/2)
	{
		printf("ALSARead available %d\n", avail);
		avail = nSamples/2;
	}

	ret = snd_pcm_readi(rechandle, samples, avail);

	if (ret < 0)
	{
		printf("RX Error %d\n", ret);
		snd_pcm_recover(rechandle, avail, 0);
		return 0;
	}

	if (m_recchannels == 1)
	{
		for (n = 0; n < ret; n++)
		{
			*(input++) = samples[n] & 0xFF;
			*(input++) = samples[n] >> 8;
		}
	}
	else
	{
		for (n = 0; n < (ret * 2); n+=2)			// return alternate
		{
			*(input++) = samples[n] & 0xFF;
			*(input++) = samples[n] >> 8;
		}
	}

//	if (ret == 0)
//		printf("AlsaRead returned no samples\n");

	return ret * 2;
}
