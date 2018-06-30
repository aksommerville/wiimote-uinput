#include "wiimote.h"
#include "wm_enums.h"

/* Button ID.
 */

const char *wm_btnid_repr(int btnid) {
  switch (btnid) {
    #define _(tag) case WM_BTNID_##tag: return #tag;

    _(CORE_UP)
    _(CORE_DOWN)
    _(CORE_LEFT)
    _(CORE_RIGHT)
    _(CORE_A)
    _(CORE_B)
    _(CORE_1)
    _(CORE_2)
    _(CORE_PLUS)
    _(CORE_MINUS)
    _(CORE_HOME)

    _(CORE_ACCELX)
    _(CORE_ACCELY)
    _(CORE_ACCELZ)

    _(CORE_BATTLOW)
    _(CORE_BATTERY)
    _(CORE_EXTPRESENT)
    _(CORE_EXTID)

    _(NUNCHUK_X)
    _(NUNCHUK_Y)
    _(NUNCHUK_ACCELX)
    _(NUNCHUK_ACCELY)
    _(NUNCHUK_ACCELZ)
    _(NUNCHUK_Z)
    _(NUNCHUK_C)

    _(CLASSIC_LX)
    _(CLASSIC_LY)
    _(CLASSIC_RX)
    _(CLASSIC_RY)
    _(CLASSIC_LA)
    _(CLASSIC_RA)
    _(CLASSIC_UP)
    _(CLASSIC_DOWN)
    _(CLASSIC_LEFT)
    _(CLASSIC_RIGHT)
    _(CLASSIC_A)
    _(CLASSIC_B)
    _(CLASSIC_X)
    _(CLASSIC_Y)
    _(CLASSIC_L)
    _(CLASSIC_R)
    _(CLASSIC_ZL)
    _(CLASSIC_ZR)
    _(CLASSIC_PLUS)
    _(CLASSIC_MINUS)
    _(CLASSIC_HOME)

    #undef _
  }
  return 0;
}

/* Device type.
 */
 
const char *wm_device_type_repr(int type) {
  switch (type) {
    #define _(tag) case WM_DEVICE_TYPE_##tag: return #tag;
    _(WIIMOTE)
    _(NUNCHUK)
    _(CLASSIC)
    #undef _
  }
  return 0;
}
