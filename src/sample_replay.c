/* KSSPLAY SAMPLE REPLAY PROGRAM (ISO/IEC 9899 Programming Language C ANSI C). */
/* Play KSS file. */
// reference file: vgmplay project's module Stream.c: C Source File for Sound Output
// Thanks to nextvolume for NetBSD support

#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#include <conio.h> // it has _kbhit and _getch
#include <windows.h>
#define _Bool BOOL
#define bool	_Bool
#define true	1
#define false	0
#else
#include <stdbool.h>
#include <limits.h> // for PATH_MAX
#include <termios.h>
#include <unistd.h> // for STDIN_FILENO and usleep()
#include <sys/time.h>   // for struct timeval in _kbhit()

// #include <WinBase.h>
// VOID WINAPI Sleep( _In_ DWORD dwMilliseconds );
// actually I found this
// WINBASEAPI VOID WINAPI Sleep( __in DWORD dwMilliseconds );
#if !defined(WIN32)
#define Sleep(msec) usleep(msec * 1000)
#endif

#include <sys/ioctl.h>
#include <fcntl.h>
#ifdef __NetBSD__
#include <sys/audioio.h>
#elif defined(__APPLE__)
// nothing
#else
#include <linux/soundcard.h>
#endif
#include <unistd.h>

#endif

#ifdef USE_LIBAO
#ifdef WIN32
#error "Sorry, but this doesn't work yet!"
#endif
#include <ao/ao.h>
#endif

#include <string.h>
#include "kssplay.h"

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

#define CHNL_NUM 1
#define SAMPLE_RATE 44100

#ifndef WIN32
typedef struct
{
    unsigned short wFormatTag;
    unsigned short nChannels;
    unsigned int nSamplesPerSec;
    unsigned int nAvgBytesPerSec;
    unsigned short nBlockAlign;
    unsigned short wBitsPerSample;
    unsigned short cbSize;
} WAVEFORMATEX; // from MSDN Help

#define WAVE_FORMAT_PCM 0x0001

#endif

// reference: vgmplay project's module Stream.h: Header File for constants and structures related to Sound Output
//

#ifdef WIN32
#include <mmsystem.h>
#else
#define MAX_PATH    PATH_MAX
#endif

#define SAMPLESIZE      (2)
#define BUFSIZE_MAX     (CHNL_NUM * SAMPLE_RATE * SAMPLESIZE) // Maximum Buffer Size in Bytes, i.e. 1 second of 16bits CHNL_NUM * SAMPLE_RATE
#ifndef WIN32
#define BUFSIZELD       11          // Buffer Size
#endif
#define AUDIOBUFFERS    200         // Maximum Buffer Count
//  Windows:    BufferBytesSize = SAMPLE_RATE / 100 * SAMPLESIZE (e.g. 44100 / 100 * 2 = 882 bytes)
//              1 Audio-Buffer = 10 msec, Min: 5
//              Win95- / WinVista-safe: 500 msec
//  Linux:      BufferBytesSize = 1 << BUFSIZELD (1 << 11 = 2048)
//              1 Audio-Buffer = 23.2 msec
unsigned int BufferBytesSize;  // Buffer Size in Bytes
unsigned short int neededLargeBuffers = 0U;
unsigned short int usedAudioBuffers = AUDIOBUFFERS;     // used AudioBuffers
volatile bool PauseThread;
volatile bool StreamPause;
bool ThreadPauseEnable;
volatile bool ThreadPauseConfrm;

#ifdef WIN32
WAVEFORMATEX WaveFmt;
static HWAVEOUT hWaveOut;
static WAVEHDR WaveHdrOut[AUDIOBUFFERS];
static HANDLE hWaveOutThread;
//static DWORD WaveOutCallbackThrID;
#else
static int hWaveOut;
static struct termios oldterm;
static bool termmode = false;
#endif
static bool WaveOutOpen = false; /* initialized to false */

unsigned int samplesPerBuffer;
static char BufferOut[AUDIOBUFFERS][BUFSIZE_MAX];
static volatile bool CloseThread;

KSSPLAY *kssplay;
KSS *kss;

