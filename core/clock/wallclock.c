// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// Time of day: see wallclock.h
#include <stdio.h>
#include "clock/wallclock.h"

// Days since 1970-01-01 of a calendar date, and the date of a day count: the proleptic Gregorian calendar counted
// in 400 year eras of 146097 days, which makes the leap rule (every 4, not every 100, but every 400) one division
static int64_t days_from_civil(int64_t y, unsigned m, unsigned d)
{
	y -= m <= 2;
	int64_t era = (y >= 0 ? y : y - 399) / 400;
	unsigned yoe = (unsigned)(y - era * 400);			// 0..399
	unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;	// day of the year, March based: 0..365
	unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;		// 0..146096
	return era * 146097 + (int64_t)doe - 719468;
}

static void civil_from_days(int64_t z, int64_t *y, unsigned *m, unsigned *d)
{
	z += 719468;
	int64_t era = (z >= 0 ? z : z - 146096) / 146097;
	unsigned doe = (unsigned)(z - era * 146097);
	unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	int64_t yy = (int64_t)yoe + era * 400;
	unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	unsigned mp = (5 * doy + 2) / 153;
	*d = doy - (153 * mp + 2) / 5 + 1;
	*m = mp < 10 ? mp + 3 : mp - 9;
	*y = yy + (*m <= 2);
}

static bool leap(unsigned y)
{
	return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

static unsigned days_in_month(unsigned y, unsigned m)
{
	static const uint8_t dm[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	return m == 2 && leap(y) ? 29 : dm[m - 1];
}

void wc_civil_from_unix(int64_t unix_s, int offset_min, wc_civil_t *out)
{
	if (unix_s < 0)
		unix_s = 0;
	int64_t t = unix_s + (int64_t)offset_min * 60;
	if (t < 0)
		t = 0;
	int64_t days = t / 86400;
	unsigned sod = (unsigned)(t % 86400);
	int64_t y;
	unsigned m, d;
	civil_from_days(days, &y, &m, &d);
	out->year = (uint16_t)y;
	out->month = (uint8_t)m;
	out->day = (uint8_t)d;
	out->hour = (uint8_t)(sod / 3600);
	out->min = (uint8_t)(sod % 3600 / 60);
	out->sec = (uint8_t)(sod % 60);
	out->wday = (uint8_t)((days + 4) % 7);		// 1970-01-01 was a Thursday
}

bool wc_unix_from_civil(const wc_civil_t *c, int64_t *unix_s)
{
	if (c->year < 1970 || c->month < 1 || c->month > 12 || c->day < 1 || c->day > days_in_month(c->year, c->month) ||
	    c->hour > 23 || c->min > 59 || c->sec > 59)
		return false;
	*unix_s = days_from_civil(c->year, c->month, c->day) * 86400 + c->hour * 3600 + c->min * 60 + c->sec;
	return true;
}

bool wc_unix_from_nmea(uint32_t utc_hms, uint32_t ddmmyy, int64_t *unix_s)
{
	if (utc_hms > 235959 || ddmmyy == 0 || ddmmyy > 311299)
		return false;
	wc_civil_t c = {
		.year = (uint16_t)(2000 + ddmmyy % 100), .month = (uint8_t)(ddmmyy / 100 % 100), .day = (uint8_t)(ddmmyy / 10000),
		.hour = (uint8_t)(utc_hms / 10000), .min = (uint8_t)(utc_hms / 100 % 100), .sec = (uint8_t)(utc_hms % 100),
	};
	return wc_unix_from_civil(&c, unix_s);
}

// The clock: the Unix time at the moment it was set (base_unix at base_ms), advanced by whole seconds on each read so
// the 32 bit millisecond counter can wrap between reads
static struct {
	bool set;
	int64_t base_unix;
	uint32_t base_ms;
} clk;

void wc_set_utc(int64_t unix_s, uint32_t now_ms)
{
	clk.set = true;
	clk.base_unix = unix_s;
	clk.base_ms = now_ms;
}

bool wc_now_utc(uint32_t now_ms, int64_t *unix_s)
{
	if (!clk.set)
		return false;
	uint32_t secs = (now_ms - clk.base_ms) / 1000;
	clk.base_unix += secs;
	clk.base_ms += secs * 1000u;		// keeps the sub-second part in the difference
	*unix_s = clk.base_unix;
	return true;
}

bool wc_now_local(uint32_t now_ms, int offset_min, wc_civil_t *out)
{
	int64_t u;
	if (!wc_now_utc(now_ms, &u))
		return false;
	wc_civil_from_unix(u, offset_min, out);
	return true;
}

void wc_clear(void)
{
	clk.set = false;
}

size_t wc_fmt_hm(char *buf, size_t n, const wc_civil_t *c, bool h24)
{
	int r;
	if (h24) {
		r = snprintf(buf, n, "%02u:%02u", c->hour, c->min);
	} else {
		unsigned h = c->hour % 12;
		r = snprintf(buf, n, "%u:%02u %s", h ? h : 12, c->min, c->hour < 12 ? "AM" : "PM");
	}
	return r < 0 ? 0 : (size_t)r < n ? (size_t)r : n - 1;
}
