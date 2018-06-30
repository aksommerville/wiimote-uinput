#include "wiimote.h"
#include "wm_report.h"
#include "wm_text.h"
#include "wm_enums.h"

/* Object definition.
 */

struct wm_report_nunchuk {
  int sx,sy; // stick
  int ax,ay,az; // accelerometer
  uint8_t buttons; // 01=Z, 02=C
};

struct wm_report_classic {
  int lx,ly,rx,ry; // sticks
  int la,ra; // analogue triggers
  uint16_t buttons;
};

struct wm_report {

  struct wm_report_delegate delegate;

  /* Operational state. */
  uint8_t rptid;
  uint8_t rumble;
  uint8_t continuous;
  uint8_t rpt3e[23];
  uint8_t pvrpt[23];
  int pvrptc;

  /* Core features. */
  uint16_t buttons;
  int accelx,accely,accelz;
  uint8_t status;
  uint8_t battery;

  /* Extension. */
  int extid;
  struct wm_report_nunchuk nunchuk;
  struct wm_report_classic classic;
  
};

/* Object lifecycle.
 */
 
struct wm_report *wm_report_new(const struct wm_report_delegate *delegate) {
  if (!delegate) return 0;
  if (!delegate->cb_button) return 0;
  if (!delegate->cb_ack) return 0;
  if (!delegate->cb_read) return 0;
  
  struct wm_report *report=calloc(1,sizeof(struct wm_report));
  if (!report) return 0;

  memcpy(&report->delegate,delegate,sizeof(struct wm_report_delegate));
  report->rptid=0x30;

  return report;
}
  
void wm_report_del(struct wm_report *report) {
  if (!report) return;

  free(report);
}

/* Configure.
 */
 
int wm_report_configure(struct wm_report *report,const struct wm_config *config) {
  if (!report||!config) return -1;
  //TODO
  wm_log_trace("%s",__func__);
  return 0;
}

/* Compare current and previous value, and fire callback if changed.
 */

static inline int wm_report_check_bit(const struct wm_report *report,int prev,int next,int mask,int btnid) {
       if ( (prev&mask)&&!(next&mask)) { if (report->delegate.cb_button(report->delegate.userdata,btnid,0)<0) return -1; }
  else if (!(prev&mask)&& (next&mask)) { if (report->delegate.cb_button(report->delegate.userdata,btnid,1)<0) return -1; }
  return 0;
}

static inline int wm_report_check_int(const struct wm_report *report,int prev,int next,int btnid) {
  if (prev!=next) { if (report->delegate.cb_button(report->delegate.userdata,btnid,next)<0) return -1; }
  return 0;
}

/* Receive status report.
 *   0000  1 flags:
 *             01 battery low
 *             02 extension present
 *             04 speaker enabled
 *             08 ir enabled
 *             10 led 1
 *             20 led 2
 *             40 led 3
 *             80 led 4
 *   0001  2 unused
 *   0003  1 battery level
 *   0004
 */

static int wm_report_deliver_status(struct wm_report *report,const uint8_t *src) {

  if (src[0]!=report->status) {
    if (wm_report_check_bit(report,report->status,src[0],0x01,WM_BTNID_CORE_BATTLOW)<0) return -1;
    if (wm_report_check_bit(report,report->status,src[0],0x02,WM_BTNID_CORE_EXTPRESENT)<0) return -1;
    report->status=src[0];
  }

  if (wm_report_check_int(report,report->battery,src[1],WM_BTNID_CORE_BATTERY)<0) return -1;
  report->battery=src[1];

  return 0;
}

/* Receive read-memory result.
 *   0000   1 (f0)=(size-1), (0f)=error
 *   0001   2 addr
 *   0003  16 data
 */