unsigned int BlocksSent = 0U;
unsigned int BlocksPlayed = 0U;

#ifdef USE_LIBAO
ao_device* dev_ao;
#endif

/* functions prototypes */
static unsigned int StartStream(void);
static unsigned int StopStream(void);
static void PauseStream(bool PauseOn);
#ifdef WIN32
static DWORD WINAPI WaveOutThread(void* Arg);
static void BufCheck(void);
static void WinNT_Check(void);
#else
static void changemode(bool);
static int _kbhit(void);
static int _getch(void);
void WaveOutLinuxCallBack(void);
#endif

#ifdef WIN32
static void WinNT_Check(void)
{
    OSVERSIONINFO VerInf;

    VerInf.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&VerInf);
    //WINNT_MODE = (VerInf.dwPlatformId == VER_PLATFORM_WIN32_NT);

    /* Following Systems need larger Audio Buffers:
        - Windows 95 (500+ ms)
        - Windows Vista (200+ ms)
    Tested Systems:
        - Windows 95B
        - Windows 98 SE
        - Windows 2000
        - Windows XP (32-bit)
        - Windows Vista (32-bit)
        - Windows 7 (64-bit)
    */

    neededLargeBuffers = 0U;
    if (VerInf.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
    {
        if (VerInf.dwMajorVersion == 4 && VerInf.dwMinorVersion == 0)
            neededLargeBuffers = 50U;  // Windows 95
    }
    else if (VerInf.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
        if (VerInf.dwMajorVersion == 6 && VerInf.dwMinorVersion == 0)
            neededLargeBuffers = 20U;  // Windows Vista
    }
}
#else
static void changemode(bool dir)
{
    static struct termios newterm;

    if (termmode == dir)
        return;

    if (dir)
    {
        newterm = oldterm;
        newterm.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newterm);
    }
    else
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldterm);
    }
    termmode = dir;
}

static int _kbhit(void)
{
    struct timeval tv;
    fd_set rdfs;
    int kbret;
    int retval;

    tv.tv_sec = 0;
    tv.tv_usec = 5000;

    FD_ZERO(&rdfs);
    FD_SET(STDIN_FILENO, &rdfs);

    retval = select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv);
    if (retval == -1)
    {
        kbret = 0;
    }
    else if (retval)
    {
        kbret = FD_ISSET(STDIN_FILENO, &rdfs);
    }
    else
    {
        kbret = 0;
    }

    return kbret;
}

static int _getch(void)
{
    int ch;
    ch = getchar();
    return ch;
}
#endif

