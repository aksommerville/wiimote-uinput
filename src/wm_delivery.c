#include "wiimote.h"
#include "wm_delivery.h"
#include "wm_enums.h"
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>

#define WM_DELIVERY_NAME_LIMIT 63

/* Object definition.
 */

struct wm_delivery {
  int fd;
  char *uinput_path;
  char *name;
  int namec;
  int device_type;
  int sync_required;

  // Only relevant to WM_DEVICE_TYPE_WIIMOTE, will these extensions be reported combined?
  int may_have_nunchuk;
  int may_have_classic;
};

/* Object lifecycle.
 */
 
struct wm_delivery *wm_delivery_new() {
  struct wm_delivery *delivery=calloc(1,sizeof(struct wm_delivery));
  if (!delivery) return 0;

  delivery->fd=-1;

  return delivery;
}

void wm_delivery_del(struct wm_delivery *delivery) {
  if (!delivery) return;

  if (delivery->fd>=0) close(delivery->fd);
  if (delivery->uinput_path) free(delivery->uinput_path);
  if (delivery->name) free(delivery->name);

  free(delivery);
}

/* Accessors.
 */
 
int wm_delivery_set_uinput_path(struct wm_delivery *delivery,const char *src,int srcc) {
  if (!delivery) return -1;
  if (delivery->fd>=0) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (delivery->uinput_path) free(delivery->uinput_path);
  delivery->uinput_path=nv;
  return 0;
}

const char *wm_delivery_get_uinput_path(const struct wm_delivery *delivery) {
  if (!delivery) return 0;
  return delivery->uinput_path;
}

int wm_delivery_set_name(struct wm_delivery *delivery,const char *src,int srcc) {
  if (!delivery) return -1;
  if (delivery->fd>=0) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc>WM_DELIVERY_NAME_LIMIT) srcc=WM_DELIVERY_NAME_LIMIT;
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
    int i; for (i=0;i<srcc;i++) {
      if ((nv[i]<0x20)||(nv[i]>0x7e)) nv[i]='?';
    }
  }
  if (delivery->name) free(delivery->name);
  delivery->name=nv;
  delivery->namec=srcc;
  return 0;
}

const char *wm_delivery_get_name(const struct wm_delivery *delivery) {
  if (!delivery) return 0;
  return delivery->name;
}

int wm_delivery_set_device_type(struct wm_delivery *delivery,int device_type) {
  if (!delivery) return -1;
  delivery->device_type=device_type;
  return 0;
}

int wm_delivery_get_device_type(const struct wm_delivery *delivery) {
  if (!delivery) return 0;
  return delivery->device_type;
}

int wm_delivery_is_connected(const struct wm_delivery *delivery) {
  if (!delivery) return 0;
  return (delivery->fd>=0)?1:0;
}

int wm_delivery_set_nunchuk_separate(struct wm_delivery *delivery,int separate) {
  if (!delivery) return -1;
  delivery->may_have_nunchuk=!separate;
  return 0;
}

int wm_delivery_set_classic_separate(struct wm_delivery *delivery,int separate) {
  if (!delivery) return -1;
  delivery->may_have_classic=!separate;
  return 0;
}

/* Populate limits for absolute axes.
 */

static void wm_delivery_populate_uud_abs_limits(struct uinput_user_dev *uud,struct wm_delivery *delivery) {
  #define LIMITS(tag,lo,hi) { uud->absmin[ABS_##tag]=lo; uud->absmax[ABS_##tag]=hi; }
  switch (delivery->device_type) {
    case WM_DEVICE_TYPE_WIIMOTE: {
        LIMITS(X,-1,1) // core d-pad
        LIMITS(Y,-1,1)
        LIMITS(RX,-512,511) // core accelerometer
        LIMITS(RY,-512,511)
        LIMITS(RZ,-512,511)
        LIMITS(MISC+0,-128,127) // nunchuk stick
        LIMITS(MISC+1,-128,127)
        LIMITS(MISC+2,-512,511) // nunchuk accelerometer
        LIMITS(MISC+3,-512,511)
        LIMITS(MISC+4,-512,511)
        LIMITS(MISC+5,-32,31) // classic sticks
        LIMITS(MISC+6,-32,31)
        LIMITS(MISC+7,-16,15)
        LIMITS(MISC+8,-16,15)
        LIMITS(MISC+9,0,31) // classic triggers
        LIMITS(MISC+10,0,31)
      } break;
    case WM_DEVICE_TYPE_NUNCHUK: {
        LIMITS(X,-128,127)
        LIMITS(Y,-128,127)
        LIMITS(RX,-512,511)
        LIMITS(RY,-512,511)
        LIMITS(RZ,-512,511)
      } break;
    case WM_DEVICE_TYPE_CLASSIC: {
        LIMITS(X,-32,31)
        LIMITS(Y,-32,31)
        LIMITS(RX,-16,15)
        LIMITS(RY,-16,15)
        LIMITS(Z,0,31)
        LIMITS(RZ,0,31)
      } break;
  }
  #undef LIMITS
}