static int wm_report_deliver_rdmem(struct wm_report *report,const uint8_t *src,int srcc) {
  int len=(src[0]>>4)+1;
  int err=(src[0]&0x0f);
  int addr=(src[1]<<8)|src[2];//TODO byte order?
  if (srcc<3+len) {
    wm_log_error("Read result short %d<%d",srcc,3+len);
    //return -1;
  }
  const uint8_t *payload=src+3;
  if (report->delegate.cb_read(report->delegate.userdata,addr,err,payload,len)<0) return -1;
  return 0;
}

/* Receive acknowledgement.
 */

static int wm_report_deliver_ack(struct wm_report *report,const uint8_t *src) {
  int rptid=src[0];
  int err=src[1];
  if (report->delegate.cb_ack(report->delegate.userdata,rptid,err)<0) return -1;
  return 0;
}

/* Receive core buttons.
 */

static int wm_report_deliver_core_buttons(struct wm_report *report,const uint8_t *src) {
  uint16_t buttons=(src[0]<<8)|src[1];
  if (buttons==report->buttons) return 0;

  if (wm_report_check_bit(report,report->buttons,buttons,0x0001,WM_BTNID_CORE_2)<0) return -1;
  if (wm_report_check_bit(report,report->buttons,buttons,0x0002,WM_BTNID_CORE_1)<0) return -1;
  if (wm_report_check_bit(report,report->buttons,buttons,0x0004,WM_BTNID_CORE_B)<0) return -1;
  if (wm_report_check_bit(report,report->buttons,buttons,0x0008,WM_BTNID_CORE_A)<0) return -1;
  if (wm_report_check_bit(report,report->buttons,buttons,0x0010,WM_BTNID_CORE_MINUS)<0) return -1;
  if (wm_report_check_bit(report,report->buttons,buttons,0x0080,WM_BTNID_CORE_HOME)<0) return -1;
  if (wm_report_check_bit(report,report->buttons,buttons,0x0100,WM_BTNID_CORE_LEFT)<0) return -1;
  if (wm_report_check_bit(report,report->buttons,buttons,0x0200,WM_BTNID_CORE_RIGHT)<0) return -1;
  if (wm_report_check_bit(report,report->buttons,buttons,0x0400,WM_BTNID_CORE_DOWN)<0) return -1;
  if (wm_report_check_bit(report,report->buttons,buttons,0x0800,WM_BTNID_CORE_UP)<0) return -1;
  if (wm_report_check_bit(report,report->buttons,buttons,0x1000,WM_BTNID_CORE_PLUS)<0) return -1;
  
  report->buttons=buttons;
  return 0;
}

/* Receive accelerometers.
 * This is 5 bytes, starting with the core buttons. (We need four bits out of there too).
 *   0000  _ X X _  _ _ _ _
 *   0001  _ Z Y _  _ _ _ _
 *   0002  X X X X  X X X X
 *   0003  Y Y Y Y  Y Y Y Y
 *   0004  Z Z Z Z  Z Z Z Z
 *   0005
 * X is 10 bits; Y and Z are 9.
 * We report all three as 10 bits for consistency, with a range of (-512..511).
 */

static int wm_report_deliver_accel(struct wm_report *report,const uint8_t *src) {
  int x=(src[2]<<2)|((src[0]&0x60)>>5);
  int y=(src[3]<<2)|((src[1]&0x20)?0x03:0x00);
  int z=(src[4]<<2)|((src[1]&0x40)?0x03:0x00);
  x-=512;
  y-=512;
  z-=512;
  if (wm_report_check_int(report,report->accelx,x,WM_BTNID_CORE_ACCELX)<0) return -1;
  if (wm_report_check_int(report,report->accely,x,WM_BTNID_CORE_ACCELY)<0) return -1;
  if (wm_report_check_int(report,report->accelz,x,WM_BTNID_CORE_ACCELZ)<0) return -1;
  report->accelx=x;
  report->accely=y;
  report->accelz=z;
  return 0;
}

/* Receive infrared.
 */

static int wm_report_deliver_ir(struct wm_report *report,const uint8_t *src,int srcc) {
  wm_log_debug("ir (%d)",srcc);//TODO
  return 0;
}

