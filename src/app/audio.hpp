#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace mattrack {

enum class Sound { Chime, BreakEnd };

// Plays the embedded WAVs. Audio is optional: if no device is available,
// play() does nothing.
class Audio {
 public:
  Audio() = default;
  ~Audio() { shutdown(); }
  Audio(const Audio&) = delete;
  Audio& operator=(const Audio&) = delete;

  // Returns false (and logs) if audio is unavailable; the app carries on.
  bool init();
  // Releases everything; must run before SDL_Quit(). Safe to call twice.
  void shutdown();
  // Plays `sound` once.
  void play(Sound sound);
  // Repeats `sound` (each clip ends in a pause) until stop().
  void loop(Sound sound);
  void stop();
  // Keeps a loop fed, and pauses the device once playback has drained so an
  // idle app keeps no audio running. Call once per main-loop iteration.
  void update();

 private:
  struct Clip {
    Uint8* data = nullptr;
    Uint32 size = 0;
  };
  bool load(Clip& clip, const unsigned char* bytes, std::size_t size);
  const Clip& clip(Sound sound) const;

  SDL_AudioStream* stream_ = nullptr;
  SDL_AudioSpec spec_{};
  Clip chime_;
  Clip break_end_;
  const Clip* looping_ = nullptr;
  bool playing_ = false;
  bool subsystem_ = false;
};

}  // namespace mattrack
