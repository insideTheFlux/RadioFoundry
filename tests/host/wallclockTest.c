// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// Time of day (core/clock/wallclock): the calendar math against the host's timegm / gmtime_r on every day from 1970 to
// 2105 (leap years, the 2100 rule), UTC offsets, the GPS date and time, and the software clock's counter wrap
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "clock/wallclock.h"

static int failures, checks;

#define CHECK(cond) do { checks++; if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static int same_as_host(int64_t t, int offset_min)
{
	time_t tt = (time_t)(t + offset_min * 60);
	struct tm g;
	gmtime_r(&tt, &g);
	wc_civil_t c;
	wc_civil_from_unix(t, offset_min, &c);
	return c.year == g.tm_year + 1900 && c.month == g.tm_mon + 1 && c.day == g.tm_mday && c.hour == g.tm_hour &&
	       c.min == g.tm_min && c.sec == g.tm_sec && c.wday == g.tm_wday;
}

int main(void)
{
	// Every day from 1970-01-01 to 2105-12-31, at three times of day, and the way back
	long bad = 0, days = 0;
	for (int64_t d = 0; d <= 49400; d++, days++) {
		static const int tod[3] = { 0, 43200 + 17, 86399 };
		for (int i = 0; i < 3; i++) {
			int64_t t = d * 86400 + tod[i];
			wc_civil_t c;
			int64_t back;
			wc_civil_from_unix(t, 0, &c);
			if (!same_as_host(t, 0) || !wc_unix_from_civil(&c, &back) || back != t)
				bad++;
		}
	}
	CHECK(bad == 0);
	CHECK(days == 49401);

	// Every 15 minute UTC offset, over days around the leap days and the year ends
	static const int64_t pts[] = { 0, 951782400 /* 2000-02-29 */, 4107542400LL /* 2100-03-01 */, 1767225599 /* 2025-12-31 23:59:59 */,
	                               1770000000, 1795024616, 2147483647LL, 4102444800LL /* 2100-01-01 */ };
	bad = 0;
	for (unsigned p = 0; p < sizeof pts / sizeof pts[0]; p++)
		for (int off = WC_OFFSET_MIN_MIN; off <= WC_OFFSET_MIN_MAX; off += 15)
			if (!same_as_host(pts[p] + 86400, off))
				bad++;
	CHECK(bad == 0);

	// 1970-01-01 was a Thursday; 2000 was a leap year and 2100 is not
	wc_civil_t c;
	wc_civil_from_unix(0, 0, &c);
	CHECK(c.year == 1970 && c.month == 1 && c.day == 1 && c.wday == 4);
	wc_civil_from_unix(951782400, 0, &c);			// 2000-02-29
	CHECK(c.year == 2000 && c.month == 2 && c.day == 29);
	wc_civil_from_unix(4107542400LL - 86400, 0, &c);	// the day before 2100-03-01
	CHECK(c.year == 2100 && c.month == 2 && c.day == 28);
	wc_civil_from_unix(0, -300, &c);				// before the epoch after the offset: held at its start
	CHECK(c.year == 1970 && c.month == 1 && c.day == 1 && c.hour == 0);
	wc_civil_from_unix(-5, 0, &c);
	CHECK(c.year == 1970 && c.sec == 0);

	// A day across the date line either way
	wc_civil_from_unix(1767225599, 14 * 60, &c);		// 2025-12-31 23:59:59 UTC is 14:00 into 2026-01-01 at +14:00
	CHECK(c.year == 2026 && c.month == 1 && c.day == 1 && c.hour == 13 && c.min == 59);
	wc_civil_from_unix(1767225600, -12 * 60, &c);		// 2026-01-01 00:00:00 UTC at -12:00: 2025-12-31 12:00
	CHECK(c.year == 2025 && c.month == 12 && c.day == 31 && c.hour == 12);

	// Fields out of range
	int64_t u;
	wc_civil_t bad_c[] = {
		{ 2023, 2, 29, 0, 0, 0, 0 }, { 2024, 2, 30, 0, 0, 0, 0 }, { 2026, 0, 1, 0, 0, 0, 0 }, { 2026, 13, 1, 0, 0, 0, 0 },
		{ 2026, 4, 31, 0, 0, 0, 0 }, { 2026, 1, 0, 0, 0, 0, 0 }, { 2026, 1, 1, 24, 0, 0, 0 }, { 2026, 1, 1, 0, 60, 0, 0 },
		{ 2026, 1, 1, 0, 0, 60, 0 }, { 1969, 12, 31, 0, 0, 0, 0 },
	};
	for (unsigned i = 0; i < sizeof bad_c / sizeof bad_c[0]; i++)
		CHECK(!wc_unix_from_civil(&bad_c[i], &u));
	wc_civil_t leapday = { 2024, 2, 29, 12, 0, 0, 0 };
	CHECK(wc_unix_from_civil(&leapday, &u) && u == 1709208000);

	// The date and time a GPS fix carries
	CHECK(wc_unix_from_nmea(123519, 230326, &u) && u == 1774269319);		// 2026-03-23 12:35:19 (two digit years are 20yy)
	CHECK(wc_unix_from_nmea(0, 10100, &u) && u == 946684800);			// 2000-01-01 00:00:00
	CHECK(wc_unix_from_nmea(235959, 311299, &u) && u == 4102444799LL);	// 99 is 2099
	CHECK(!wc_unix_from_nmea(0xFFFFFFFFu, 230394, &u));	// no time
	CHECK(!wc_unix_from_nmea(123519, 0, &u));		// no date (a fixed position, or GGA)
	CHECK(!wc_unix_from_nmea(240000, 230394, &u));
	CHECK(!wc_unix_from_nmea(126019, 230394, &u));
	CHECK(!wc_unix_from_nmea(123519, 310226, &u));		// 31 February
	CHECK(!wc_unix_from_nmea(123519, 231394, &u));		// month 13

	// The software clock: never set, set, sub-second part kept, counter wrap, a long run
	wc_clear();
	CHECK(!wc_now_utc(5000, &u));
	CHECK(!wc_now_local(5000, 0, &c));
	wc_set_utc(1000000, 1000);
	CHECK(wc_now_utc(1900, &u) && u == 1000000);		// 0.9 s: not yet
	CHECK(wc_now_utc(2100, &u) && u == 1000001);		// 1.1 s
	CHECK(wc_now_utc(3050, &u) && u == 1000002);		// 2.05 s: the parts left over by the first reads count
	wc_set_utc(2000000, 0xFFFFFF00u);			// 256 ms before the counter wraps
	CHECK(wc_now_utc(5000, &u) && u == 2000005);		// 5.256 s across the wrap
	wc_set_utc(3000000, 0xFFFFFC00u);
	uint32_t ms = 0xFFFFFC00u;
	int64_t want = 3000000;
	int ok = 1;
	for (int i = 0; i < 400; i++) {				// read every 10 days for 4000 days
		ms += 10u * 86400u * 1000u;
		want += 10 * 86400;
		if (!wc_now_utc(ms, &u) || u != want)
			ok = 0;
	}
	CHECK(ok);
	wc_set_utc(1795024616, 0);				// 2026-11-18 ... local time at two offsets
	CHECK(wc_now_local(60000, 0, &c) && c.year == 2026 && c.month == 11 && c.day == 18 && c.hour == 17 && c.min == 57 && c.sec == 56);
	CHECK(wc_now_local(60000, -8 * 60, &c) && c.day == 18 && c.hour == 9 && c.min == 57 && c.sec == 56);
	wc_clear();
	CHECK(!wc_now_utc(0, &u));

	// Text
	char b[16];
	wc_civil_t t1 = { 2026, 11, 18, 14, 32, 5, 3 }, t2 = { 2026, 11, 18, 0, 5, 0, 3 }, t3 = { 2026, 11, 18, 12, 0, 0, 3 },
	           t4 = { 2026, 11, 18, 13, 7, 0, 3 }, t5 = { 2026, 11, 18, 9, 5, 0, 3 };
	CHECK(wc_fmt_hm(b, sizeof b, &t1, true) == 5 && !strcmp(b, "14:32"));
	CHECK(wc_fmt_hm(b, sizeof b, &t2, true) == 5 && !strcmp(b, "00:05"));
	CHECK(wc_fmt_hm(b, sizeof b, &t5, true) == 5 && !strcmp(b, "09:05"));
	CHECK(wc_fmt_hm(b, sizeof b, &t1, false) == 7 && !strcmp(b, "2:32 PM"));
	CHECK(wc_fmt_hm(b, sizeof b, &t2, false) == 8 && !strcmp(b, "12:05 AM"));
	CHECK(wc_fmt_hm(b, sizeof b, &t3, false) == 8 && !strcmp(b, "12:00 PM"));
	CHECK(wc_fmt_hm(b, sizeof b, &t4, false) == 7 && !strcmp(b, "1:07 PM"));
	CHECK(wc_fmt_hm(b, 4, &t1, true) == 3 && !strcmp(b, "14:"));		// truncated, still terminated

	printf("# wallclockTest: %d checks, %s\n", checks, failures ? "FAILED" : "PASSED");
	return failures != 0;
}
