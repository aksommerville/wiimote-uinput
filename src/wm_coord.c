#include "wiimote.h"
#include "wm_coord.h"
#include "wm_config.h"
#include "wm_delivery.h"
#include "wm_transport.h"
#include "wm_report.h"
#include "wm_enums.h"
#include "wm_text.h"
#include <unistd.h>

#define WM_EXT_STATE_UNSET      0
#define WM_EXT_STATE_WAIT_ACK1  1
#define WM_EXT_STATE_WAIT_ACK2  2
#define WM_EXT_STATE_WAIT_EXTID 3

/* Object definition.
 */

struct wm_coord {
  struct wm_transport *transport;
  struct wm_report *report;
  struct wm_delivery *delivery_core;
  struct wm_delivery *delivery_ext; // Always exists but not always connected.
  struct wm_config *config; // WEAK
  int startup;
  int ext_state;
  int extid;
};

/* Object lifecycle.
 */

struct wm_coord *wm_coord_new() {
  struct wm_coord *coord=calloc(1,sizeof(struct wm_coord));
  if (!coord) return 0;

  return coord;
}

void wm_coord_del(struct wm_coord *coord) {
  if (!coord) return;
  
  wm_transport_del(coord->transport);
  wm_report_del(coord->report);
  wm_delivery_del(coord->delivery_core);
  wm_delivery_del(coord->delivery_ext);

  free(coord);
}

/* Should we create a separate delivery for the extension?
 */

static int wm_coord_requires_delivery_ext(const struct wm_coord *coord) {
  switch (coord->extid) {
    case WM_DEVICE_TYPE_NUNCHUK: return wm_config_get_nunchuk_separate(coord->config);
    case WM_DEVICE_TYPE_CLASSIC: return wm_config_get_classic_separate(coord->config);
  }
  return 0;
}

/* Finish extension handshake with ID translated to a known extension.
 */

static int wm_coord_accept_extension(struct wm_coord *coord,int extid) {
  coord->extid=extid;

  uint8_t req[32];
  int reqc;
  if (wm_report_set_rptid(coord->report,0x34)<0) return -1;
  if ((reqc=wm_report_compose_rptid(req,sizeof(req),coord->report))<0) return -1;
  if (wm_transport_write(coord->transport,req,reqc)!=reqc) return -1;

  wm_log_info("Connected extension '%s'",wm_device_type_repr(extid));

  if (wm_report_set_extension(coord->report,extid)<0) return -1;

  if (wm_coord_requires_delivery_ext(coord)) {
    wm_log_debug("Must create separate delivery for extension.");
    if (wm_delivery_set_device_type(coord->delivery_ext,coord->extid)<0) return -1;
    if (wm_delivery_connect(coord->delivery_ext)<0) return -1;
  } else {
    wm_log_debug("Extension will share core delivery.");
  }
  
  return 0;
}

/* Finish extension handshake, ID in hand.
 * (rawid) is 6 bytes:
 *   00 00 a4 20 00 00 Nunchuk
 *   00 00 a4 20 01 01 Classic
 *   01 00 a4 20 01 01 Classic Pro
 *   ff 00 a4 20 00 13 Drawsome Graphics Tablet (not fully documented at WiiBrew, and I don't have one)
 *   00 00 a4 20 01 03 Guitar
 *   01 00 a4 20 01 03 Drums
 *   03 00 a4 20 01 03 Turntable
 */

static int wm_coord_receive_extension_id(struct wm_coord *coord,const uint8_t *rawid) {

  if (!memcmp(rawid,"\x00\x00\xa4\x20\x00\x00",6)) return wm_coord_accept_extension(coord,WM_DEVICE_TYPE_NUNCHUK);
  if (!memcmp(rawid,"\x00\x00\xa4\x20\x01\x01",6)) return wm_coord_accept_extension(coord,WM_DEVICE_TYPE_CLASSIC);
  
  char buf[32];
  int bufc=wm_report_repr(buf,sizeof(buf),rawid,6);
  if ((bufc<0)||(bufc>sizeof(buf))) bufc=0;
  wm_log_warning("Unknown extension ID: %.*s",bufc,buf);
  return 0;
}

