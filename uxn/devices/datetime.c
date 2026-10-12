#include <time.h>
#include "datetime.h"
#include <stdio.h>

extern Uint8 dev[0x100];

static DateTime dateTime;
static bool updateNeeded = false;

void datetime_update()
{
	if (updateNeeded)
	{
		updateNeeded = false;
		ds1302setDateTime(&dateTime);
	}
	ds1302getDateTime(&dateTime);
}

Uint8 datetime_dei_yhb(void)
{

	return (dateTime.year + 1900) >> 8;
}
Uint8 datetime_dei_ylb(void)
{
	return (dateTime.year + 1900);
}
Uint8 datetime_dei_mon(void)
{
	return dateTime.month;
}
Uint8 datetime_dei_day(void)
{
	return dateTime.day;
}
Uint8 datetime_dei_hou(void)
{
	return dateTime.hour;
}
Uint8 datetime_dei_min(void)
{
	return dateTime.minute;
}
Uint8 datetime_dei_sec(void)
{
	return dateTime.second;
}
Uint8 datetime_dei_wday(void)
{
	return dateTime.dow;
}
Uint8 datetime_dei_ydayhb(void)
{
	return 0;
}
Uint8 datetime_dei_ydaylb(void)
{
	return 0;
}
Uint8 datetime_dei_dst(void)
{
	return 0;
}

void datetime_deo_year(void)
{
	dateTime.year = ((dev[0xc0] << 8) | dev[0xc1]) - 1900;
	updateNeeded = true;
}

void datetime_deo_mon(void)
{
	dateTime.month = dev[0xc2];
	updateNeeded = true;
}

void datetime_deo_day(void)
{
	dateTime.day = dev[0xc3];
	updateNeeded = true;
}

void datetime_deo_hou(void)
{
	dateTime.hour = dev[0xc4];
	updateNeeded = true;
}

void datetime_deo_min(void)
{
	dateTime.minute = dev[0xc5];
	updateNeeded = true;
}

void datetime_deo_sec(void)
{
	dateTime.second = dev[0xc6];
	updateNeeded = true;
}

void datetime_deo_wday(void)
{
	dateTime.dow = dev[0xc7];
	updateNeeded = true;
}

void datetime_deo_yearday(void)
{
}

void datetime_deo_dst(void)
{
}
