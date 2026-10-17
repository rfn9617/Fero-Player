#pragma once

// Keeps the default Windows audio device awake with a silent shared-mode
// stream, so starting a video does not wait for the sound driver to leave its
// power-saving state (about 0.6 s on Realtek). The stream stops by itself after
// a period without activity and is restarted on the next touch().
namespace AudioWarmup {
void touch();   // start, or extend the keep-awake period
void stop();    // stop now (exclusive audio, shutdown)
} // namespace AudioWarmup