static unsigned int StartStream(void)
{
    unsigned int RetVal = 0xffffffff;
#ifdef USE_LIBAO
    ao_sample_format ao_fmt;
#else
#ifdef WIN32
    UINT16 Cnt;
    HANDLE WaveOutThreadHandle;
    DWORD WaveOutThreadID;
    //char TestStr[0x80];
#elif defined(__NetBSD__)
    struct audio_info AudioInfo;
#else
    unsigned int ArgVal;
#endif
#endif  // USE_LIBAO

    if (WaveOutOpen)
        return 0xD0;    // Thread is already active

#if defined(WIN32) || defined(USE_LIBAO)
#if defined(WIN32)
    // Init Audio
    WaveFmt.wFormatTag = WAVE_FORMAT_PCM;
    WaveFmt.nChannels = CHNL_NUM;
    WaveFmt.nSamplesPerSec = SAMPLE_RATE;
    WaveFmt.wBitsPerSample = 16;
    WaveFmt.nBlockAlign = ( WaveFmt.nChannels * WaveFmt.wBitsPerSample ) / 8;
    WaveFmt.nAvgBytesPerSec = WaveFmt.nBlockAlign * WaveFmt.nSamplesPerSec;
    WaveFmt.cbSize = 0;
#else
    memset(&ao_fmt, 0, sizeof(ao_sample_format));
#endif // WIN32

    BufferBytesSize = SAMPLE_RATE * SAMPLESIZE / 100; /* 1/100 s = 10 ms */
    if (BufferBytesSize > BUFSIZE_MAX)
        BufferBytesSize = BUFSIZE_MAX;
#else
    BufferBytesSize = 1 << BUFSIZELD;
#endif
    samplesPerBuffer = BufferBytesSize / SAMPLESIZE;
    if (usedAudioBuffers > AUDIOBUFFERS)
        usedAudioBuffers = AUDIOBUFFERS;

    PauseThread = true; /* in ISO/IEC 9899:1999 <stdbool.h> standard header */
    ThreadPauseConfrm = false; /* in ISO/IEC 9899:1999 <stdbool.h> standard header */
    CloseThread = false;
    StreamPause = false;

#ifndef USE_LIBAO
#ifdef WIN32
    ThreadPauseEnable = true;
    WaveOutThreadHandle = CreateThread(NULL, 0x00, &WaveOutThread, NULL, 0x00,
                                       &WaveOutThreadID);
    if(WaveOutThreadHandle == NULL)
        return 0xC8;        // CreateThread failed
    CloseHandle(WaveOutThreadHandle);

    if((MMRESULT)waveOutOpen(&hWaveOut, WAVE_MAPPER, &WaveFmt, 0x00, 0x00, CALLBACK_NULL) != (MMRESULT)MMSYSERR_NOERROR) /* MMRESULT waveOutOpen(...) */
#else
    ThreadPauseEnable = false;
#ifdef __NetBSD__
    hWaveOut = open("/dev/audio", O_WRONLY);
#else
    hWaveOut = open("/dev/dsp", O_WRONLY);
#endif
    if (hWaveOut < 0)
#endif
#else   // ifndef USE_LIBAO
    ao_initialize();

    ThreadPauseEnable = false;
    ao_fmt.bits = 16;
    ao_fmt.rate = SAMPLE_RATE;
    ao_fmt.channels = CHNL_NUM;
    ao_fmt.byte_format = AO_FMT_NATIVE;
    ao_fmt.matrix = NULL;

    dev_ao = ao_open_live(ao_default_driver_id(), &ao_fmt, NULL);
    if (dev_ao == NULL)
#endif
    {
        CloseThread = true;
        return 0xC0;        // waveOutOpen failed
    }
    WaveOutOpen = true;

    //sprintf(TestStr, "Buffer 0,0:\t%p\nBuffer 0,1:\t%p\nBuffer 1,0:\t%p\nBuffer 1,1:\t%p\n",
    //      &BufferOut[0][0], &BufferOut[0][1], &BufferOut[1][0], &BufferOut[1][1]);
    //AfxMessageBox(TestStr);
#ifndef USE_LIBAO
#ifdef WIN32
    for (Cnt = 0x00; Cnt < usedAudioBuffers; Cnt ++)
    {
        WaveHdrOut[Cnt].lpData = BufferOut[Cnt];    // &BufferOut[Cnt][0x00];
        WaveHdrOut[Cnt].dwBufferLength = BufferBytesSize;
        WaveHdrOut[Cnt].dwBytesRecorded = 0x00;
        WaveHdrOut[Cnt].dwUser = 0x00;
        WaveHdrOut[Cnt].dwFlags = 0x00;
        WaveHdrOut[Cnt].dwLoops = 0x00;
        WaveHdrOut[Cnt].lpNext = NULL;
        WaveHdrOut[Cnt].reserved = 0x00;
        RetVal = waveOutPrepareHeader(hWaveOut, &WaveHdrOut[Cnt], sizeof(WAVEHDR));
        WaveHdrOut[Cnt].dwFlags |= WHDR_DONE;
    }
#elif defined(__NetBSD__)
    AUDIO_INITINFO(&AudioInfo);

    AudioInfo.mode = AUMODE_PLAY;
    AudioInfo.play.sample_rate = SAMPLE_RATE;
    AudioInfo.play.channels = CHNL_NUM;
    AudioInfo.play.precision = 16;
    AudioInfo.play.encoding = AUDIO_ENCODING_SLINEAR;

    RetVal = ioctl(hWaveOut, AUDIO_SETINFO, &AudioInfo);
    if (RetVal)
        printf("Error setting audio information!\n");
#else
    ArgVal = (usedAudioBuffers << 16) | BUFSIZELD;
    RetVal = ioctl(hWaveOut, SNDCTL_DSP_SETFRAGMENT, &ArgVal);
    if (RetVal)
        printf("Error setting Fragment Size!\n");
    ArgVal = AFMT_S16_NE;
    RetVal = ioctl(hWaveOut, SNDCTL_DSP_SETFMT, &ArgVal);
    if (RetVal)
        printf("Error setting Format!\n");
    ArgVal = CHNL_NUM;
    RetVal = ioctl(hWaveOut, SNDCTL_DSP_CHANNELS, &ArgVal);
    if (RetVal)
        printf("Error setting Channels!\n");
    ArgVal = SAMPLE_RATE;
    RetVal = ioctl(hWaveOut, SNDCTL_DSP_SPEED, &ArgVal);
    if (RetVal)
        printf("Error setting Sample Rate!\n");
#endif
    RetVal = 0U;
#else
    RetVal = 0U;
#endif  // USE_LIBAO

    PauseThread = false;
    return RetVal;
}

