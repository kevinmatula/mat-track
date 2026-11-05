#include "app/audio.hpp"

#include <cstdio>

#include "app/assets.hpp"

namespace mattrack {

void Audio::shutdown() {
  if (stream_) SDL_DestroyAudioStream(stream_);
  stream_ = nullptr;
  SDL_free(chime_.data);
  SDL_free(break_end_.data);
  chime_ = {};
  break_end_ = {};
  looping_ = nullptr;
  playing_ = false;
  if (subsystem_) SDL_QuitSubSystem(SDL_INIT_AUDIO);
  subsystem_ = false;
}

bool Audio::load(Clip& clip, const unsigned char* bytes, std::size_t size) {
  SDL_AudioSpec spec;
  SDL_IOStream* io = SDL_IOFromConstMem(bytes, size);
  if (!io || !SDL_LoadWAV_IO(io, /*closeio=*/true, &spec, &clip.data, &clip.size)) {
    std::fprintf(stderr, "mat-track: cannot load sound: %s\n", SDL_GetError());
    return false;
  }
  // Both clips are generated with the same format (tools/gen_sounds.py).
  if (spec_.freq == 0) spec_ = spec;
  return spec.format == spec_.format && spec.channels == spec_.channels &&
         spec.freq == spec_.freq;
}

bool Audio::init() {
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    std::fprintf(stderr, "mat-track: no audio (%s); sounds disabled\n", SDL_GetError());
    return false;
  }
  subsystem_ = true;
  if (!load(chime_, assets::chime_wav, assets::chime_wav_size) ||
      !load(break_end_, assets::break_end_wav, assets::break_end_wav_size)) {
    return false;
  }
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec_, nullptr, nullptr);
  if (!stream_) {
    std::fprintf(stderr, "mat-track: cannot open audio device (%s); sounds disabled\n",
                 SDL_GetError());
    return false;
  }
  return true;  // the device starts paused
}

const Audio::Clip& Audio::clip(Sound sound) const {
  return sound == Sound::Chime ? chime_ : break_end_;
}

void Audio::play(Sound sound) {
  if (!stream_) return;
  looping_ = nullptr;
  SDL_ClearAudioStream(stream_);
  SDL_PutAudioStreamData(stream_, clip(sound).data, static_cast<int>(clip(sound).size));
  SDL_ResumeAudioStreamDevice(stream_);
  playing_ = true;
}

void Audio::loop(Sound sound) {
  play(sound);
  if (stream_) looping_ = &clip(sound);
}

void Audio::stop() {
  if (!stream_) return;
  looping_ = nullptr;
  SDL_ClearAudioStream(stream_);
  SDL_PauseAudioStreamDevice(stream_);
  playing_ = false;
}

void Audio::update() {
  if (!playing_) return;
  const int queued = SDL_GetAudioStreamQueued(stream_);
  if (looping_) {
    // Top up well before the queue runs dry; the main loop wakes at least
    // every 250 ms, and each clip is ~2 s.
    if (queued < static_cast<int>(looping_->size) / 2) {
      SDL_PutAudioStreamData(stream_, looping_->data, static_cast<int>(looping_->size));
    }
  } else if (queued == 0) {
    SDL_PauseAudioStreamDevice(stream_);
    playing_ = false;
  }
}

}  // namespace mattrack
