#ifndef CJFX_H
#define CJFX_H

/*---------------------------*/
/*---CJFX_Sound.h------------*/
/*---------------By Claude---*/
/*---------------------------*/
/*---请保证你在你的项目里----*/
/*---包含了CJFX_Sound.c和----*/
/*---.h文件------------------*/
/*---基于DPCM。--------------*/
/*---------------------------*/

// 初始化音频系统，程序启动时调用一次
int  CJFX_Init(void);

// 关闭音频系统，程序退出时调用
void CJFX_Quit(void);

// 播放声音（非阻塞）
// Data    : 单声道 16bit 48000Hz PCM 数据指针
// Location: 起始字节偏移
// Length  : 播放字节数
// panning : 0=纯左 128=居中 255=纯右
// 返回 0 成功，-1 槽满丢弃
int  CJFX_PlaySound(char* Data, int Location, int Length, unsigned char panning, unsigned char volume);

#endif
