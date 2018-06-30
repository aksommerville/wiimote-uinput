/* wm_report.h
 * Logical layer, turning Wiimote reports into discrete events.
 */

#ifndef WM_REPORT_H
#define WM_REPORT_H

struct wm_report;
struct wm_config;

struct wm_report_delegate {
  int (*cb_button)(void *userdata,int btnid,int value);
  int (*cb_ack)(void *userdata,uint8_t rptid,uint8_t result);
  int (*cb_read)(void *userdata,uint16_t addr,int err,const void *src,int srcc);
  void *userdata;
};

struct wm_report *wm_report_new(const struct wm_report_delegate *delegate);
void wm_report_del(struct wm_report *report);

/* This might end up being very configurable, so we don't bother enumerating all the various configuration points.
 * Just expose the whole configuration once before you start using the report.
 */
int wm_report_configure(struct wm_report *report,const struct wm_config *config);

/* Deliver a new raw report, straight off the transport.
 * Callback will fire and state will update as warranted.
 */
int wm_report_deliver(struct wm_report *report,const void *src,int srcc);

/* Get the most recent state of any button.
 * Anything we don't recognize, or isn't connected, we return 0.
 */
int wm_report_get_button(const struct wm_report *report,int btnid);

/* Force the value of a button.
 * This *does* trigger the callback, if it's valid and changed.
 * Values are quietly clamped to the legal range for the given button.
 * Returns:
 *   >0 if the value changed
 *    0 if value is redundant after clamping
 *   <0 if (report) is inconsistent or the callback fails
 */
int wm_report_set_button(struct wm_report *report,int btnid,int value);

int wm_report_set_rptid(struct wm_report *report,uint8_t rptid);
int wm_report_set_rumble(struct wm_report *report,int rumble);
int wm_report_set_continuous(struct wm_report *report,int continuous);

/* The report object doesn't know what extension is connected to it.
 * TODO Maybe we should adjust the responsibilities of coord and report in this regard.
 * You must tell me how to interpret extension reports.
 */
int wm_report_set_extension(struct wm_report *report,int extid);

/* Compose output reports.
 * No output report is longer than 23 bytes. We fail if you provide a short buffer.
 */

int wm_report_compose_led(void *dst,int dsta,struct wm_report *report,int led1,int led2,int led3,int led4);
int wm_report_compose_rptid(void *dst,int dsta,struct wm_report *report);
int wm_report_compose_write(void *dst,int dsta,struct wm_report *report,int addr,const void *src,int srcc);
int wm_report_compose_read(void *dst,int dsta,struct wm_report *report,int addr,int size);

#endif
