// SPDX-License-Identifier: MIT
#pragma once
// Starts Switch networking and HTTPS once for the whole process. The updater, online
// cheats and cover downloads share it, so one finishing never switches the network
// off underneath another. It is shut down at exit. Thread-safe; retries after failure.
bool networkStart();
