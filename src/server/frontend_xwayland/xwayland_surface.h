/*
 * Copyright (C) 2018 Marius Gripsgard <marius@ubports.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef MIR_FRONTEND_XWAYLAND_WM_SHELLSURFACE_H
#define MIR_FRONTEND_XWAYLAND_WM_SHELLSURFACE_H

#include "wl_surface.h"
#include "xwayland_wm.h"
#include "xwayland_surface_role_surface.h"
#include "xwayland_surface_observer_surface.h"

#include <mutex>
#include <chrono>

extern "C" {
#include <xcb/xcb.h>
}

struct wm_size_hints
{
    uint32_t flags;
    int32_t x, y;
    int32_t width, height; /* should set so old wm's don't mess up */
    int32_t min_width, min_height;
    int32_t max_width, max_height;
    int32_t width_inc, height_inc;
    struct
    {
        int32_t x;
        int32_t y;
    } min_aspect, max_aspect;
    int32_t base_width, base_height;
    int32_t win_gravity;
};

#define USPosition (1L << 0)
#define USSize (1L << 1)
#define PPosition (1L << 2)
#define PSize (1L << 3)
#define PMinSize (1L << 4)
#define PMaxSize (1L << 5)
#define PResizeInc (1L << 6)
#define PAspect (1L << 7)
#define PBaseSize (1L << 8)
#define PWinGravity (1L << 9)

struct motif_wm_hints
{
    uint32_t flags;
    uint32_t functions;
    uint32_t decorations;
    int32_t input_mode;
    uint32_t status;
};

#define MWM_HINTS_FUNCTIONS (1L << 0)
#define MWM_HINTS_DECORATIONS (1L << 1)
#define MWM_HINTS_INPUT_MODE (1L << 2)
#define MWM_HINTS_STATUS (1L << 3)

#define MWM_FUNC_ALL (1L << 0)
#define MWM_FUNC_RESIZE (1L << 1)
#define MWM_FUNC_MOVE (1L << 2)
#define MWM_FUNC_MINIMIZE (1L << 3)
#define MWM_FUNC_MAXIMIZE (1L << 4)
#define MWM_FUNC_CLOSE (1L << 5)

#define MWM_DECOR_ALL (1L << 0)
#define MWM_DECOR_BORDER (1L << 1)
#define MWM_DECOR_RESIZEH (1L << 2)
#define MWM_DECOR_TITLE (1L << 3)
#define MWM_DECOR_MENU (1L << 4)
#define MWM_DECOR_MINIMIZE (1L << 5)
#define MWM_DECOR_MAXIMIZE (1L << 6)

#define MWM_DECOR_EVERYTHING \
    (MWM_DECOR_BORDER | MWM_DECOR_RESIZEH | MWM_DECOR_TITLE | MWM_DECOR_MENU | MWM_DECOR_MINIMIZE | MWM_DECOR_MAXIMIZE)

#define MWM_INPUT_MODELESS 0
#define MWM_INPUT_PRIMARY_APPLICATION_MODAL 1
#define MWM_INPUT_SYSTEM_MODAL 2
#define MWM_INPUT_FULL_APPLICATION_MODAL 3
#define MWM_INPUT_APPLICATION_MODAL MWM_INPUT_PRIMARY_APPLICATION_MODAL

#define MWM_TEAROFF_WINDOW (1L << 0)

#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT 0
#define _NET_WM_MOVERESIZE_SIZE_TOP 1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT 2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT 3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT 4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM 5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT 6
#define _NET_WM_MOVERESIZE_SIZE_LEFT 7
#define _NET_WM_MOVERESIZE_MOVE 8           /* movement only */
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD 9  /* size via keyboard */
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD 10 /* move via keyboard */
#define _NET_WM_MOVERESIZE_CANCEL 11        /* cancel operation */