/* Send ioctls to describe all possible events.
 */

static int wm_delivery_set_event_bits(struct wm_delivery *delivery) {

  if (ioctl(delivery->fd,UI_SET_EVBIT,EV_KEY)<0) return -1;
  if (ioctl(delivery->fd,UI_SET_EVBIT,EV_ABS)<0) return -1;

  #define SETABS(tag) if (ioctl(delivery->fd,UI_SET_ABSBIT,ABS_##tag)<0) return -1;
  #define SETBTN(tag) if (ioctl(delivery->fd,UI_SET_KEYBIT,BTN_##tag)<0) return -1;
  #define SETKEY(tag) if (ioctl(delivery->fd,UI_SET_KEYBIT,KEY_##tag)<0) return -1;

  switch (delivery->device_type) {

    case WM_DEVICE_TYPE_WIIMOTE: {

        SETABS(X)
        SETABS(Y)
        SETBTN(0) //TODO button names
        SETBTN(1)
        SETBTN(2)
        SETBTN(3)
        SETBTN(4)
        SETBTN(5)
        SETBTN(6)

        SETABS(RX)
        SETABS(RY)
        SETABS(RZ)

        if (delivery->may_have_nunchuk) {
          SETABS(MISC+0)
          SETABS(MISC+1)
          SETABS(MISC+2)
          SETABS(MISC+3)
          SETABS(MISC+4)
          SETBTN(7)
          SETBTN(8)
        }

        if (delivery->may_have_classic) {
          SETABS(MISC+5)
          SETABS(MISC+6)
          SETABS(MISC+7)
          SETABS(MISC+8)
          SETABS(MISC+9)
          SETABS(MISC+10)
          SETKEY(UP)
          SETKEY(DOWN)
          SETKEY(LEFT)
          SETKEY(RIGHT)
          SETKEY(A)
          SETKEY(B)
          SETKEY(X)
          SETKEY(Y)
          SETKEY(L)
          SETKEY(R)
          SETKEY(Z)
          SETKEY(C)
          SETKEY(EQUAL)
          SETKEY(MINUS)
          SETKEY(ESC)
        }

      } break;

    case WM_DEVICE_TYPE_NUNCHUK: {
        SETABS(X)
        SETABS(Y)
        SETABS(RX)
        SETABS(RY)
        SETABS(RZ)
        SETBTN(0)
        SETBTN(1)
      } break;

    case WM_DEVICE_TYPE_CLASSIC: {
        SETABS(X)
        SETABS(Y)
        SETABS(Z)
        SETABS(RX)
        SETABS(RY)
        SETABS(RZ)
        SETBTN(0)
        SETBTN(1)
        SETBTN(2)
        SETBTN(3)
        SETBTN(EAST)
        SETBTN(WEST)
        SETBTN(SOUTH)
        SETBTN(NORTH)
        SETBTN(TL)
        SETBTN(TR)
        SETBTN(TL2)
        SETBTN(TR2)
        SETBTN(START)
        SETBTN(SELECT)
        SETBTN(MODE)
      } break;

  }
  #undef SETABS
  #undef SETBTN
  #undef SETKEY
  return 0;
}

/* Compose device name.
 */

