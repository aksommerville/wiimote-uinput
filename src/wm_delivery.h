/* wm_delivery.h
 * Manages the output to uinput.
 * There may be more than one delivery channel per instance, eg if a Classic Controller is connected.
 */

#ifndef WM_DELIVERY_H
#define WM_DELIVERY_H

struct wm_delivery;

struct wm_delivery *wm_delivery_new();
void wm_delivery_del(struct wm_delivery *delivery);

/* These properties can only be set while disconnected.
 */
int wm_delivery_set_uinput_path(struct wm_delivery *delivery,const char *src,int srcc);
const char *wm_delivery_get_uinput_path(const struct wm_delivery *delivery);
int wm_delivery_set_name(struct wm_delivery *delivery,const char *src,int srcc);
const char *wm_delivery_get_name(const struct wm_delivery *delivery);
int wm_delivery_set_device_type(struct wm_delivery *delivery,int device_type);
int wm_delivery_get_device_type(const struct wm_delivery *delivery);

int wm_delivery_set_nunchuk_separate(struct wm_delivery *delivery,int separate);
int wm_delivery_set_classic_separate(struct wm_delivery *delivery,int separate);

/* A connected delivery has an open connection to uinput and can be accessed via evdev.
 */
int wm_delivery_connect(struct wm_delivery *delivery);
int wm_delivery_disconnect(struct wm_delivery *delivery);
int wm_delivery_is_connected(const struct wm_delivery *delivery);

/* Call this for any changed report item.
 */
int wm_delivery_set_button(struct wm_delivery *delivery,int btnid,int value);

/* Call this at the end of each report.
 */
int wm_delivery_synchronize(struct wm_delivery *delivery);

#endif
