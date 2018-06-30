#include "wiimote.h"
#include "wm_config.h"
#include "wm_coord.h"
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/* Signal handler.
 */

static volatile int wm_sigc=0;

static void wm_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: case SIGTERM: {
        if (++wm_sigc>=3) {
          wm_log_error("Failed to terminate after 3 signals. Aborting hard.");
          exit(1);
        }
      } break;
  }
}

/* --help
 */

static void wm_print_help(const char *exename) {
  printf("Usage: %s [OPTIONS] DEVICE\n",exename);
  printf("DEVICE is a Bluetooth address (eg \"11:22:33:44:55:66\") or a user-defined alias.\n");
  printf("OPTIONS:\n");
  printf("  --help                 Print this message.\n");
  printf("  --version              Print version number.\n");
  printf("  --uinput-path=PATH     Set path to uinput (default \"/dev/uinput\").\n");
  printf("  --no-daemonize         Stay in the foreground.\n");
  printf("  --retry-count=INT      Try so many times to connect (default 1).\n");
  printf("  --verbosity=INT        How much logging, 0=silent..5=noisy (default 3).\n");
  printf("  --nunchuk-separate     Create a separate device for the nunchuk extension.\n");
  printf("  --no-classic-separate  Report base and classic extension as one device.\n");
  printf("Options may be stored in a config file '%s'.\n",WM_CONFIG_FILE_PATH);
  printf("In the config file, omit the leading dashes, and a value is required.\n");
}

static void wm_print_version() {
  printf("wiimote 2.0\n");
  printf("2018-04-08\n");
}

/* Short options.
 */

static int wm_set_short_options(struct wm_config *config,const char *src) {
  for (;*src;src++) switch (*src) {
    default: {
        wm_log_error("Unexpected short option '%c'.",*src);
        return -1;
      }
  }
  return 0;
}

/* Read arguments from command line.
 */

static int wm_set_command_line(struct wm_config *config,int argc,char **argv) {
  int argp=1; while (argp<argc) {
    const char *arg=argv[argp++];

    // Ignore empty argument.
    if (!arg||!arg[0]) {
      continue;
    }

    // No dash, device name.
    if (arg[0]!='-') {
      if (wm_config_get_device_name(config)) {
        wm_log_error("Multiple device names.");
        return -1;
      }
      if (wm_config_set_device_name(config,arg,-1)<0) {
        return -1;
      }
      continue;
    }

    // Dash alone: illegal.
    if (!arg[1]) goto _bad_argument_;

    // Short options.
    if (arg[1]!='-') {
      if (wm_set_short_options(config,arg+1)<0) return -1;
      continue;
    }

    // Double-dash alone: illegal.
    if (!arg[2]) goto _bad_argument_;

    // Long options are "--KEY=VALUE".
    // VALUE is "1" if missing, or "0" if missing and KEY begins "no-".
    const char *k=arg+2,*v=0;
    int kc=0,vc=0;
    while (k[kc]&&(k[kc]!='=')) kc++;
    if (k[kc]=='=') {
      v=k+kc+1;
      while (v[vc]) vc++;
    } else {
      if ((kc>=3)&&!memcmp(k,"no-",3)) {
        k+=3;
        kc-=3;
        v="0";
        vc=1;
      } else {
        v="1";
        vc=1;
      }
    }

    // Pick off special options "--help" and "--version".
    if ((kc==4)&&!memcmp(k,"help",4)) {
      wm_print_help(argv[0]);
      exit(0);
    }
    if ((kc==7)&&!memcmp(k,"version",7)) {
      wm_print_version();
      exit(0);
    }

    // Any other long option goes directly into the config.
    if (wm_config_set(config,k,kc,v,vc)<0) {
      wm_log_error("Failed to set long option '%.*s' = '%.*s'",kc,k,vc,v);
      return -1;
    }

    continue;
   _bad_argument_:
    wm_log_error("Unexpected argument '%s'",arg);
    return -1;
  }

  /* Confirm that we received a device name. */
  if (!wm_config_get_device_name(config)) {
    wm_log_error("Device name required.");
    return -1;
  }
  
  return 0;
}

/* Daemonize.
 */

static int wm_daemonize() {
  if (getppid()==1) return 0;

  int pid=fork();
  if (pid<0) {
    wm_log_error("fork() failed.");
    return -1;
  }
  if (pid>0) {
    wm_log_info("Launched daemon process %d. Terminating foreground.",pid);
    exit(0);
  }

  umask(0);
  int sid=setsid();
  if (sid<0) {
    wm_log_error("setsid() failed.");
    return -1;
  }

  chdir("/");

  int fd=open("/dev/null",O_RDWR);
  if (fd>=0) {
    dup2(fd,STDIN_FILENO);
    dup2(fd,STDOUT_FILENO);
    dup2(fd,STDERR_FILENO);
    close(fd);
  }

  return 0;
}

/* Main entry point.
 */

int main(int argc,char **argv) {

  wm_log_trace("Starting up.");

  signal(SIGINT,wm_rcvsig);
  signal(SIGTERM,wm_rcvsig);

  struct wm_config *config=wm_config_new();
  if (!config) return 1;
  if (wm_config_read_file(config,WM_CONFIG_FILE_PATH)<0) return 1;
  if (wm_set_command_line(config,argc,argv)<0) return 1;
  if (wm_log_configure(config)<0) return -1;

  struct wm_coord *coord=wm_coord_new();
  if (!coord) return 1;
  if (wm_coord_startup(coord,config)<0) {
    wm_log_error("Failed to apply configuration.");
    return 1;
  }

  if (wm_config_get_daemonize(config)) {
    if (wm_daemonize()<0) {
      wm_log_error("Failed to fork daemon process.");
      return 1;
    }
  }

  wm_log_trace("Begin main loop.");
  while (!wm_sigc&&wm_coord_is_running(coord)) {
    if (wm_coord_update(coord)<0) {
      return 1;
    }
  }

  wm_log_trace("Terminating.");
  wm_coord_del(coord);
  wm_config_del(config);
  return 0;
}
