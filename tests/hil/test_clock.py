# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
"""The time of day in the status strip (docs/spec/radio-features.md 5.6, core/clock): the clock follows the GPS time.
Nothing transmits and no setting is changed.

The debug link's UI view says what the strip shows (ui.clock_mode, ui.clock_hm, ui.utc_off_min). The test runs where
Settings > Display > Clock is on (24 or 12 hour) and the board GPS has a fix: the time shown is this computer's UTC
time plus the radio's Time zone setting, to within two minutes. Without the clock built, switched on or a fix, it
skips and says which.
"""
import datetime

import pytest

from rfoundry_hil import as_int, skip

NONE = 0xFFFF
TOLERANCE_MIN = 2


@pytest.mark.feature("clock")
def test_clock_follows_gps(radio):
    v = radio.view(["link"])
    if "ui.clock_hm" not in v:
        skip("not_built", "built without CONFIG_CLOCK, or a build before the clock's debug link keys")
    if as_int(v["ui.clock_mode"], 0) == 0:
        skip("state", "Settings > Display > Clock is Off")
    pos = radio.view(["aprs"])
    if pos.get("pos.gps_fix") != "1":
        skip("state", "no GPS fix yet: the clock waits for the date and time of one (take the radio where it sees the sky)")
    shown = as_int(v["ui.clock_hm"], NONE)
    assert shown != NONE, "the clock is on and the GPS has a fix, but no time is shown"
    now = datetime.datetime.now(datetime.timezone.utc)
    want = (now.hour * 60 + now.minute + as_int(v["ui.utc_off_min"], 0)) % 1440
    diff = min((shown - want) % 1440, (want - shown) % 1440)
    assert diff <= TOLERANCE_MIN, f"the radio shows {shown // 60:02d}:{shown % 60:02d}, this computer says {want // 60:02d}:{want % 60:02d}"