/* Receive nunchuk.
 * Stick is up-positive; we invert it per my personal preference (up-negative).
 */

static const uint8_t wm_null_report_nunchuk[6]={
  0x80,
  0x80,
  0x80,
  0x80,
  0x80,
  0x03,
};

static int wm_report_deliver_nunchuk(struct wm_report *report,const uint8_t *src,int srcc) {
  if (srcc<6) return 0;
  struct wm_report_nunchuk prev=report->nunchuk;
  
  report->nunchuk.sx=src[0]-128;
  report->nunchuk.sy=128-src[1];
  report->nunchuk.ax=((src[2]<<2)|((src[5]&0x0c)>>2))-512;
  report->nunchuk.ay=((src[3]<<2)|((src[5]&0x30)>>4))-512;
  report->nunchuk.az=((src[4]<<2)|((src[5]&0xc0)>>6))-512;
  report->nunchuk.buttons=((~src[5])&0x03);

  if (wm_report_check_int(report,prev.sx,report->nunchuk.sx,WM_BTNID_NUNCHUK_X)<0) return -1;
  if (wm_report_check_int(report,prev.sy,report->nunchuk.sy,WM_BTNID_NUNCHUK_Y)<0) return -1;
  if (wm_report_check_int(report,prev.ax,report->nunchuk.ax,WM_BTNID_NUNCHUK_ACCELX)<0) return -1;
  if (wm_report_check_int(report,prev.ay,report->nunchuk.ay,WM_BTNID_NUNCHUK_ACCELY)<0) return -1;
  if (wm_report_check_int(report,prev.az,report->nunchuk.az,WM_BTNID_NUNCHUK_ACCELZ)<0) return -1;
  if (wm_report_check_bit(report,prev.buttons,report->nunchuk.buttons,0x01,WM_BTNID_NUNCHUK_Z)<0) return -1;
  if (wm_report_check_bit(report,prev.buttons,report->nunchuk.buttons,0x02,WM_BTNID_NUNCHUK_C)<0) return -1;

  return 0;
}

/* Receive classic controller.
 * Sticks are up-positive; we invert them per my personal preference (up-negative).
 */

static const uint8_t wm_null_report_classic[6]={
  0xa0,
  0x40,
  0x10,
  0x00,
  0xff,
  0xff,
};

