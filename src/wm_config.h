/* wm_config.h
 */

#ifndef WM_CONFIG_H
#define WM_CONFIG_H

struct wm_config;

struct wm_config *wm_config_new();
void wm_config_del(struct wm_config *config);

/* Generialized interface.
 * All configuration items are accessible as string-keyed strings.
 * For device names, the key is "device.NAME" and value is the bdaddr in presentation form.
 *****************************************************************************/
 
int wm_config_set(struct wm_config *config,const char *k,int kc,const char *v,int vc);

int wm_config_decode(struct wm_config *config,const char *src,int srcc);

int wm_config_read_file(struct wm_config *config,const char *path);

/* Global items.
 * Strings are always returned NUL-terminated.
 * Do not modify or free a returned string, and do not retain it beyond any mutation of the config.
 * We also return string lengths for convenience.
 *****************************************************************************/

int wm_config_set_uinput_path(struct wm_config *config,const char *src,int srcc);
int wm_config_get_uinput_path(void *dstpp,const struct wm_config *config);

int wm_config_set_daemonize(struct wm_config *config,int daemonize);
int wm_config_get_daemonize(const struct wm_config *config);

int wm_config_set_retry_count(struct wm_config *config,int retry_count);
int wm_config_get_retry_count(const struct wm_config *config);

int wm_config_set_nunchuk_separate(struct wm_config *config,int nunchuk_separate);
int wm_config_get_nunchuk_separate(const struct wm_config *config);

int wm_config_set_classic_separate(struct wm_config *config,int classic_separate);
int wm_config_get_classic_separate(const struct wm_config *config);

int wm_config_set_verbosity(struct wm_config *config,int verbosity);
int wm_config_get_verbosity(const struct wm_config *config);

int wm_config_set_device_name(struct wm_config *config,const char *src,int srcc);
const char *wm_config_get_device_name(const struct wm_config *config);

// (v) is presentation form, we fail if it doesn't evaluate.
int wm_config_add_device_alias(struct wm_config *config,const char *k,int kc,const char *v,int vc);

/* Device aliases.
 * These functions all deal with unencoded bdaddr, ie exactly as they appear in a sockaddr.
 * wm_config_get_device_by_name() accepts aliases or bdaddr in presentation form.
 *****************************************************************************/

int wm_config_count_devices(const struct wm_config *config);
int wm_config_get_device_by_index(void *namepp,void *bdaddr,const struct wm_config *config,int p);
int wm_config_get_device_by_name(void *bdaddr,const struct wm_config *config,const char *name,int namec);
int wm_config_get_device_by_bdaddr(void *namepp,const struct wm_config *config,const void *bdaddr);

// Doesn't check for redundancy or anything. Typically you want wm_config_add_device_alias().
int wm_config_append_alias(struct wm_config *config,const char *k,int kc,const void *bdaddr);

#endif
