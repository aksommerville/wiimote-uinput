/* wm_coord.h
 * Coordinator.
 * This object is aware of the other major objects (transport, report, delivery).
 * It is responsible for coordination among them, and high-level operational logic.
 */

#ifndef WM_COORD_H
#define WM_COORD_H

struct wm_coord;
struct wm_config;

struct wm_coord *wm_coord_new();
void wm_coord_del(struct wm_coord *coord);

int wm_coord_startup(struct wm_coord *coord,struct wm_config *config);
int wm_coord_update(struct wm_coord *coord);
int wm_coord_shutdown(struct wm_coord *coord);
int wm_coord_is_running(const struct wm_coord *coord);

#endif
