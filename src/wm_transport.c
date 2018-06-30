#include "wiimote.h"
#include "wm_transport.h"
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>

/* Object definition.
 */

struct wm_transport {
  int fdr,fdw;
  struct sockaddr_l2 saddr;
  int retry_count;
};

/* Object lifecycle.
 */
 
struct wm_transport *wm_transport_new(const void *bdaddr,int retry_count) {
  if (!bdaddr||(retry_count<1)) return 0;
  struct wm_transport *transport=calloc(1,sizeof(struct wm_transport));
  if (!transport) return 0;

  transport->fdr=-1;
  transport->fdw=-1;
  transport->retry_count=retry_count;

  transport->saddr.l2_family=AF_BLUETOOTH;
  memcpy(&transport->saddr.l2_bdaddr,bdaddr,6);

  return transport;
}

void wm_transport_del(struct wm_transport *transport) {
  if (!transport) return;
  if (transport->fdr>=0) close(transport->fdr);
  if (transport->fdw>=0) close(transport->fdw);
  free(transport);
}

/* Trivial accessors.
 */

int wm_transport_is_connected(const struct wm_transport *transport) {
  if (!transport) return 0;
  return (transport->fdr>=0)?1:0;
}

/* Connect.
 */

int wm_transport_connect(struct wm_transport *transport) {
  if (!transport) return -1;
  if (transport->fdr>=0) return 0;
  if (transport->fdw>=0) return -1;
  wm_log_trace("%s",__func__);

  if ((transport->fdr=socket(PF_BLUETOOTH,SOCK_SEQPACKET,BTPROTO_L2CAP))<0) {
    wm_log_error("socket() failed: %m");
    return -1;
  }
  if ((transport->fdw=socket(PF_BLUETOOTH,SOCK_SEQPACKET,BTPROTO_L2CAP))<0) {
    wm_log_error("socket() failed: %m");
    wm_transport_disconnect(transport);
    return -1;
  }

  wm_log_info("Connecting to device (PSM 0x13)...");
  transport->saddr.l2_psm=0x13;
  if (connect(transport->fdr,(struct sockaddr*)&transport->saddr,sizeof(transport->saddr))<0) {
    wm_log_error("connect() failed: %m");
    wm_transport_disconnect(transport);
    return -1;
  }

  wm_log_info("Connecting to device (PSM 0x11)...");
  transport->saddr.l2_psm=0x11;
  if (connect(transport->fdw,(struct sockaddr*)&transport->saddr,sizeof(transport->saddr))<0) {
    wm_log_error("connect() failed: %m");
    wm_transport_disconnect(transport);
    return -1;
  }

  wm_log_info("Connected");
  return 0;
}

/* Disconnect.
 */
 
int wm_transport_disconnect(struct wm_transport *transport) {
  if (!transport) return -1;
  wm_log_trace("%s",__func__);
  if (transport->fdr>=0) {
    close(transport->fdr);
    transport->fdr=-1;
  }
  if (transport->fdw>=0) {
    close(transport->fdw);
    transport->fdw=-1;
  }
  return 0;
}

/* I/O.
 */
 
int wm_transport_read(void *dst,int dsta,struct wm_transport *transport) {
  if (!dst||(dsta<1)||!transport) return -1;
  if (transport->fdr<0) return -1;
  return read(transport->fdr,dst,dsta);
}

int wm_transport_write(struct wm_transport *transport,const void *src,int srcc) {
  if (!transport||!src||(srcc<1)) return -1;
  if (transport->fdw<0) return -1;
  return write(transport->fdw,src,srcc);
}

/* Poll.
 */
 
int wm_transport_poll(struct wm_transport *transport,int to_ms) {
  if (!transport) return -1;
  if (transport->fdr<0) return -1;
  struct pollfd pollfd={0};
  pollfd.fd=transport->fdr;
  pollfd.events=POLLIN|POLLHUP|POLLERR;
  int err=poll(&pollfd,1,to_ms);
  return err;
}
