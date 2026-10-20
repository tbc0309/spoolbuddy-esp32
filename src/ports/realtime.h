// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "bridge.h"
/* Called only by the REST worker. Socket callbacks only deliver parsed events. */
void sb_realtime_poll(bool connected, sb_transport_fn transport);