static int wm_report_deliver_classic(struct wm_report *report,const uint8_t *src,int srcc) {
  if (srcc<6) return 0;
  struct wm_report_classic prev=report->classic;

  report->classic.lx=(src[0]&0x3f)-32;
  report->classic.ly=32-(src[1]&0x3f);
  report->classic.rx=(((src[0]&0xc0)>>3)|((src[1]&0xc0)>>5)|(src[2]>>7))-16;
  report->classic.ry=16-(src[2]&0x1f);
  report->classic.la=((src[2]&0x60)>>2)|(src[3]>>5);
  report->classic.ra=(src[3]&0x1f);
  report->classic.buttons=~((src[4]<<8)|src[5]);

  if (wm_report_check_int(report,prev.lx,report->classic.lx,WM_BTNID_CLASSIC_LX)<0) return -1;
  if (wm_report_check_int(report,prev.ly,report->classic.ly,WM_BTNID_CLASSIC_LY)<0) return -1;
  if (wm_report_check_int(report,prev.rx,report->classic.rx,WM_BTNID_CLASSIC_RX)<0) return -1;
  if (wm_report_check_int(report,prev.ry,report->classic.ry,WM_BTNID_CLASSIC_RY)<0) return -1;
  if (wm_report_check_int(report,prev.la,report->classic.la,WM_BTNID_CLASSIC_LA)<0) return -1;
  if (wm_report_check_int(report,prev.ra,report->classic.ra,WM_BTNID_CLASSIC_RA)<0) return -1;
  if (prev.buttons!=report->classic.buttons) {
    if (wm_report_check_bit(report,prev.buttons,report->classic.buttons,0x0001,WM_BTNID_CLASSIC_UP)<0) return -1;
    if (wm_report_check_bit(report,prev.buttons,report->classic.buttons,0x0002,WM_BTNID_CLASSIC_LEFT)<0) return -1;
    if (wm_report_check_bit(report,prev.buttons,report->classic.buttons,0x0004,WM_BTNID_CLASSIC_ZR)<0) return -1;
    if (wm_report_check_bit(report,prev.buttons,report->classic.buttons,0x0008,WM_BTNID_CLASSIC_X)<0) return -1;
    if (wm_report_check_bit(report,prev.buttons,report->classic.buttons,0x0010,WM_BTNID_CLASSIC_A)<0) return -1;
    if (wm_report_check_bit(report,prev.buttons,report->classic.buttons,0x0020,WM_BTNID_CLASSIC_Y)<0) return -1;
    if (wm_report_check_bit(report,prev.buttons,report->classic.buttons,0x0040,WM_BTNID_CLASSIC_B)<0) return -1;
    if (wm_report_check_bit(report,prev.buttons,report->classic.buttons,0x0080,WM_BTNID_CLASSIC_ZL)<0) return -1;
    if (wm_report_check_bit(report,prev.buttons,report->classic.buttons,0x0200,WM_BTNID_CLASSIC_R)<0) return -1;
    if (wm_report_check_bit(report,prev.buttons,report->classic.buttons,0x0400,WM_BTNID_CLASSIC_PLUS)<0) return -1;
    if (wm_report_check_bit(report,prev.buttons,report->classic.buttons,0x0800,WM_BTNID_CLASSIC_HOME)<0) return -1;
    if (wm_report_check_bit(report,prev.buttons,report->classic.buttons,0x1000,WM_BTNID_CLASSIC_MINUS)<0) return -1;
    if (wm_report_check_bit(report,prev.buttons,report->classic.buttons,0x2000,WM_BTNID_CLASSIC_L)<0) return -1;
    if (wm_report_check_bit(report,prev.buttons,report->classic.buttons,0x4000,WM_BTNID_CLASSIC_DOWN)<0) return -1;
    if (wm_report_check_bit(report,prev.buttons,report->classic.buttons,0x8000,WM_BTNID_CLASSIC_RIGHT)<0) return -1;
  }

  return 0;
}

/* Receive extension report.
 */

static int wm_report_deliver_ext(struct wm_report *report,const uint8_t *src,int srcc) {
  switch (report->extid) {
    case WM_DEVICE_TYPE_NUNCHUK: return wm_report_deliver_nunchuk(report,src,srcc);
    case WM_DEVICE_TYPE_CLASSIC: return wm_report_deliver_classic(report,src,srcc);
    //TODO other extensions. specs at /Users/andy/doc/wiimote-extension.html. I'll have to fly blind though
  }
  return 0;
}

/* Receive interleaved reports. (3e/3f)
 * These expect the entire report, headers and all.
 */

static int wm_report_deliver_3e(struct wm_report *report,const uint8_t *src) {
  memcpy(report->rpt3e,src,23);
  if (wm_report_deliver_core_buttons(report,src+2)<0) return -1;
  return 0;
}

static int wm_report_deliver_3f(struct wm_report *report,const uint8_t *src) {
  
  if (wm_report_deliver_core_buttons(report,src+2)<0) return -1;

  /* Accelerometers. */
  int x=report->rpt3e[4];
  int y=src[4];
  int z=
    ((report->rpt3e[3]&0x60)<<1)|
    ((report->rpt3e[2]&0x60)>>1)|
    ((src[3]&0x60)>>3)|
    ((src[2]&0x60)>>5)
  ;
  x=((x<<2)|((x&1)?3:0))-512;
  y=((y<<2)|((y&1)?3:0))-512;
  z=((z<<2)|((z&1)?3:0))-512;
  if (wm_report_check_int(report,report->accelx,x,WM_BTNID_CORE_ACCELX)<0) return -1;
  if (wm_report_check_int(report,report->accely,x,WM_BTNID_CORE_ACCELY)<0) return -1;
  if (wm_report_check_int(report,report->accelz,x,WM_BTNID_CORE_ACCELZ)<0) return -1;
  report->accelx=x;
  report->accely=y;
  report->accelz=z;

  //TODO interleaved extension bytes
  
  return 0;
}

