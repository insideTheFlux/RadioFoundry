// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// Time of day (docs/spec/radio-features.md 5.6): date and time from a Unix time and a UTC offset, the date and time a
// GPS fix carries, and a software clock that keeps running from the last time it was set. It knows no board and no
// hardware clock: a board's RTC, when there is one, gives the same Unix time through wc_set_utc(). The clock stays
// unset until something sets it, so the radio never shows a made-up time.
#ifndef RFOUNDRY_CLOCK_WALLCLOCK_H
#define RFOUNDRY_CLOCK_WALLCLOCK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
	uint16_t year;		// 1970 and later
	uint8_t month;		// 1..12
	uint8_t day;		// 1..31
	uint8_t hour;		// 0..23
	uint8_t min;		// 0..59
	uint8_t sec;		// 0..59
	uint8_t wday;		// 0 Sunday .. 6 Saturday
} wc_civil_t;

// UTC offsets are in minutes: the real ones run from -12:00 to +14:00, in steps of 15 minutes
#define WC_OFFSET_MIN_MIN	(-12 * 60)
#define WC_OFFSET_MIN_MAX	(14 * 60)

// Seconds since 1970-01-01 00:00:00 UTC, shifted by offset_min, as a calendar date and time. Negative Unix times
// give 1970-01-01 00:00:00.
void wc_civil_from_unix(int64_t unix_s, int offset_min, wc_civil_t *out);
// The inverse (no offset). False for a field out of range (month 13, February 30) or a year before 1970.
bool wc_unix_from_civil(const wc_civil_t *c, int64_t *unix_s);
// The UTC time and date of a GPS fix as an NMEA sentence carries them: hhmmss and ddmmyy (nmea_fix_t utc_hms and date).
// False for an unknown value (0xFFFFFFFF, date 0) or a field out of range. The two-digit year is 2000 + yy.
bool wc_unix_from_nmea(uint32_t utc_hms, uint32_t ddmmyy, int64_t *unix_s);

// The software clock. now_ms is the millisecond counter (hal_millis()); it may wrap, as long as the clock is read at
// least once in 49 days.
void wc_set_utc(int64_t unix_s, uint32_t now_ms);
// The UTC Unix time now. False while the clock was never set.
bool wc_now_utc(uint32_t now_ms, int64_t *unix_s);
// The local date and time now at the given offset. False while the clock was never set.
bool wc_now_local(uint32_t now_ms, int offset_min, wc_civil_t *out);
// Forget the time (a test, or a source that turned out wrong).
void wc_clear(void);

// "14:32" (h24), or "2:32 PM" (12 hour, no leading zero, 12:xx for the hour after midnight and noon). Returns the
// length; the buffer needs at least 9 bytes.
size_t wc_fmt_hm(char *buf, size_t n, const wc_civil_t *c, bool h24);

#endif
