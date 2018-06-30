#include "wiimote.h"
#include "wm_log.h"
#include "wm_config.h"

/* Globals.
 */

static int wm_log_verbosity=3;

/* Reconfigure.
 */
 
int wm_log_configure(const struct wm_config *config) {
  if (!config) return -1;
  wm_log_verbosity=wm_config_get_verbosity(config);
  if (wm_log_verbosity<0) wm_log_verbosity=0;
  else if (wm_log_verbosity>5) wm_log_verbosity=5;
  return 0;
}

/* Log, main.
 */

static int wm_log(const char *levelname,const char *fmt,va_list vargs) {
  char msg[256];
  int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
  if (msgc>=sizeof(msg)) {
    fprintf(stderr,"wiimote:%s: (message too long)\n",levelname);
  } else {
    fprintf(stderr,"wiimote:%s: %.*s\n",levelname,msgc,msg);
  }
  return 0;
}

/* Public entry points for simple logging.
 */

int wm_log_error(const char *fmt,...) {
  if (wm_log_verbosity<1) return 0;
  va_list vargs;
  va_start(vargs,fmt);
  return wm_log("ERROR",fmt?fmt:"",vargs);
}

int wm_log_warning(const char *fmt,...) {
  if (wm_log_verbosity<2) return 0;
  va_list vargs;
  va_start(vargs,fmt);
  return wm_log("WARNING",fmt?fmt:"",vargs);
}

int wm_log_info(const char *fmt,...) {
  if (wm_log_verbosity<3) return 0;
  va_list vargs;
  va_start(vargs,fmt);
  return wm_log("INFO",fmt?fmt:"",vargs);
}

int wm_log_debug(const char *fmt,...) {
  if (wm_log_verbosity<4) return 0;
  va_list vargs;
  va_start(vargs,fmt);
  return wm_log("DEBUG",fmt?fmt:"",vargs);
}

int wm_log_trace(const char *fmt,...) {
  if (wm_log_verbosity<5) return 0;
  va_list vargs;
  va_start(vargs,fmt);
  return wm_log("TRACE",fmt?fmt:"",vargs);
}

/* Test log level enablement.
 */
 
int wm_log_error_enabled() { return (wm_log_verbosity>=1)?1:0; }
int wm_log_warning_enabled() { return (wm_log_verbosity>=2)?1:0; }
int wm_log_info_enabled() { return (wm_log_verbosity>=3)?1:0; }
int wm_log_debug_enabled() { return (wm_log_verbosity>=4)?1:0; }
int wm_log_trace_enabled() { return (wm_log_verbosity>=5)?1:0; }