static unsigned int StopStream(void)
{
    unsigned int RetVal;
#ifdef WIN32
    UINT16 Cnt;
#endif

    if (! WaveOutOpen)
        return 0xD8;    // Thread is not active

    CloseThread = true;
#ifdef WIN32
    for (Cnt = 0; Cnt < usedAudioBuffers; Cnt ++)
    {
        Sleep(1);
        if (hWaveOutThread == NULL)
            break;
    }
#endif
    WaveOutOpen = false;

#ifndef USE_LIBAO
#ifdef WIN32
    /* RetVal = */waveOutReset(hWaveOut);
    for (Cnt = 0x00; Cnt < usedAudioBuffers; Cnt ++)
        /* RetVal = */waveOutUnprepareHeader(hWaveOut, &WaveHdrOut[Cnt], sizeof(WAVEHDR));

    RetVal = waveOutClose(hWaveOut);
    if(RetVal != MMSYSERR_NOERROR)
        return 0xC4;        // waveOutClose failed  -- but why ???
#else
    RetVal = close(hWaveOut);
#endif
#else
    /* RetVal = */ao_close(dev_ao);

    /* RetVal = */ao_shutdown();
    RetVal = 0U;
#endif   // ifndef USE_LIBAO
    return RetVal;
}

static void PauseStream(bool PauseOn)
{
    if (! WaveOutOpen)
        return; // Thread is not active

#ifdef WIN32
    switch(PauseOn)
    {
    case true:
        waveOutPause(hWaveOut);
        break;
    case false:
        waveOutRestart(hWaveOut);
        break;
    }
    StreamPause = PauseOn;
#else
    switch(PauseOn)
    {
    case true:
        break;
    case false:
        break;
    }
    PauseThread = PauseOn;
#endif
}

#ifdef WIN32

static DWORD WINAPI WaveOutThread(void* Arg)
{
#ifdef NDEBUG
    DWORD RetVal;
#endif
    UINT16 CurBuf;

    UINT32 WrtSmpls;
    //char TestStr[0x80];
    bool DidBuffer = false; // a buffer was processed

    hWaveOutThread = GetCurrentThread();
#ifdef NDEBUG
    RetVal = SetThreadPriority(hWaveOutThread, THREAD_PRIORITY_TIME_CRITICAL);
    if (! RetVal)
    {
        // Error by setting priority
        // try a lower priority, because too low priorities cause sound stuttering
        RetVal = SetThreadPriority(hWaveOutThread, THREAD_PRIORITY_HIGHEST);
    }
#endif

    BlocksSent = 0U;
    BlocksPlayed = 0U;
    while(! CloseThread)
    {
        while(PauseThread && ! CloseThread && ! (StreamPause && DidBuffer))
        {
            ThreadPauseConfrm = true;
            Sleep(1);
        }
        if (CloseThread)
            break;

        BufCheck();
        DidBuffer = false;
        for (CurBuf = 0U; CurBuf < usedAudioBuffers; CurBuf ++)
        {
            if (WaveHdrOut[CurBuf].dwFlags & WHDR_DONE)
            {
                if (WaveHdrOut[CurBuf].dwUser & 0x01)
                    BlocksPlayed++;
                else
                    WaveHdrOut[CurBuf].dwUser |= 0x01;

                KSSPLAY_calc(kssplay, (int16_t *)WaveHdrOut[CurBuf].lpData, (uint32_t)samplesPerBuffer) ;

                WrtSmpls = samplesPerBuffer;

                WaveHdrOut[CurBuf].dwBufferLength = WrtSmpls * SAMPLESIZE;
                waveOutWrite(hWaveOut, &WaveHdrOut[CurBuf], sizeof(WAVEHDR));

                DidBuffer = true;
                BlocksSent++;
                BufCheck();
                //CurBuf = 0x00;
                //break;
            }
            if (CloseThread)
                break;
        }
        Sleep(1);
    }

    hWaveOutThread = NULL;
    return (DWORD)0x00000000;
}