/* Receive report.
 */
 
int wm_report_deliver(struct wm_report *report,const void *src,int srcc) {
  if (!report) return -1;
  if (!src) return -1;
  const uint8_t *SRC=src;

  /* A few quick sanity checks. */
  if (srcc>23) {
    wm_log_warning("Ignoring long report (%d>23)",srcc);
    return 0;
  }
  if (srcc<2) {
    wm_log_warning("Ignoring short report (%d)",srcc);
    return 0;
  }
  if (SRC[0]!=0xa1) {
    wm_log_warning("Ignoring %d-byte report due to byte 0 == 0x%02x (expect 0xa1)",srcc,SRC[0]);
    return 0;
  }

  /* Ignore redundant reports.
   * My Rock Candy remotes send these in mode 0x30 even if we tell it not to.
   */  
  if ((srcc==report->pvrptc)&&!memcmp(src,report->pvrpt,srcc)) {
    return 0;
  }
  memcpy(report->pvrpt,src,srcc);
  report->pvrptc=srcc;

  //TODO
  char buf[256];
  int bufc=wm_report_repr(buf,sizeof(buf),src,srcc);
  if ((bufc>0)&&(bufc<=sizeof(buf))) {
    wm_log_trace("REPORT: %.*s",bufc,buf);
  } else {
    wm_log_trace("REPORT %d bytes",srcc);
  }

  /* Parse report bits based on the report ID. */
  switch (SRC[1]) {
    #define LEN(n) if (srcc<n) { \
      wm_log_warning("Ignoring report 0x%02x due to length %d < %d",SRC[1],srcc,n); \
      break; \
    }
    #define CORE if (wm_report_deliver_core_buttons(report,SRC+2)<0) return -1;
    #define STATUS if (wm_report_deliver_status(report,SRC+4)<0) return -1;
    #define ACCEL if (wm_report_deliver_accel(report,SRC+2)<0) return -1;
    #define EXT(p,c) if (wm_report_deliver_ext(report,SRC+p,c)<0) return -1;
    #define IR(p,c) if (wm_report_deliver_ir(report,SRC+p,c)<0) return -1;

    case 0x20: LEN( 8) CORE STATUS break;
    case 0x21: LEN( 8) CORE if (wm_report_deliver_rdmem(report,SRC+4,srcc-4)<0) return -1; break;
    case 0x22: LEN( 6) CORE if (wm_report_deliver_ack(report,SRC+4)<0) return -1; break;
    case 0x30: LEN( 4) CORE break;
    case 0x31: LEN( 7) CORE ACCEL break;
    case 0x32: LEN(12) CORE EXT(4,8) break;
    case 0x33: LEN(19) CORE ACCEL IR(7,12) break;
    case 0x34: LEN(23) CORE EXT(4,19) break;
    case 0x35: LEN(23) CORE ACCEL EXT(7,16) break;
    case 0x36: LEN(23) CORE IR(4,10) EXT(14,9) break;
    case 0x37: LEN(23) CORE ACCEL IR(7,10) EXT(17,6) break;
    case 0x3d: LEN(23) EXT(2,21) break;
    case 0x3e: LEN(23) if (wm_report_deliver_3e(report,src)<0) return -1; break;
    case 0x3f: LEN(23) if (wm_report_deliver_3f(report,src)<0) return -1; break;

    #undef LEN
    #undef CORE
    #undef STATUS
    #undef ACCEL
    #undef EXT
    #undef IR
    default: {
        wm_log_warning("Ignoring unknown %d-byte report ID 0x%02x",srcc,SRC[1]);
      }
  }
  return 0;
}

/* Get button.
 */
 
