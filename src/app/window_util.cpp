#include "app/window_util.hpp"

#include <cstring>

#ifdef MAT_TRACK_HAVE_X11
#include <X11/Xlib.h>
#endif

namespace mattrack {

namespace {

#ifdef MAT_TRACK_HAVE_X11
bool activate_x11(SDL_Window* window) {
  const char* driver = SDL_GetCurrentVideoDriver();
  if (!driver || std::strcmp(driver, "x11") != 0) return false;

  const SDL_PropertiesID props = SDL_GetWindowProperties(window);
  auto* dpy = static_cast<Display*>(
      SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
  const auto xwin =
      static_cast<Window>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
  if (!dpy || !xwin) return false;

  // _NET_ACTIVE_WINDOW: data.l[0] = source indication (2 = pager/taskbar),
  // data.l[1] = timestamp, data.l[2] = our currently active window (none).
  XEvent ev{};
  ev.xclient.type = ClientMessage;
  ev.xclient.window = xwin;
  ev.xclient.message_type = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = 2;
  ev.xclient.data.l[1] = CurrentTime;
  ev.xclient.data.l[2] = 0;
  XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureRedirectMask | SubstructureNotifyMask,
             &ev);
  XFlush(dpy);
  return true;
}
#endif

}  // namespace

void bring_to_front(SDL_Window* window) {
#ifdef MAT_TRACK_HAVE_X11
  if (activate_x11(window)) return;
#endif
  if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) SDL_RestoreWindow(window);
  SDL_RaiseWindow(window);
}

}  // namespace mattrack
