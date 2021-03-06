/*Copyright 2015 Huawei Technologies Co., Ltd. All rights reserved.
eSDK is licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
		http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include "ivs_timer.h"
#include "ivs_error.h"
#include "ivs_error.h"
#include <Windows.h>
#include "ivs_log.h"
#pragma comment(lib,"Winmm.lib")

#define TARGET_RESOLUTION 1         // 1-millisecond target resolution

#define TIMER_ACCURACY 2

int timerID;

//����10���ӷ�����ת
unsigned long timeTick = 0xFFFF159F;

static void CALLBACK  TimerCallBack(UINT uTimerID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
    timeTick += TIMER_ACCURACY;
}

int  IVS_StarTimer()
{
    TIMECAPS tc;

    UINT wTimerRes;

    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR)
    {
        return IVS_FAIL;
    }

    wTimerRes = min(max(tc.wPeriodMin, TARGET_RESOLUTION), tc.wPeriodMax);

    timeBeginPeriod(wTimerRes);

    timerID = timeSetEvent(TIMER_ACCURACY, wTimerRes, TimerCallBack, NULL, TIME_PERIODIC);
    if (!timerID)
    {
        return IVS_FAIL;
    }

    return IVS_SUCCEED;
}

void  IVS_StopTimer()
{
    if (timerID)
    {
        // is timer event pending?
        timeKillEvent(timerID);  // cancel the event
        timerID = 0;
    }
}

unsigned long  IVS_GetTimeTick()
{
    return timeTick;
}

unsigned long long IVS_GetSysTickUs(void)
{
	LARGE_INTEGER counter, freq;
	QueryPerformanceCounter(&counter);
	QueryPerformanceFrequency(&freq);

	lldiv_t d = lldiv (counter.QuadPart, freq.QuadPart);
	return (d.quot * 1000000) + ((d.rem * 1000000) / freq.QuadPart);
}