#define TYPE_WM_PROTOCOLS XCB_ATOM_CUT_BUFFER0
#define TYPE_MOTIF_WM_HINTS XCB_ATOM_CUT_BUFFER1
#define TYPE_NET_WM_STATE XCB_ATOM_CUT_BUFFER2
#define TYPE_WM_NORMAL_HINTS XCB_ATOM_CUT_BUFFER3

namespace mir
{
namespace shell
{
class Shell;
}
namespace frontend
{
class WlSeat;
class XWaylandWM;
class XWaylandSurfaceObserver;

class XWaylandSurface
    : public XWaylandSurfaceRoleSurface,
      public XWaylandSurfaceObserverSurface
{
public:
    XWaylandSurface(
        XWaylandWM *wm,
        WlSeat& seat,
        std::shared_ptr<shell::Shell> const& shell,
        xcb_create_notify_event_t *event);
    ~XWaylandSurface();

    void map();
    void close(); ///< Idempotent
    void net_wm_state_client_message(uint32_t const (&data)[5]);
    void wm_change_state_client_message(uint32_t const (&data)[5]);
    void dirty_properties();
    void read_properties();
    void set_surface(WlSurface* wl_surface); ///< Should only be called on the Wayland thread
    void set_workspace(int workspace);
    void unmap();
    void move_resize(uint32_t detail);

private:
    /// contains more information than just a MirWindowState
    /// (for example if a minimized window would otherwise be maximized)
    struct WindowState
    {
        bool minimized{false};
        bool maximized{false};
        bool fullscreen{false};
    };

    /// Overrides from XWaylandSurfaceObserverSurface
    /// @{
    void scene_surface_state_set(MirWindowState new_state) override;
    void scene_surface_resized(const geometry::Size& new_size) override;
    void scene_surface_close_requested() override;
    void run_on_wayland_thread(std::function<void()>&& work) override;
    /// @}

    /// Overrides from XWaylandSurfaceRoleSurface
    /// Should only be called on the Wayland thread
    /// @{
    void wl_surface_destroyed() override;
    void wl_surface_committed(WlSurface* wl_surface) override;
    auto scene_surface() const -> std::experimental::optional<std::shared_ptr<scene::Surface>> override;
    /// @}

    /// The last state we have either requested of Mir or been informed of by Mir
    /// Prevents requesting a window state that we are already in
    MirWindowState cached_mir_window_state{mir_window_state_unknown};

    /// Should only be called on the Wayland thread
    /// Should NOT be called under lock
    /// Does nothing if we already have a scene::Surface
    void create_scene_surface_if_needed(WlSurface* wl_surface);

    /// Sets the window's _NET_WM_STATE property based on the contents of window_state
    /// Also sets the state of the scene surface to match window_state
    /// Should be called after every change to window_state
    /// Should NOT be called under lock
    void set_window_state(WindowState const& new_window_state);

    auto latest_input_timestamp(std::lock_guard<std::mutex> const&) -> std::chrono::nanoseconds;

    XWaylandWM* const xwm;
    WlSeat& seat;
    std::shared_ptr<shell::Shell> const shell;
    xcb_window_t const window;

    std::mutex mutable mutex;

    /// Reflects the _NET_WM_STATE and WM_STATE we have currently set on the window
    /// Should only be modified by set_wm_state()
    WindowState window_state;

    bool props_dirty;
    struct
    {
        std::string title;
        std::string appId;
        int deleteWindow;
    } properties;

    struct
    {
        xcb_window_t parent;
        geometry::Point position;
        geometry::Size size;
        bool override_redirect;
    } const init;

    std::experimental::optional<std::shared_ptr<XWaylandSurfaceObserver>> surface_observer;

    /// Only true when we are in the process of creating a scene surface
    bool creating_scene_surface{false};
    std::weak_ptr<scene::Surface> weak_scene_surface;
};
} /* frontend */
} /* mir */

#endif /* end of include guard: MIR_FRONTEND_XWAYLAND_WM_SHELLSURFACE_H */
