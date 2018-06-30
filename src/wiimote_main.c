#include "wiimote.h"

#define DEVICE_LIST_PATH "/etc/wiimote/devices"

/* signal
 *****************************************************************************/

static volatile int wiimote_sigc=0;

static void rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: case SIGQUIT: case SIGTERM: {
        if (++wiimote_sigc>=3) { fprintf(stderr,"too many pending signals, abort\n"); exit(1); }
      } break;
  }
}

/* evaluate device address
 *****************************************************************************/

static inline int wiimote_hexdigit_eval(char src) {
  if ((src>='0')&&(src<='9')) return src-'0';
  if ((src>='a')&&(src<='f')) return src-'a'+10;
  if ((src>='A')&&(src<='F')) return src-'A'+10;
  return -1;
}

static int wiimote_bdaddr_eval(struct sockaddr_l2 *dst,const char *src,int literal_only) {
  if (!src) return -1;
  int srcc=0; while (src[srcc]) srcc++;
  dst->l2_family=AF_BLUETOOTH;
  if (srcc==17) {
    if ((src[2]==':')&&(src[5]==':')&&(src[8]==':')&&(src[11]==':')&&(src[14]==':')) {
      uint8_t buf[6];
      int i; for (i=0;i<6;i++) {
        int hi=wiimote_hexdigit_eval(src[i*3]);
        int lo=wiimote_hexdigit_eval(src[i*3+1]);
        if ((hi<0)||(lo<0)) goto _not_bdaddr_;
        buf[5-i]=(hi<<4)|lo;
      }
      memcpy(&dst->l2_bdaddr,buf,6);
      return 0;
    }
  }
 _not_bdaddr_:
  if (!literal_only) {
    int fd=open(DEVICE_LIST_PATH,O_RDONLY);
    if (fd>=0) {
      char buf[1024];
      int bufc=read(fd,buf,sizeof(buf));
      if (bufc>0) { // NB: some contents are ignored if file longer than 1024 bytes
        int bufp=0; while (bufp<bufc) {
          if ((unsigned char)buf[bufp]<=0x20) { bufp++; continue; }
          char *line=buf+bufp;
          int linec=0,cmt=0;
          while ((bufp<bufc)&&(buf[bufp]!=0x0a)&&(buf[bufp]!=0x0d)) {
            if (buf[bufp]=='#') cmt=1;
            else if (!cmt) linec++;
            bufp++;
          }
          while (linec&&((unsigned char)line[linec-1]<=0x20)) linec++;
          if (!linec) continue;
          if (linec<18) {
            fprintf(stderr,"%s: malformed line: %.*s\n",DEVICE_LIST_PATH,linec,line);
            continue;
          }
          line[17]=0;
          if (wiimote_bdaddr_eval(dst,line,1)<0) {
            fprintf(stderr,"%s: expected bdaddr, found '%s'\n",DEVICE_LIST_PATH,line);
            continue;
          }
          line+=18; linec-=18;
          while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
          if (linec!=srcc) continue;
          if (strncasecmp(line,src,srcc)) continue;
          close(fd);
          return 0;
        }
      }
      close(fd);
    }
  }
  return -1;
}

/* uinput
 *****************************************************************************/

static int wiimote_uinput_fd=-1;
static int wiimote_require_sync=0;

static int wiimote_uinput_init(const char *name) {
  int i;
  
  if ((wiimote_uinput_fd=open("/dev/uinput",O_RDWR))<0) {
    fprintf(stderr,"/dev/uinput: %m\n");
    return -1;
  }

  struct uinput_user_dev uud={0};
  int namec=0;
  if (!name) name="Wii Remote";
  while (name[namec]&&(namec<sizeof(uud.name))) namec++;
  memcpy(uud.name,name,namec);
  uud.name[namec]=0;
  uud.absmin[ABS_X]=uud.absmin[ABS_Y]=-1;
  uud.absmax[ABS_X]=uud.absmax[ABS_Y]=1;
  if (write(wiimote_uinput_fd,&uud,sizeof(uud))!=sizeof(uud)) {
    fprintf(stderr,"/dev/uinput: writing uinput_user_dev: %m\n");
    return -1;
  }
  if (ioctl(wiimote_uinput_fd,UI_SET_EVBIT,EV_KEY)<0) return -1;
  if (ioctl(wiimote_uinput_fd,UI_SET_EVBIT,EV_ABS)<0) return -1;
  if (ioctl(wiimote_uinput_fd,UI_SET_ABSBIT,ABS_X)<0) return -1;
  if (ioctl(wiimote_uinput_fd,UI_SET_ABSBIT,ABS_Y)<0) return -1;
  if (ioctl(wiimote_uinput_fd,UI_SET_KEYBIT,BTN_A)<0) return -1;
  if (ioctl(wiimote_uinput_fd,UI_SET_KEYBIT,BTN_B)<0) return -1;
  if (ioctl(wiimote_uinput_fd,UI_SET_KEYBIT,BTN_1)<0) return -1;
  if (ioctl(wiimote_uinput_fd,UI_SET_KEYBIT,BTN_2)<0) return -1;
  if (ioctl(wiimote_uinput_fd,UI_SET_KEYBIT,BTN_START)<0) return -1; // plus
  if (ioctl(wiimote_uinput_fd,UI_SET_KEYBIT,BTN_SELECT)<0) return -1; // minus
  if (ioctl(wiimote_uinput_fd,UI_SET_KEYBIT,BTN_MODE)<0) return -1; // home
  if (ioctl(wiimote_uinput_fd,UI_DEV_CREATE)<0) {
    fprintf(stderr,"/dev/uinput: UI_CREATE_DEV: %m\n");
    return -1;
  }
  
  return 0;
}

