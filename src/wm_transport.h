/* wm_transport.h
 * Manages raw communication with the device.
 */

#ifndef WM_TRANSPORT_H
#define WM_TRANSPORT_H

struct wm_transport;

/* bdaddr and retry count are fixed at construction, but we do not automatically connect.
 */
struct wm_transport *wm_transport_new(const void *bdaddr,int retry_count);
void wm_transport_del(struct wm_transport *transport);

/* Establish or break the socket connection.
 */
int wm_transport_connect(struct wm_transport *transport);
int wm_transport_disconnect(struct wm_transport *transport);
int wm_transport_is_connected(const struct wm_transport *transport);

/* Transfer data.
 * L2CAP is packet-oriented, so these should be discrete packets, not really a loose stream.
 * Both functions return the length transferred.
 * Both functions are blocking.
 * Read returns 0 if the connection is lost.
 */
int wm_transport_read(void *dst,int dsta,struct wm_transport *transport);
int wm_transport_write(struct wm_transport *transport,const void *src,int srcc);

/* Wait for incoming data to become available.
 * Behavior depends on (to_ms):
 *   >0: Sleep for so many milliseconds
 *    0: Return immediately
 *   <0: Sleep indefinitely
 * Returns:
 *   >0: Read will not block
 *    0: Read will block
 *   <0: Error (eg not connected)
 */
int wm_transport_poll(struct wm_transport *transport,int to_ms);

#endif