static int wm_delivery_compose_base_name(char *dst,int dsta,const struct wm_delivery *delivery) {

  if (delivery->namec>0) {
    if (delivery->namec<=dsta) {
      memcpy(dst,delivery->name,delivery->namec);
      if (delivery->namec<dsta) dst[delivery->namec]=0;
    }
    return delivery->namec;
  }

  const char *src="Wii Remote";
  int srcc=0; while (src[srcc]) srcc++;
  if (srcc<=dsta) {
    memcpy(dst,src,srcc);
    if (srcc<dsta) dst[srcc]=0;
  }
  return srcc;
}

static int wm_delivery_compose_name(char *dst,int dsta,const struct wm_delivery *delivery) {

  int dstc=wm_delivery_compose_base_name(dst,dsta,delivery);
  if (dstc<0) return -1;

  if (delivery->device_type!=WM_DEVICE_TYPE_WIIMOTE) {
    if (dstc<dsta) dst[dstc]=':';
    dstc++;
    const char *extname=wm_device_type_repr(delivery->device_type);
    if (!extname||!extname[0]) extname="EXTENSION";
    int extnamec=0; while (extname[extnamec]) extnamec++;
    if (dstc<=dsta-extnamec) memcpy(dst+dstc,extname,extnamec);
    dstc+=extnamec;
  }

  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Connect.
 */

int wm_delivery_connect(struct wm_delivery *delivery) {
  if (!delivery) return -1;
  if (delivery->fd>=0) return 0;
  wm_log_trace("%s",__func__);

  if (!delivery->uinput_path) return -1;
  if ((delivery->fd=open(delivery->uinput_path,O_RDWR))<0) {
    wm_log_error("%s: Failed to open: %m",delivery->uinput_path);
    return -1;
  }

  struct uinput_user_dev uud={0};
  if (wm_delivery_compose_name(uud.name,sizeof(uud.name),delivery)<0) return -1;

  wm_delivery_populate_uud_abs_limits(&uud,delivery);

  if (write(delivery->fd,&uud,sizeof(uud))!=sizeof(uud)) {
    wm_log_error("%s: Failed to write uinput_user_dev: %m",delivery->uinput_path);
    wm_delivery_disconnect(delivery);
    return -1;
  }

  if (wm_delivery_set_event_bits(delivery)<0) {
    wm_log_error("%s: Failed to set device description: %m",delivery->uinput_path);
    wm_delivery_disconnect(delivery);
    return -1;
  }

  if (ioctl(delivery->fd,UI_DEV_CREATE)<0) {
    wm_log_error("%s: UI_DEV_CREATE failed: %m",delivery->uinput_path);
    wm_delivery_disconnect(delivery);
    return -1;
  }

  return 0;
}

/* Disconnect.
 */
 
int wm_delivery_disconnect(struct wm_delivery *delivery) {
  if (!delivery) return -1;
  if (delivery->fd<0) return 0;
  close(delivery->fd);
  delivery->fd=-1;
  return 0;
}

/* Translate input event.
 * Return 0 to discard event, >0 to proceed, or <0 for real errors.
 */

static int wm_delivery_translate_event(struct input_event *evt,const struct wm_delivery *delivery,int btnid,int value) {
  evt->value=value;
  evt->type=EV_KEY;

  /* Mangle extension buttons to fit all in one device. */
  if (delivery->device_type==WM_DEVICE_TYPE_WIIMOTE) switch (btnid) {

    case WM_BTNID_CORE_UP:    evt->type=EV_ABS; evt->code=ABS_Y; evt->value=value?-1:0; return 1;
    case WM_BTNID_CORE_DOWN:  evt->type=EV_ABS; evt->code=ABS_Y; evt->value=value? 1:0; return 1;
    case WM_BTNID_CORE_LEFT:  evt->type=EV_ABS; evt->code=ABS_X; evt->value=value?-1:0; return 1;
    case WM_BTNID_CORE_RIGHT: evt->type=EV_ABS; evt->code=ABS_X; evt->value=value? 1:0; return 1;
    case WM_BTNID_CORE_A:     evt->code=BTN_0; return 1; //TODO preferred button names (<linux/input.h>)
    case WM_BTNID_CORE_B:     evt->code=BTN_1; return 1;
    case WM_BTNID_CORE_1:     evt->code=BTN_2; return 1;
    case WM_BTNID_CORE_2:     evt->code=BTN_3; return 1;
    case WM_BTNID_CORE_MINUS: evt->code=BTN_4; return 1;
    case WM_BTNID_CORE_PLUS:  evt->code=BTN_5; return 1;
    case WM_BTNID_CORE_HOME:  evt->code=BTN_6; return 1;

    case WM_BTNID_CORE_ACCELX: evt->type=EV_ABS; evt->code=ABS_RX; evt->value=value; return 1;
    case WM_BTNID_CORE_ACCELY: evt->type=EV_ABS; evt->code=ABS_RY; evt->value=value; return 1;
    case WM_BTNID_CORE_ACCELZ: evt->type=EV_ABS; evt->code=ABS_RZ; evt->value=value; return 1;

    //TODO Do we want to report status fields? eg extension connected

    case WM_BTNID_NUNCHUK_X:       evt->type=EV_ABS; evt->code=ABS_MISC+0; evt->value=value; return 1;
    case WM_BTNID_NUNCHUK_Y:       evt->type=EV_ABS; evt->code=ABS_MISC+1; evt->value=value; return 1;
    case WM_BTNID_NUNCHUK_ACCELX:  evt->type=EV_ABS; evt->code=ABS_MISC+2; evt->value=value; return 1;
    case WM_BTNID_NUNCHUK_ACCELY:  evt->type=EV_ABS; evt->code=ABS_MISC+3; evt->value=value; return 1;
    case WM_BTNID_NUNCHUK_ACCELZ:  evt->type=EV_ABS; evt->code=ABS_MISC+4; evt->value=value; return 1;
    case WM_BTNID_NUNCHUK_Z:       evt->code=BTN_7; return 1;
    case WM_BTNID_NUNCHUK_C:       evt->code=BTN_8; return 1;

    case WM_BTNID_CLASSIC_LX:    evt->type=EV_ABS; evt->code=ABS_MISC+5; evt->value=value; return 1;
    case WM_BTNID_CLASSIC_LY:    evt->type=EV_ABS; evt->code=ABS_MISC+6; evt->value=value; return 1;
    case WM_BTNID_CLASSIC_RX:    evt->type=EV_ABS; evt->code=ABS_MISC+7; evt->value=value; return 1;
    case WM_BTNID_CLASSIC_RY:    evt->type=EV_ABS; evt->code=ABS_MISC+8; evt->value=value; return 1;
    case WM_BTNID_CLASSIC_LA:    evt->type=EV_ABS; evt->code=ABS_MISC+9; evt->value=value; return 1;
    case WM_BTNID_CLASSIC_RA:    evt->type=EV_ABS; evt->code=ABS_MISC+10; evt->value=value; return 1;
    case WM_BTNID_CLASSIC_UP:    evt->code=KEY_UP; return 1;
    case WM_BTNID_CLASSIC_DOWN:  evt->code=KEY_DOWN; return 1;
    case WM_BTNID_CLASSIC_LEFT:  evt->code=KEY_LEFT; return 1;
    case WM_BTNID_CLASSIC_RIGHT: evt->code=KEY_RIGHT; return 1;
    case WM_BTNID_CLASSIC_A:     evt->code=KEY_A; return 1;
    case WM_BTNID_CLASSIC_B:     evt->code=KEY_B; return 1;
    case WM_BTNID_CLASSIC_X:     evt->code=KEY_X; return 1;
    case WM_BTNID_CLASSIC_Y:     evt->code=KEY_Y; return 1;
    case WM_BTNID_CLASSIC_L:     evt->code=KEY_L; return 1;
    case WM_BTNID_CLASSIC_R:     evt->code=KEY_R; return 1;
    case WM_BTNID_CLASSIC_ZL:    evt->code=KEY_Z; return 1;
    case WM_BTNID_CLASSIC_ZR:    evt->code=KEY_C; return 1;
    case WM_BTNID_CLASSIC_PLUS:  evt->code=KEY_EQUAL; return 1;
    case WM_BTNID_CLASSIC_MINUS: evt->code=KEY_MINUS; return 1;
    case WM_BTNID_CLASSIC_HOME:  evt->code=KEY_ESC; return 1;

  /* Treat extension buttons as primaries. */
  } else switch (btnid) {

    case WM_BTNID_NUNCHUK_X:       evt->type=EV_ABS; evt->code=ABS_X; evt->value=value; return 1;
    case WM_BTNID_NUNCHUK_Y:       evt->type=EV_ABS; evt->code=ABS_Y; evt->value=value; return 1;
    case WM_BTNID_NUNCHUK_ACCELX:  evt->type=EV_ABS; evt->code=ABS_RX; evt->value=value; return 1;
    case WM_BTNID_NUNCHUK_ACCELY:  evt->type=EV_ABS; evt->code=ABS_RY; evt->value=value; return 1;
    case WM_BTNID_NUNCHUK_ACCELZ:  evt->type=EV_ABS; evt->code=ABS_RZ; evt->value=value; return 1;
    case WM_BTNID_NUNCHUK_Z:       evt->code=BTN_0; return 1;
    case WM_BTNID_NUNCHUK_C:       evt->code=BTN_1; return 1;

    case WM_BTNID_CLASSIC_LX:    evt->type=EV_ABS; evt->code=ABS_X; evt->value=value; return 1;
    case WM_BTNID_CLASSIC_LY:    evt->type=EV_ABS; evt->code=ABS_Y; evt->value=value; return 1;
    case WM_BTNID_CLASSIC_RX:    evt->type=EV_ABS; evt->code=ABS_RX; evt->value=value; return 1;
    case WM_BTNID_CLASSIC_RY:    evt->type=EV_ABS; evt->code=ABS_RY; evt->value=value; return 1;
    case WM_BTNID_CLASSIC_LA:    evt->type=EV_ABS; evt->code=ABS_Z; evt->value=value; return 1;
    case WM_BTNID_CLASSIC_RA:    evt->type=EV_ABS; evt->code=ABS_RZ; evt->value=value; return 1;
    case WM_BTNID_CLASSIC_UP:    evt->code=BTN_0; return 1;
    case WM_BTNID_CLASSIC_DOWN:  evt->code=BTN_1; return 1;
    case WM_BTNID_CLASSIC_LEFT:  evt->code=BTN_2; return 1;
    case WM_BTNID_CLASSIC_RIGHT: evt->code=BTN_3; return 1;
    case WM_BTNID_CLASSIC_A:     evt->code=BTN_EAST; return 1;
    case WM_BTNID_CLASSIC_B:     evt->code=BTN_SOUTH; return 1;
    case WM_BTNID_CLASSIC_X:     evt->code=BTN_NORTH; return 1;
    case WM_BTNID_CLASSIC_Y:     evt->code=BTN_WEST; return 1;
    case WM_BTNID_CLASSIC_L:     evt->code=BTN_TL; return 1;
    case WM_BTNID_CLASSIC_R:     evt->code=BTN_TR; return 1;
    case WM_BTNID_CLASSIC_ZL:    evt->code=BTN_TL2; return 1;
    case WM_BTNID_CLASSIC_ZR:    evt->code=BTN_TR2; return 1;
    case WM_BTNID_CLASSIC_PLUS:  evt->code=BTN_START; return 1;
    case WM_BTNID_CLASSIC_MINUS: evt->code=BTN_SELECT; return 1;
    case WM_BTNID_CLASSIC_HOME:  evt->code=BTN_MODE; return 1;
    
  }
  return 0;
}

/* Send event to uinput device.
 */

int wm_delivery_set_button(struct wm_delivery *delivery,int btnid,int value) {
  if (!delivery) return -1;
  if (delivery->fd<0) return -1;

  struct input_event evt={0};
  int err=wm_delivery_translate_event(&evt,delivery,btnid,value);
  if (err<=0) return err;

  if (write(delivery->fd,&evt,sizeof(evt))!=sizeof(evt)) {
    wm_log_error("Failed to write event to uinput: %m");
    return -1;
  }

  delivery->sync_required=1;

  return 0;
}

int wm_delivery_synchronize(struct wm_delivery *delivery) {
  if (!delivery) return -1;
  if (delivery->fd<0) return 0;
  if (!delivery->sync_required) return 0;

  struct input_event evt={.type=EV_SYN,.code=SYN_REPORT};
  if (write(delivery->fd,&evt,sizeof(evt))!=sizeof(evt)) {
    wm_log_error("Failed to send report synchronization to uinput: %m");
    return -1;
  }

  return 0;
}
