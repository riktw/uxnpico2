#include <time.h>
#include "datetime.h"
#include <stdio.h>

extern uint32_t instructionCount;

static time_t seconds = 0;
static struct tm *datetime_t;

void SecondPasssed()
{
	seconds++;
	unsigned int ips = (instructionCount / seconds);
	volatile uint32_t secs = seconds;
	// printf("Sec: %li ", secs);
	// printf("Instr: %i ", instructionCount);
	// printf("IPS: %li \n", ips);
}

void datetime_update()
{
	struct tm *t = localtime(&seconds);
	datetime_t = t;
}

Uint8 datetime_dei_yhb(void)
{
	datetime_update();
	return (datetime_t->tm_year + 1900) >> 8;
}
Uint8 datetime_dei_ylb(void)
{
	datetime_update();
	return (datetime_t->tm_year + 1900);
}
Uint8 datetime_dei_mon(void)
{
	datetime_update();
	return datetime_t->tm_mon;
}
Uint8 datetime_dei_day(void)
{
	datetime_update();
	return datetime_t->tm_mday;
}
Uint8 datetime_dei_hou(void)
{
	datetime_update();
	return datetime_t->tm_hour;
}
Uint8 datetime_dei_min(void)
{
	datetime_update();
	return datetime_t->tm_min;
}
Uint8 datetime_dei_sec(void)
{
	datetime_update();
	return datetime_t->tm_sec;
}
Uint8 datetime_dei_wday(void)
{
	datetime_update();
	return datetime_t->tm_wday;
}
Uint8 datetime_dei_ydayhb(void)
{
	datetime_update();
	return datetime_t->tm_yday >> 8;
}
Uint8 datetime_dei_ydaylb(void)
{
	datetime_update();
	return datetime_t->tm_yday;
}
Uint8 datetime_dei_dst(void)
{
	datetime_update();
	return datetime_t->tm_isdst;
}