int wm_report_get_button(const struct wm_report *report,int btnid) {
  if (!report) return 0;
  //TODO
  return 0;
}

/* Set button.
 */
 
int wm_report_set_button(struct wm_report *report,int btnid,int value) {
  if (!report) return -1;
  //TODO
  wm_log_trace("%s %d=%d",__func__,btnid,value);
  return 0;
}

/* Set properties.
 */
 
int wm_report_set_rptid(struct wm_report *report,uint8_t rptid) {
  if (!report) return -1;
  switch (rptid) {
    case 0x30: case 0x31: case 0x32: case 0x33: case 0x34:
    case 0x35: case 0x36: case 0x37: case 0x3d: case 0x3e: case 0x3f:
      break;
    default: return -1;
  }
  report->rptid=rptid;
  return 0;
}

int wm_report_set_rumble(struct wm_report *report,int rumble) {
  if (!report) return -1;
  report->rumble=rumble?0x01:0x00;//TODO
  return 0;
}

int wm_report_set_continuous(struct wm_report *report,int continuous) {
  if (!report) return -1;
  report->continuous=continuous?0x02:0x00;//TODO
  return 0;
}

/* Change extension.
 */

int wm_report_set_extension(struct wm_report *report,int extid) {
  if (!report) return -1;

  /* Ensure it's a known extension and not the one we already have. */
  if (extid==report->extid) return 0;
  switch (extid) {
    case 0:
    case WM_DEVICE_TYPE_NUNCHUK:
    case WM_DEVICE_TYPE_CLASSIC:
      break;
    default: return -1;
  }

  /* Drop any state associated with the previous extension. */
  switch (report->extid) {
    case WM_DEVICE_TYPE_NUNCHUK: {
        if (wm_report_deliver_nunchuk(report,wm_null_report_nunchuk,sizeof(wm_null_report_nunchuk))<0) return -1;
      } break;
    case WM_DEVICE_TYPE_CLASSIC: {
        if (wm_report_deliver_nunchuk(report,wm_null_report_classic,sizeof(wm_null_report_classic))<0) return -1;
      } break;
  }

  report->extid=extid;
  return 0;
}

/* Compose requests.
 */
 
int wm_report_compose_led(void *dst,int dsta,struct wm_report *report,int led1,int led2,int led3,int led4) {
  if (!dst||(dsta<3)) return -1;
  uint8_t *DST=dst;
  DST[0]=0xa2;
  DST[1]=0x11;
  DST[2]=(led1?0x80:0)|(led2?0x40:0)|(led3?0x20:0)|(led4?0x10:0);
  return 3;
}

int wm_report_compose_rptid(void *dst,int dsta,struct wm_report *report) {
  if (!dst||(dsta<4)) return -1;
  uint8_t *DST=dst;
  DST[0]=0xa2;
  DST[1]=0x12;
  DST[2]=report->continuous|report->rumble;
  DST[3]=report->rptid;
  return 4;
}

int wm_report_compose_write(void *dst,int dsta,struct wm_report *report,int addr,const void *src,int srcc) {
  if (!dst||(dsta<23)) return -1;
  if (srcc>16) return -1;
  uint8_t *DST=dst;
  DST[0]=0xa2;
  DST[1]=0x16;
  DST[2]=addr>>24;
  DST[3]=addr>>16;
  DST[4]=addr>>8;
  DST[5]=addr;
  DST[6]=srcc;
  memcpy(DST+7,src,srcc);
  memset(DST+7+srcc,0,16-srcc);
  return 23;
}

int wm_report_compose_read(void *dst,int dsta,struct wm_report *report,int addr,int size) {
  if (!dst||(dsta<8)) return -1;
  if ((size<0)||(size>0xffff)) return -1;
  uint8_t *DST=dst;
  DST[0]=0xa2;
  DST[1]=0x17;
  DST[2]=addr>>24;
  DST[3]=addr>>16;
  DST[4]=addr>>8;
  DST[5]=addr;
  DST[6]=size>>8;
  DST[7]=size;
  return 8;
}
