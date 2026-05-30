#include "CJFX_Sound.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "DPCM_L.c"
#include "DPCM_R.c"



#define SAMPLE_RATE 48000
#define PI 3.14159265358979323846

// 把正弦波编码成 DPCM
// 返回字节数
int makeDPCM(double freq, double seconds, unsigned char** out)
{
    int totalBits  = (int)(SAMPLE_RATE * seconds);
    int totalBytes = (totalBits + 7) / 8;
    unsigned char* buf = (unsigned char*)calloc(totalBytes, 1);

    unsigned char dac = 127;
    int b;
    for (b = 0; b < totalBits; b++) {
        // 目标值（正弦波，0~255）
        double t      = (double)b / SAMPLE_RATE;
        int    target = (int)(sin(2.0 * PI * freq * t) * 100.0 + 127.0);

        int bit;
        if (dac < target) { dac++; bit = 1; }
        else              { dac--; bit = 0; }

        if (bit)
            buf[b / 8] |= (1 << (b & 7));
    }

    *out = buf;
    return totalBytes;
}

int main(void)
{
    unsigned char* sound1 = NULL;
    unsigned char* sound2 = NULL;

    int len1 = makeDPCM(440.0, 2.0, &sound1);  // 440Hz 2秒
    int len2 = makeDPCM(880.0, 2.0, &sound2);  // 880Hz 2秒

    if (CJFX_Init() != 0) {
        fprintf(stderr, "CJFX_Init failed\n");
        return 1;
    }

    // 440Hz 居中，全音量
    CJFX_PlaySound(L, 0, sizeof(L), 0, 255);
    CJFX_PlaySound(R, 0, sizeof(R), 255, 255);
    printf("正在播放：バカみたいに");

    getchar();

    CJFX_Quit();
    free(sound1);
    free(sound2);
    printf("完毕\n");
    return 0;
}