static void wiimote_uinput_quit() {
  if (wiimote_uinput_fd>=0) close(wiimote_uinput_fd);
}

/* send event to uinput
 *****************************************************************************/

static int wiimote_send_event(int type,int code,int value) {
  struct input_event evt={.type=type,.code=code,.value=value};
  if (write(wiimote_uinput_fd,&evt,sizeof(evt))!=sizeof(evt)) return -1;
  if (type==EV_SYN) wiimote_require_sync=0;
  else wiimote_require_sync=1;
  return 0;
}

static int wiimote_synchronize_output() {
  if (!wiimote_require_sync) return 0;
  return wiimote_send_event(EV_SYN,SYN_REPORT,0);
}

/* receive buttons report
 *****************************************************************************/

/* 
Bit	Mask	First Byte	Second Byte
0	 0x01	 D-Pad Left	 Two
1	 0x02	 D-Pad Right	 One
2	 0x04	 D-Pad Down	 B
3	 0x08	 D-Pad Up	 A
4	 0x10	 Plus	 Minus
5	 0x20	 Other uses	 Other uses
6	 0x40	 Other uses	 Other uses
7	 0x80	 Unknown	 Home
*/
static int wiimote_rcvrpt(uint16_t current,uint16_t previous) {
  #define BTN(mask,btnid) \
    if ((current&mask)&&!(previous&mask)) { if (wiimote_send_event(EV_KEY,btnid,1)<0) return -1; } \
    else if (!(current&mask)&&(previous&mask)) { if (wiimote_send_event(EV_KEY,btnid,0)<0) return -1; }
  #define AXIS(masklo,maskhi,absid) { \
    int vcr,vpv; \
    switch (current&(masklo|maskhi)) { \
      case masklo: vcr=-1; break; \
      case maskhi: vcr=1; break; \
      default: vcr=0; break; \
    } \
    switch (previous&(masklo|maskhi)) { \
      case masklo: vpv=-1; break; \
      case maskhi: vpv=1; break; \
      default: vpv=0; break; \
    } \
    if (vcr!=vpv) { if (wiimote_send_event(EV_ABS,absid,vcr)<0) return -1; } \
  }
  BTN(0x0001,BTN_2)
  BTN(0x0002,BTN_1)
  BTN(0x0004,BTN_B)
  BTN(0x0008,BTN_A)
  BTN(0x0010,BTN_SELECT)
  BTN(0x0080,BTN_MODE)
  BTN(0x1000,BTN_START)
  AXIS(0x0800,0x0400,ABS_X)
  AXIS(0x0200,0x0100,ABS_Y)
  #undef BTN
  #undef AXIS

  //XXX Since we haven't provided an easy way to kill the connection, kill it any time HOME is pressed.
  if (current&0x0080) return -1;
  
  return 0;
}

/* daemonize: borrowed from Doug Potter http://www.itp.uzh.ch/~dpotter/howto/daemonize
 *****************************************************************************/

static void daemonize(void)
{
    pid_t pid, sid;

    /* already a daemon */
    if ( getppid() == 1 ) return;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) exit (1);
    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0) exit (0);

    /* At this point we are executing as the child process */

    /* Change the file mode mask */
    umask(0);

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    /* Change the current working directory.  This prevents the current
       directory from being locked; hence not being able to remove it. */
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    /* Redirect standard files to /dev/null */
    freopen( "/dev/null", "r", stdin);
    freopen( "/dev/null", "w", stdout);
    freopen( "/dev/null", "w", stderr);
}

