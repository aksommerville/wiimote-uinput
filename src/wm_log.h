/* wm_log.h
 * Unlike other services, this one is not wrapped in an object, it's completely global.
 */

#ifndef WM_LOG_H
#define WM_LOG_H

struct wm_config;

int wm_log_configure(const struct wm_config *config);

int wm_log_error(const char *fmt,...);
int wm_log_warning(const char *fmt,...);
int wm_log_info(const char *fmt,...);
int wm_log_debug(const char *fmt,...);
int wm_log_trace(const char *fmt,...);

int wm_log_error_enabled();
int wm_log_warning_enabled();
int wm_log_info_enabled();
int wm_log_debug_enabled();
int wm_log_trace_enabled();

#endif