/* Connect extension. (Begin process)
 * 1. Write {0x55} to 0x04a400f0.
 * 2. Write {0x00} to 0x04a400fb.
 * 3. Read 6 from 0x04a400fa.
 * 4. Set report mode to 0x32.
 * TODO This works with Nintendo devices but not Rock Candy.
 */

static int wm_coord_connect_extension(struct wm_coord *coord) {
  if (coord->ext_state==WM_EXT_STATE_UNSET) {
    /* "The New Way" */
    wm_log_debug("wm_coord_connect_extension: writing 0x55 to 0x04a400f0");
    uint8_t req[32];
    int reqc=wm_report_compose_write(req,sizeof(req),coord->report,0x04a400f0,"\x55",1);
    if (wm_transport_write(coord->transport,req,reqc)!=reqc) return -1;
    coord->ext_state=WM_EXT_STATE_WAIT_ACK1;
    /**/
    /* "The Old Way"
    wm_log_debug("wm_coord_connect_extension: writing 0x00 to 0x04a40040");
    uint8_t req[32];
    int reqc=wm_report_compose_write(req,sizeof(req),coord->report,0x04a40040,"\0",1);
    if (reqc<0) return -1;
    if (wm_transport_write(coord->transport,req,reqc)!=reqc) return -1;
    coord->ext_state=WM_EXT_STATE_WAIT_ACK2;
    /**/
  }
  return 0;
}

/* Disconnect extension.
 */

static int wm_coord_disconnect_extension(struct wm_coord *coord) {

  uint8_t req[32];
  if (wm_report_set_extension(coord->report,0)<0) return -1;
  if (wm_report_set_rptid(coord->report,0x30)<0) return -1;
  int reqc=wm_report_compose_rptid(req,sizeof(req),coord->report);
  if (reqc<0) return -1;
  if (wm_transport_write(coord->transport,req,reqc)!=reqc) return -1;
  coord->ext_state=WM_EXT_STATE_UNSET;

  if (wm_delivery_disconnect(coord->delivery_ext)<0) return -1;

  wm_log_info("Disconnected extension.");

  return 0;
}

/* Report callback: button state changed.
 */

static int wm_coord_cb_button(void *userdata,int btnid,int value) {
  struct wm_coord *coord=userdata;
  
  wm_log_trace("%s %d[%s]=%d",__func__,btnid,wm_btnid_repr(btnid),value);

  /* Look for events we handle internally. */
  switch (btnid) {
  
    case WM_BTNID_CORE_EXTPRESENT: {
        if (value) {
          wm_log_debug("WM_BTNID_CORE_EXTPRESENT, begin extension handshake");
          if (wm_coord_connect_extension(coord)<0) return -1;
        } else {
          if (wm_coord_disconnect_extension(coord)<0) return -1;
        }
      } break;
      
  }

  /* If delivery_ext is connected and this button belongs to the extension, report it there. */
  if (wm_btnid_is_extension(btnid)&&wm_delivery_is_connected(coord->delivery_ext)) {
    if (wm_delivery_set_button(coord->delivery_ext,btnid,value)<0) return -1;
  } else {
    if (wm_delivery_set_button(coord->delivery_core,btnid,value)<0) return -1;
  }
  
  return 0;
}

/* Report callback: ACK
 */

