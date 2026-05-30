#include "CJFX_Sound.h"
#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_RATE   48000
#define CHANNELS      2
#define BITS          16
#define BUF_BYTES     12288
#define BUF_COUNT     2
#define MAX_VOICES    8

// ── 声音槽 ────────────────────────────────────────────────
typedef struct {
    const char*   data;
    int           location;
    int           length;        // 字节数
    int           samplePointer; // bit 索引，最大 length*8
    unsigned char dac;           // 当前 DAC 值，0~255
    float         volL;
    float         volR;
    int           active;
} Voice;

static Voice    g_voices[MAX_VOICES];
static CRITICAL_SECTION g_voiceLock;

// ── waveOut ───────────────────────────────────────────────
static HWAVEOUT g_hwo;
static short    g_pcm[BUF_COUNT][BUF_BYTES / 2];  // [缓冲区][样本]
static WAVEHDR  g_hdr[BUF_COUNT];
static HANDLE   g_semFree;
static HANDLE   g_thread;
static int      g_running;

// ── 混音：把所有 active 的 Voice 混进 out，共 frames 帧 ──
static void mix(short* out, int frames){
    int* tmp = (int*)calloc(frames * 2, sizeof(int));
    int i, f;

    EnterCriticalSection(&g_voiceLock);
    for (i = 0; i < MAX_VOICES; i++) {        // 外层：遍历所有槽
        Voice* v = &g_voices[i];
        if (!v->active) continue;             // 跳过空槽，不是break

        for (f = 0; f < frames; f++) {        // 内层：每一帧
            int byteIndex = v->samplePointer / 8;
            if (byteIndex >= v->length) {
                v->active = 0;
                break;
            }
            int bit = (v->data[v->location + byteIndex] >> (v->samplePointer & 7)) & 1;
            bit ? v->dac ++ : v->dac --;
            v->samplePointer++;

            int s = ((int)v->dac - 128) * 256;
            tmp[f * 2 + 0] += (int)(s * v->volL);
            tmp[f * 2 + 1] += (int)(s * v->volR);
        }
    }
    LeaveCriticalSection(&g_voiceLock);

    for (f = 0; f < frames * 2; f++) {
        int s = tmp[f];
        if      (s >  32767) s =  32767;
        else if (s < -32768) s = -32768;
        out[f] = (short)s;
    }
    free(tmp);
}

// ── 后台线程：持续填充缓冲区并提交 ──────────────────────
static DWORD WINAPI audioThread(LPVOID param){
    int framesPerBuf = BUF_BYTES / (CHANNELS * BITS / 8);  // 1536
    int idx = 0;

    while (g_running) {
        WaitForSingleObject(g_semFree, INFINITE);
        if (!g_running) break;

        WAVEHDR* h = &g_hdr[idx];
        short*   p =  g_pcm[idx];

        mix(p, framesPerBuf);

        h->dwBufferLength = BUF_BYTES;
        h->dwFlags       &= ~WHDR_DONE;
        waveOutWrite(g_hwo, h, sizeof(WAVEHDR));

        idx = (idx + 1) % BUF_COUNT;
    }
    return 0;
}

static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT msg,
                                  DWORD_PTR instance,
                                  DWORD_PTR param1, DWORD_PTR param2){
    if (msg == WOM_DONE)
        ReleaseSemaphore(g_semFree, 1, NULL);
}

// ── 公开接口 ──────────────────────────────────────────────
int CJFX_Init(void){
    int i;
    WAVEFORMATEX wfx    = {0};
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels       = CHANNELS;
    wfx.nSamplesPerSec  = SAMPLE_RATE;
    wfx.wBitsPerSample  = BITS;
    wfx.nBlockAlign     = CHANNELS * BITS / 8;
    wfx.nAvgBytesPerSec = SAMPLE_RATE * wfx.nBlockAlign;
	
	
	
    InitializeCriticalSection(&g_voiceLock);
    memset(g_voices, 0, sizeof(g_voices));

    g_semFree = CreateSemaphore(NULL, BUF_COUNT, BUF_COUNT, NULL);

    if (waveOutOpen(&g_hwo, WAVE_MAPPER, &wfx,
                    (DWORD_PTR)waveOutProc, 0,
                    CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
        return -1;

    for (i = 0; i < BUF_COUNT; i++) {
        memset(&g_hdr[i], 0, sizeof(WAVEHDR));
        g_hdr[i].lpData         = (LPSTR)g_pcm[i];
        g_hdr[i].dwBufferLength = BUF_BYTES;
        waveOutPrepareHeader(g_hwo, &g_hdr[i], sizeof(WAVEHDR));
    }

    g_running = 1;
    g_thread  = CreateThread(NULL, 0, audioThread, NULL, 0, NULL);
    return 0;
}

void CJFX_Quit(void){
    int i;
    g_running = 0;
    ReleaseSemaphore(g_semFree, 1, NULL);  // 唤醒线程让它退出
    WaitForSingleObject(g_thread, 3000);
    CloseHandle(g_thread);

    waveOutReset(g_hwo);
    for (i = 0; i < BUF_COUNT; i++)
        waveOutUnprepareHeader(g_hwo, &g_hdr[i], sizeof(WAVEHDR));
    waveOutClose(g_hwo);

    CloseHandle(g_semFree);
    DeleteCriticalSection(&g_voiceLock);
}

int CJFX_PlaySound(char* Data, int Location, int Length, unsigned char panning, unsigned char volume){
    int i;
    float vol  = volume  / 255.0f;
    float pan  = panning / 255.0f;
    float volL = (1.0f - pan) * vol;
    float volR =          pan * vol;

    EnterCriticalSection(&g_voiceLock);
    for (i = 0; i < MAX_VOICES; i++) {
        if (!g_voices[i].active) {
            g_voices[i].data          = Data;
            g_voices[i].location      = Location;
            g_voices[i].length        = Length;
            g_voices[i].samplePointer = 0;
            g_voices[i].dac           = 127;
            g_voices[i].volL          = volL;
            g_voices[i].volR          = volR;
            g_voices[i].active        = 1;
            LeaveCriticalSection(&g_voiceLock);
            return 0;
        }
    }
    LeaveCriticalSection(&g_voiceLock);
    return -1;
}