static void BufCheck(void)
{
    UINT16 CurBuf;

    for (CurBuf = 0x00; CurBuf < usedAudioBuffers; CurBuf ++)
    {
        if (WaveHdrOut[CurBuf].dwFlags & WHDR_DONE)
        {
            if (WaveHdrOut[CurBuf].dwUser & 0x01)
            {
                WaveHdrOut[CurBuf].dwUser &= ~0x01;
                BlocksPlayed ++;
            }
        }
    }
}

#else // #ifdef WIN32

void WaveOutLinuxCallBack(void)
{
    unsigned int RetVal;
    unsigned int CurBuf;

    if (! WaveOutOpen)
        return; // Device not opened

    CurBuf = BlocksSent % usedAudioBuffers;
    if (PauseThread)
    {
        KSSPLAY_calc_silent(kssplay, (uint32_t)samplesPerBuffer);
    }
    else
    {
        KSSPLAY_calc(kssplay, (int16_t *)&BufferOut[CurBuf][0], (uint32_t)samplesPerBuffer) ;
        BlocksSent++;
    }
    BlocksPlayed++;

#ifndef USE_LIBAO
     RetVal = write(hWaveOut, (const void *)BufferOut[CurBuf], (size_t)(samplesPerBuffer * SAMPLESIZE));
#else
    /* RetVal = */ao_play(dev_ao, (char *)&BufferOut[CurBuf][0], (uint_32)(samplesPerBuffer * SAMPLESIZE));
#endif

#ifdef NDEBUG
    if (0 == (BlocksPlayed % 10))
    {
        printf("+10Blocks\n");
    }
#endif
 }

#endif // ifdef WIN32

static volatile bool done = false;

