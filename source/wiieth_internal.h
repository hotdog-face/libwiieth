// libwiieth internals shared between the source files (not installed).
// Copyright (c) 2026 Mitchell Vizensky. SPDX-License-Identifier: MIT
#ifndef WIIETH_INTERNAL_H
#define WIIETH_INTERNAL_H

void wiieth_logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));   // -> wiieth_set_log's callback

#endif
