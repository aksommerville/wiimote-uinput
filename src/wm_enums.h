/* wm_enums.h
 * Symbols for devices and buttons.
 */

#ifndef WM_ENUMS_H
#define WM_ENUMS_H

#define WM_DEVICE_TYPE_WIIMOTE     1
#define WM_DEVICE_TYPE_NUNCHUK     2
#define WM_DEVICE_TYPE_CLASSIC     3

#define WM_BTNID_CORE_UP           1
#define WM_BTNID_CORE_DOWN         2
#define WM_BTNID_CORE_LEFT         3
#define WM_BTNID_CORE_RIGHT        4
#define WM_BTNID_CORE_A            5
#define WM_BTNID_CORE_B            6
#define WM_BTNID_CORE_1            7
#define WM_BTNID_CORE_2            8
#define WM_BTNID_CORE_PLUS         9
#define WM_BTNID_CORE_MINUS       10
#define WM_BTNID_CORE_HOME        11

#define WM_BTNID_CORE_ACCELX      12
#define WM_BTNID_CORE_ACCELY      13
#define WM_BTNID_CORE_ACCELZ      14

#define WM_BTNID_CORE_BATTLOW     15
#define WM_BTNID_CORE_BATTERY     16
#define WM_BTNID_CORE_EXTPRESENT  17
#define WM_BTNID_CORE_EXTID       18

#define WM_BTNID_NUNCHUK_X        19
#define WM_BTNID_NUNCHUK_Y        20
#define WM_BTNID_NUNCHUK_ACCELX   21
#define WM_BTNID_NUNCHUK_ACCELY   22
#define WM_BTNID_NUNCHUK_ACCELZ   23
#define WM_BTNID_NUNCHUK_Z        24
#define WM_BTNID_NUNCHUK_C        25

#define WM_BTNID_CLASSIC_LX       26
#define WM_BTNID_CLASSIC_LY       27
#define WM_BTNID_CLASSIC_RX       28
#define WM_BTNID_CLASSIC_RY       29
#define WM_BTNID_CLASSIC_LA       30
#define WM_BTNID_CLASSIC_RA       31
#define WM_BTNID_CLASSIC_UP       32
#define WM_BTNID_CLASSIC_DOWN     33
#define WM_BTNID_CLASSIC_LEFT     34
#define WM_BTNID_CLASSIC_RIGHT    35
#define WM_BTNID_CLASSIC_A        36
#define WM_BTNID_CLASSIC_B        37
#define WM_BTNID_CLASSIC_X        38
#define WM_BTNID_CLASSIC_Y        39
#define WM_BTNID_CLASSIC_L        40
#define WM_BTNID_CLASSIC_R        41
#define WM_BTNID_CLASSIC_ZL       42
#define WM_BTNID_CLASSIC_ZR       43
#define WM_BTNID_CLASSIC_PLUS     44
#define WM_BTNID_CLASSIC_MINUS    45
#define WM_BTNID_CLASSIC_HOME     46

const char *wm_btnid_repr(int btnid);
const char *wm_device_type_repr(int type);

static inline int wm_btnid_is_extension(int btnid) {
  return (btnid>=WM_BTNID_NUNCHUK_X)?1:0;
}

#endif
