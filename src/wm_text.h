/* wm_text.h
 * Simple text helpers.
 */

#ifndef WM_TEXT_H
#define WM_TEXT_H

int wm_bdaddr_repr(char *dst,int dsta,const void *src);
int wm_bdaddr_eval(void *dst,const char *src,int srcc);

int wm_decsint_repr(char *dst,int dsta,int src);
int wm_int_eval(int *dst,const char *src,int srcc);

/* Compose a neat hexadecimal representation of arbitrary data.
 * Mostly for logging reports.
 */
int wm_report_repr(char *dst,int dsta,const void *src,int srcc);

#endif