static int wm_coord_cb_ack(void *userdata,uint8_t rptid,uint8_t result) {
  struct wm_coord *coord=userdata;
  wm_log_trace("ACK 0x%02x = 0x%02x",rptid,result);

  if (coord->ext_state==WM_EXT_STATE_WAIT_ACK1) {
    if (rptid==0x16) {
      if (result) {
        wm_log_error("Error %d enabling extension. After 0x55 to 0x04a400f0.",result);
        coord->ext_state=WM_EXT_STATE_UNSET;
      } else {
        wm_log_debug("Extension handshake, ACK1: writing 0x00 to 0x04a400fb");
        uint8_t req[32];
        int reqc=wm_report_compose_write(req,sizeof(req),coord->report,0x04a400fb,"\0",1);
        if (reqc<0) return -1;
        if (wm_transport_write(coord->transport,req,reqc)!=reqc) return -1;
        coord->ext_state=WM_EXT_STATE_WAIT_ACK2;
      }
    }
  }

  if (coord->ext_state==WM_EXT_STATE_WAIT_ACK2) {
    if (rptid==0x16) {
      if (result) {
        wm_log_error("Error %d enabling extension. After 0x00 to 0x04a400fb.",result);
        coord->ext_state=WM_EXT_STATE_UNSET;
      } else {
        wm_log_debug("Extension handshake, ACK2: reading 6 from 0x04a400fa");
        uint8_t req[32];
        int reqc=wm_report_compose_read(req,sizeof(req),coord->report,0x04a400fa,6);
        if (reqc<0) return -1;
        if (wm_transport_write(coord->transport,req,reqc)!=reqc) return -1;
        coord->ext_state=WM_EXT_STATE_WAIT_EXTID;
      }
    }
  }
  
  return 0;
}

/* Report callback: read
 */

static int wm_coord_cb_read(void *userdata,uint16_t addr,int err,const void *src,int srcc) {
  struct wm_coord *coord=userdata;
  if (err) {
    wm_log_error("READ[%04x]: error %d",addr,err);
    if (coord->ext_state==WM_EXT_STATE_WAIT_EXTID) {
      coord->ext_state=WM_EXT_STATE_UNSET;
    }
    
  } else {
  
    if (wm_log_trace_enabled()) {
      char buf[64];
      int bufc=wm_report_repr(buf,sizeof(buf),src,srcc);
      if ((bufc<0)||(bufc>sizeof(buf))) bufc=0;
      wm_log_trace("READ[%04x]: %.*s",addr,bufc,buf);
    }

    /* Are we expecting the extension ID, and is this it? */
    if (coord->ext_state==WM_EXT_STATE_WAIT_EXTID) {
      if ((addr==0x00fa)&&(srcc>=6)) {
        if (wm_coord_receive_extension_id(coord,src)<0) return -1;
      }
      coord->ext_state=WM_EXT_STATE_UNSET;
      return 0;
    }

    //TODO Other reactions to successful read.
  }
  return 0;
}

/* Startup dependents.
 */

static int wm_coord_startup_transport(struct wm_coord *coord,struct wm_config *config) {
  if (coord->transport) return -1;
  
  uint8_t bdaddr[6];
  if (wm_config_get_device_by_name(bdaddr,config,wm_config_get_device_name(config),-1)<0) {
    wm_log_error("Failed to resolve device name '%s'",wm_config_get_device_name(config));
    return -1;
  }

  if (!(coord->transport=wm_transport_new(bdaddr,wm_config_get_retry_count(config)))) return -1;

  if (wm_transport_connect(coord->transport)<0) {
    return -1;
  }
  
  return 0;
}

static int wm_coord_startup_report(struct wm_coord *coord,struct wm_config *config) {
  if (coord->report) return -1;

  struct wm_report_delegate delegate={
    .cb_button=wm_coord_cb_button,
    .cb_ack=wm_coord_cb_ack,
    .cb_read=wm_coord_cb_read,
    .userdata=coord,
  };

  if (!(coord->report=wm_report_new(&delegate))) return -1;
  if (wm_report_configure(coord->report,config)<0) {
    return -1;
  }

  return 0;
}