/* main
 *****************************************************************************/

int main(int argc,char **argv) {
  int fdr=-1,fdw=-1;
  struct sockaddr_l2 saddr={0};
  uint16_t rpt=0,pvrpt=0;

  signal(SIGINT,rcvsig);
  signal(SIGTERM,rcvsig);
  signal(SIGQUIT,rcvsig);

  /* Acquire device address. */
  if (argc!=2) {
    fprintf(stderr,"Usage: %s BDADDR\n",argv[0]);
    return 1;
  }
  if (wiimote_bdaddr_eval(&saddr,argv[1],0)<0) {
    fprintf(stderr,"%s: failed to evaluate device address '%s'\n",argv[0],argv[1]);
    return 1;
  }

  /* Connect to device -- we need separate read and write sockets. */
  printf("Creating sockets...\n");
  if (((fdr=socket(PF_BLUETOOTH,SOCK_SEQPACKET,BTPROTO_L2CAP))<0)
    ||((fdw=socket(PF_BLUETOOTH,SOCK_SEQPACKET,BTPROTO_L2CAP))<0)
  ) {
    fprintf(stderr,"%s: failed to create bluetooth sockets: %m\n",argv[0]);
    goto _done_;
  }
  printf("Connecting read pipe...\n");
  saddr.l2_psm=0x13;
  if (connect(fdr,(struct sockaddr*)&saddr,sizeof(saddr))<0) {
    fprintf(stderr,"%s: failed to open connection: %m\n",argv[1]);
    goto _done_;
  }
  printf("Connecting write pipe...\n");
  saddr.l2_psm=0x11;
  if (connect(fdw,(struct sockaddr*)&saddr,sizeof(saddr))<0) {
    fprintf(stderr,"%s: failed to open connection: %m\n",argv[1]);
    goto _done_;
  }

  /* Set LEDs all on. */
  printf("Setting LEDs...\n");
  { unsigned char req[3]={0xa2,0x11,0xf0};
    if (write(fdw,req,sizeof(req))!=sizeof(req)) goto _done_;
  }

  /* Set reporting mode.
   * This is not necessary for my Nintendo remotes, but it is for the Rock Candies.
   * Bit 0x04 of byte 2 is supposed to control continuous reporting, but the Rock Candies don't respect it. Whatever.
   */
  printf("Setting report mode...\n");
  { unsigned char req[4]={0xa2,0x12,0x00,0x30};
    if (write(fdw,req,sizeof(req))!=sizeof(req)) goto _done_;
  }

  printf("Creating uinput device...\n");
  if (wiimote_uinput_init(argv[1])<0) goto _done_;

  printf("Daemonizing...\n");
  daemonize();

  /* Main loop. */
  while (!wiimote_sigc) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fdr,&fds);
    if (select(FD_SETSIZE,&fds,0,0,0)<0) {
      if (errno!=EINTR) fprintf(stderr,"%s: %m\n",argv[1]);
      goto _done_;
    }
    if (!FD_ISSET(fdr,&fds)) { usleep(1000); continue; }
    unsigned char buf[4]={0};
    int bufc=read(fdr,buf,sizeof(buf));
    if (bufc<0) {
      if (errno!=EINTR) fprintf(stderr,"%s: %m\n",argv[1]);
      goto _done_;
    }
    if (bufc!=sizeof(buf)) break; // eg 0 if the connection broke
    if ((buf[0]!=0xa1)||(buf[1]!=0x30)) {
      fprintf(stderr,"%s: unexpected report: %02x %02x %02x %02x\n",argv[1],buf[0],buf[1],buf[2],buf[3]);
      continue; // Nintendo never hits here, but Rock Candy does, with (a1,22) ACK. Whatever, carry on.
    }
    //printf("REPORT: %02x %02x %02x %02x\n",buf[0],buf[1],buf[2],buf[3]);
    rpt=(buf[2]<<8)|buf[3];
    if (rpt==pvrpt) continue;
    if (wiimote_rcvrpt(rpt,pvrpt)<0) goto _done_;
    if (wiimote_synchronize_output()<0) goto _done_;
    pvrpt=rpt;
  }
  
 _done_:
  printf("Terminating\n");
  if (fdr>=0) close(fdr);
  if (fdw>=0) close(fdw);
  wiimote_uinput_quit();
  return 0;
}
