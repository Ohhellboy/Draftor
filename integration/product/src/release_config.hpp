#pragma once
// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Ruotao Wang
#ifndef DRAFTOR_RELEASE_BUILD
#define DRAFTOR_RELEASE_BUILD 0
#endif
// TEST_ONLY: reuse the validated finite-device endpoint overlay and bounded UART
// queue in both arms. No telemetry module or full ordinary diagnostics enabled.
#ifndef DRAFTOR_AUTH_OBSERVE
#define DRAFTOR_AUTH_OBSERVE 0
#endif