static int wm_coord_startup_delivery(struct wm_coord *coord,struct wm_config *config) {
  if (coord->delivery_core||coord->delivery_ext) return -1;
  if (!(coord->delivery_core=wm_delivery_new())) return -1;
  if (!(coord->delivery_ext=wm_delivery_new())) return -1;

  if (wm_delivery_set_device_type(coord->delivery_core,WM_DEVICE_TYPE_WIIMOTE)<0) return -1;
  if (wm_delivery_set_nunchuk_separate(coord->delivery_core,wm_config_get_nunchuk_separate(config))<0) return -1;
  if (wm_delivery_set_classic_separate(coord->delivery_core,wm_config_get_classic_separate(config))<0) return -1;

  const char *path=0;
  int pathc=wm_config_get_uinput_path(&path,config);
  if (pathc<0) return -1;
  if (wm_delivery_set_uinput_path(coord->delivery_core,path,pathc)<0) return -1;
  if (wm_delivery_set_uinput_path(coord->delivery_ext,path,pathc)<0) return -1;

  if (wm_delivery_set_name(coord->delivery_core,wm_config_get_device_name(config),-1)<0) return -1;
  if (wm_delivery_set_name(coord->delivery_ext,wm_config_get_device_name(config),-1)<0) return -1;

  if (wm_delivery_connect(coord->delivery_core)<0) return -1;

  return 0;
}

/* Begin device handshake.
 */

static int wm_coord_handshake(struct wm_coord *coord) {
  uint8_t req[32];
  int reqc;
  
  if ((reqc=wm_report_compose_led(req,sizeof(req),coord->report,1,1,1,1))<0) return -1;
  if (wm_transport_write(coord->transport,req,reqc)!=reqc) return -1;
  
  if (wm_report_set_rptid(coord->report,0x30)<0) return -1;
  if ((reqc=wm_report_compose_rptid(req,sizeof(req),coord->report))<0) return -1;
  if (wm_transport_write(coord->transport,req,reqc)!=reqc) return -1;
  
  return 0;
}

/* Start up, main entry point.
 */

int wm_coord_startup(struct wm_coord *coord,struct wm_config *config) {
  if (!coord||!config) return -1;
  if (coord->startup) return -1;

  coord->config=config;
  
  if (wm_coord_startup_transport(coord,config)<0) return -1;
  if (wm_coord_startup_report(coord,config)<0) return -1;
  if (wm_coord_startup_delivery(coord,config)<0) return -1;

  if (wm_coord_handshake(coord)<0) return -1;
  
  coord->startup=1;
  return 0;
}

int wm_coord_is_running(const struct wm_coord *coord) {
  if (!coord) return 0;
  return coord->startup;
}

/* Shut down.
 */

int wm_coord_shutdown(struct wm_coord *coord) {
  if (!coord) return -1;
  wm_transport_del(coord->transport);
  coord->transport=0;
  wm_report_del(coord->report);
  coord->report=0;
  wm_delivery_del(coord->delivery_core);
  coord->delivery_core=0;
  wm_delivery_del(coord->delivery_ext);
  coord->delivery_ext=0;
  coord->startup=0;
  return 0;
}

/* Update.
 */

int wm_coord_update(struct wm_coord *coord) {
  if (!coord) return -1;
  if (!coord->startup) return -1;

  /* Is a report ready? */
  int err=wm_transport_poll(coord->transport,1000);
  if (err<0) return -1;
  if (!err) return 0;

  /* Read next report. */
  uint8_t rpt[32]={0};
  int rptc=wm_transport_read(rpt,sizeof(rpt),coord->transport);
  if (rptc<0) return -1;
  if (!rptc) return wm_coord_shutdown(coord);

  /* Process report, digested events will come back through our callbacks. */
  if (wm_report_deliver(coord->report,rpt,rptc)<0) {
    return -1;
  }

  /* Alert uinput that the report is complete. */
  if (wm_delivery_synchronize(coord->delivery_core)<0) return -1;
  if (wm_delivery_synchronize(coord->delivery_ext)<0) return -1;
  
  return 0;
}
