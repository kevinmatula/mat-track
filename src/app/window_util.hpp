#pragma once

#include <SDL3/SDL.h>

namespace mattrack {

// Restores the window if minimized, raises it and gives it focus, as if the
// user had clicked it in the taskbar.
//
// SDL_RestoreWindow / SDL_RaiseWindow ask the window manager as the
// application itself, which focus-stealing prevention (e.g. Cinnamon/Muffin)
// refuses once the user has been working in another app: the window just
// flashes and stays minimized. On X11 we therefore send the EWMH activation
// request with the "pager" source, which window managers always honour.
void bring_to_front(SDL_Window* window);

// Sets the window / taskbar icon from the embedded assets/icon.bmp.
void set_window_icon(SDL_Window* window);

}  // namespace mattrack