int main(int argc, char *argv[]) {
    const int rate = SAMPLE_RATE, nch = CHNL_NUM, bps = 16;
    int song_num = 0, play_time = 60, fade_time = 5, loop_num = 1, vol = 0;
    int data_length = 0, c;
    int quality = 0;
    unsigned char PlayingMode = 0U; // Normal Mode

    bool PosPrint = false;
    bool QuitPlay = false;
    bool PausePlay = false;
    unsigned int PlayTimeEnd;

    printf("Initializing ...\n");

#ifdef EMSCRIPTEN
    EM_ASM(FS.mkdir('/data'); FS.mount(NODEFS, {root : '.'}, '/data'););
#endif

#ifdef WIN32
    WinNT_Check();
#else
    tcgetattr(STDIN_FILENO, &oldterm);
    changemode(true);
#endif

    // PlayingMode = 0x00; Normal Mode
    // PlayingMode = 0x01; FM only Mode
    // PlayingMode = 0x02; Normal/FM mixed Mode (NOT in sync, Hardware is a lot faster)
    switch(PlayingMode)
    {
    case 0x00:
        usedAudioBuffers = 10;
        break;
    case 0x01:
        usedAudioBuffers = 0;   // no AudioBuffers needed
        break;
    case 0x02:
        usedAudioBuffers = 5;   // try to sync Hardware/Software Emulator as well as possible
        break;
    }
    if (usedAudioBuffers < neededLargeBuffers) {
        usedAudioBuffers = neededLargeBuffers;
    }

    printf("[KSSPLAY SAMPLE PROGRAM] q)uit p)rev n)ext spacebar for pause\n") ;
#ifdef NDEBUG
    printf("udesAudioBuffers %u\n",usedAudioBuffers) ;
#endif
    if (argc < 2) {
#if !defined(WIN32)
        changemode(false);
#endif
        printf("Usage: %s FILENAME\n",argv[0]) ;
        exit(-1);
    }

    KSS_load_kinrou("IN_MSX_PATHKINROU5.DRV") ;
    KSS_load_opxdrv("IN_MSX_PATHOPX4KSS.BIN") ;
    KSS_load_fmbios("IN_MSX_PATHFMPAC.ROM") ;
    KSS_load_mbmdrv("IN_MSX_PATHMBR143.001") ;
    KSS_load_mpk106("IN_MSX_PATHMPK.BIN") ;
    KSS_load_mpk103("IN_MSX_PATHMPK103.BIN") ;

    if((kss=KSS_load_file(argv[1]))== NULL)
    {
#if !defined(WIN32)
        changemode(false);
#endif
        printf("FILE READ ERROR!\n") ;
        exit(-1) ;
    }

    /* INIT KSSPLAY */
    kssplay = KSSPLAY_new(rate, nch, bps) ;
    KSSPLAY_set_data(kssplay, kss) ;

    /* Print title strings */
    printf("Vol = %d\n", (vol = kssplay->master_volume));
    printf("[%s]", kss->idstr);
    printf("%s\n", kss->title);
    if (kss->extra)
    {
        printf("%s\n", kss->extra);
    }
    else
    {
        printf("\n");
    }

    if (0U != StartStream())
    {
#if !defined(WIN32)
        changemode(false);
#endif
        printf("Error opening Sound Device!\n");
        return -1;
    }
    KSSPLAY_reset(kssplay, song_num, 0) ;

    KSSPLAY_set_device_quality(kssplay, EDSC_PSG, quality);
    KSSPLAY_set_device_quality(kssplay, EDSC_SCC, quality);
    KSSPLAY_set_device_quality(kssplay, EDSC_OPLL, quality);

    PlayTimeEnd = 0U;
    QuitPlay = false;
    PosPrint = true;
    while(! QuitPlay)
    {
        if (PosPrint)
        {
            PosPrint = false;

#ifdef WIN32
            printf("Playing %01.2f%%\t", 100.0);
#else
            // \t doesn't display correctly under Linux
            // but \b causes flickering under Windows
            printf("Playing %01.2f%%   \b\b\b\t", 100.0);
            fflush(stdout);
#endif
            /* PlaySmpl = BlocksPlayed * samplesPerBuffer; */
        }
#ifndef WIN32
        WaveOutLinuxCallBack();
#endif

        if(_kbhit()) {
            c = _getch() ;
            switch(c)
            {
                case 'q' :
                    goto quit ;
                case 'n' :
                    song_num = (song_num+1)&0xff ;
                    KSSPLAY_reset(kssplay, song_num, 0) ;
                    break ;
                case 'N' :
                    song_num = (song_num+10)&0xff ;
                    KSSPLAY_reset(kssplay, song_num, 0) ;
                    break ;
                case 'p' :
                    song_num = (song_num+0xff)&0xff ;
                    KSSPLAY_reset(kssplay, song_num, 0) ;
                    break ;
                case 'P' :
                    song_num = (song_num+0xff-9)&0xff ;
                    KSSPLAY_reset(kssplay, song_num, 0) ;
                    break ;
                case ' ' :
                    PausePlay = !PausePlay ;
                    PauseStream(PausePlay);
                    if (PausePlay)
                    {
                        KSSPLAY_set_master_volume(kssplay, -100);
                    }
                    else
                    {
                        KSSPLAY_set_master_volume(kssplay, vol);
                    }
                    break ;
            }
            printf("[$%02x(%d)]",song_num,song_num) ;
#if !defined(WIN32)
            fflush(stdout);
#endif
        }
    }
    done = false;

quit:
#if !defined(WIN32)
    changemode(false);
#endif

    KSSPLAY_delete(kssplay);
    KSS_delete(kss);
 
    if (0U != StopStream())
    {
        printf("Error closing Sound Device!\n");
        return -1;
    }

    return 0;
}

