#include "wiimote.h"
#include "wm_config.h"
#include "wm_text.h"
#include "wm_fs.h"

/* Object definition.
 */

struct wm_config_alias {
  char *name;
  int namec;
  uint8_t bdaddr[6];
};

struct wm_config {
  struct wm_config_alias *aliasv;
  int aliasc,aliasa;
  char *uinput_path;
  int uinput_pathc;
  int retry_count;
  int daemonize;
  int nunchuk_separate;
  int classic_separate;
  int verbosity;
  char *device_name;
  int device_namec;
};

/* Object lifecycle.
 */
 
struct wm_config *wm_config_new() {
  struct wm_config *config=calloc(1,sizeof(struct wm_config));
  if (!config) return 0;

  /* The default configuration. */
  if (
    (wm_config_set_uinput_path(config,"/dev/uinput",-1)<0)||
    (wm_config_set_retry_count(config,1)<0)||
    (wm_config_set_daemonize(config,1)<0)||
    (wm_config_set_nunchuk_separate(config,0)<0)||
    (wm_config_set_classic_separate(config,1)<0)||
    (wm_config_set_verbosity(config,3)<0)||
  0) {
    wm_config_del(config);
    return 0;
  }

  return config;
}

void wm_config_del(struct wm_config *config) {
  if (!config) return;

  if (config->aliasv) {
    while (config->aliasc-->0) {
      if (config->aliasv[config->aliasc].name) {
        free(config->aliasv[config->aliasc].name);
      }
    }
    free(config->aliasv);
  }

  if (config->uinput_path) free(config->uinput_path);
  if (config->device_name) free(config->device_name);

  free(config);
}

/* Set item by name.
 */
 
int wm_config_set(struct wm_config *config,const char *k,int kc,const char *v,int vc) {
  if (!config) return -1;
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  #define STRFLD(tag,name) if ((kc==sizeof(name)-1)&&!memcmp(k,name,kc)) { \
    if (wm_config_set_##tag(config,v,vc)<0) return -1; \
    return 0; \
  }
  #define INTFLD(tag,name) if ((kc==sizeof(name)-1)&&!memcmp(k,name,kc)) { \
    int vn; \
    if (wm_int_eval(&vn,v,vc)<0) { \
      wm_log_error("Failed to evaluate '%.*s' as integer for '%.*s'.",vc,v,kc,k); \
      return -1; \
    } \
    if (wm_config_set_##tag(config,vn)<0) return -1; \
    return 0; \
  }

  STRFLD(uinput_path,"uinput-path")
  INTFLD(daemonize,"daemonize")
  INTFLD(retry_count,"retry-count")
  INTFLD(nunchuk_separate,"nunchuk-separate")
  INTFLD(classic_separate,"classic-separate")
  INTFLD(verbosity,"verbosity")
  STRFLD(device_name,"device-name")

  #undef STRFLD
  #undef INTFLD

  if ((kc>=7)&&!memcmp(k,"device.",7)) {
    if (wm_config_add_device_alias(config,k+7,kc-7,v,vc)<0) return -1;
    return 0;
  }
  
  return -1;
}

/* Decode entire configuration.
 */

int wm_config_decode(struct wm_config *config,const char *src,int srcc) {
  if (!config) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }

  int srcp=0,lineno=1;
  while (srcp<srcc) {
    const char *k=src+srcp,*v=0;
    int kc=0,vc=0,cmt=0;
    while (srcp<srcc) {
      if (src[srcp]==0x0a) {
        srcp++;
        break;
      } else if (cmt) ;
      else if (src[srcp]=='#') cmt=1;
      else if (v) vc++;
      else if (src[srcp]=='=') v=src+srcp+1;
      else kc++;
      srcp++;
    }
    while (kc&&((unsigned char)k[kc-1]<=0x20)) kc--;
    while (kc&&((unsigned char)k[0]<=0x20)) { kc--; k++; }
    while (vc&&((unsigned char)v[vc-1]<=0x20)) vc--;
    while (vc&&((unsigned char)v[0]<=0x20)) { vc--; v++; }
    if (kc||vc) {
      if (wm_config_set(config,k,kc,v,vc)<0) {
        wm_log_error("%d: Failed to set '%.*s' to '%.*s'",lineno,kc,k,vc,v);
        return -1;
      }
    }
    lineno++;
  }
  
  return 0;
}

/* Decode configuration from file.
 */
 
int wm_config_read_file(struct wm_config *config,const char *path) {
  if (!config||!path) return -1;
  char *src=0;
  int srcc=wm_file_read(&src,path,0);
  if (srcc<0) return -1;
  if (!src) {
    wm_log_warning("%s: File not found. Proceeding with default configuration.",path);
    return 0;
  }
  int err=wm_config_decode(config,src,srcc);
  free(src);
  return err;
}

/* Public configuration item accessors.
 */

int wm_config_set_uinput_path(struct wm_config *config,const char *src,int srcc) {
  if (!config) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc<1)||(srcc>=1024)) {
    wm_log_error("Invalid length %d for uinput_path. (1..1023)",srcc);
    return -1;
  }
  char *nv=malloc(srcc+1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  nv[srcc]=0;
  if (config->uinput_path) free(config->uinput_path);
  config->uinput_path=nv;
  config->uinput_pathc=srcc;
  return 0;
}
    
int wm_config_get_uinput_path(void *dstpp,const struct wm_config *config) {
  if (!config) return -1;
  if (dstpp) *(void**)dstpp=config->uinput_path;
  return config->uinput_pathc;
}

int wm_config_set_daemonize(struct wm_config *config,int daemonize) {
  if (!config) return -1;
  config->daemonize=daemonize?1:0;
  return 0;
}

int wm_config_get_daemonize(const struct wm_config *config) {
  if (!config) return 0;
  return config->daemonize;
}

int wm_config_set_retry_count(struct wm_config *config,int retry_count) {
  if (!config) return -1;
  if (retry_count<1) {
    wm_log_error("Invalid retry count %d",retry_count);
    return -1;
  }
  config->retry_count=retry_count;
  return 0;
}

int wm_config_get_retry_count(const struct wm_config *config) {
  if (!config) return -1;
  return config->retry_count;
}

int wm_config_set_nunchuk_separate(struct wm_config *config,int nunchuk_separate) {
  if (!config) return -1;
  config->nunchuk_separate=nunchuk_separate?1:0;
  return 0;
}

int wm_config_get_nunchuk_separate(const struct wm_config *config) {
  if (!config) return 0;
  return config->nunchuk_separate;
}

int wm_config_set_classic_separate(struct wm_config *config,int classic_separate) {
  if (!config) return -1;
  config->classic_separate=classic_separate;
  return 0;
}

int wm_config_get_classic_separate(const struct wm_config *config) {
  if (!config) return 0;
  return config->classic_separate;
}

int wm_config_set_verbosity(struct wm_config *config,int verbosity) {
  if (!config) return -1;
  if (verbosity<0) verbosity=0;
  else if (verbosity>5) verbosity=5;
  config->verbosity=verbosity;
  return 0;
}

int wm_config_get_verbosity(const struct wm_config *config) {
  if (!config) return 5;
  return config->verbosity;
}

int wm_config_set_device_name(struct wm_config *config,const char *src,int srcc) {
  if (!config) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc<1)||(srcc>=256)) {
    wm_log_error("Invalid length %d for device_name. (1..1023)",srcc);
    return -1;
  }
  char *nv=malloc(srcc+1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  nv[srcc]=0;
  if (config->device_name) free(config->device_name);
  config->device_name=nv;
  config->device_namec=srcc;
  return 0;
}

const char *wm_config_get_device_name(const struct wm_config *config) {
  if (!config) return 0;
  return config->device_name;
}

/* Add alias.
 */
 
int wm_config_add_device_alias(struct wm_config *config,const char *k,int kc,const char *v,int vc) {
  if (!config) return -1;
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  char bdaddr[6];
  if (wm_bdaddr_eval(bdaddr,v,vc)<0) {
    wm_log_error("'%.*s' is not a valid Bluetooth device address. Example: '11:22:33:44:55:66'",vc,v);
    return -1;
  }

  char existing[6]={0};
  if (wm_config_get_device_by_name(existing,config,k,kc)>=0) {
    if (memcmp(bdaddr,existing,6)) {
      wm_log_error("Alias '%.*s' redefined.",kc,k);
      return -1;
    } else {
      wm_log_warning("Alias '%.*s' defined more than once to the same bdaddr.",kc,k);
      return 0;
    }
  }

  if (wm_config_append_alias(config,k,kc,bdaddr)<0) {
    return -1;
  }
  return 0;
}

/* Sequential access to aliases.
 */

int wm_config_count_devices(const struct wm_config *config) {
  if (!config) return 0;
  return config->aliasc;
}

int wm_config_get_device_by_index(void *namepp,void *bdaddr,const struct wm_config *config,int p) {
  if (!config) return -1;
  if ((p<0)||(p>=config->aliasc)) return -1;
  if (namepp) *(void**)namepp=config->aliasv[p].name;
  if (bdaddr) memcpy(bdaddr,config->aliasv[p].bdaddr,6);
  return config->aliasv[p].namec;
}

/* Alias lookup.
 */

int wm_config_get_device_by_name(void *bdaddr,const struct wm_config *config,const char *name,int namec) {
  if (!config) return -1;
  if (!name) namec=0; else if (namec<0) { namec=0; while (name[namec]) namec++; }

  const struct wm_config_alias *alias=config->aliasv;
  int i=config->aliasc; for (;i-->0;alias++) {
    if (alias->namec!=namec) continue;
    if (memcmp(alias->name,name,namec)) continue;
    if (bdaddr) memcpy(bdaddr,alias->bdaddr,6);
    return 0;
  }

  return wm_bdaddr_eval(bdaddr,name,namec);
}

int wm_config_get_device_by_bdaddr(void *namepp,const struct wm_config *config,const void *bdaddr) {
  if (!config) return -1;
  if (!bdaddr) return -1;
  const struct wm_config_alias *alias=config->aliasv;
  int i=config->aliasc; for (;i-->0;alias++) {
    if (memcmp(alias->bdaddr,bdaddr,6)) continue;
    if (namepp) *(void**)namepp=alias->name;
    return alias->namec;
  }
  return -1;
}

/* Add alias to list.
 */
 
int wm_config_append_alias(struct wm_config *config,const char *k,int kc,const void *bdaddr) {
  if (!config||!bdaddr) return -1;
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (kc>255) return -1;

  if (config->aliasc>=config->aliasa) {
    int na=config->aliasa+8;
    if (na>INT_MAX/sizeof(struct wm_config_alias)) return -1;
    void *nv=realloc(config->aliasv,sizeof(struct wm_config_alias)*na);
    if (!nv) return -1;
    config->aliasv=nv;
    config->aliasa=na;
  }

  struct wm_config_alias *alias=config->aliasv+config->aliasc;
  memset(alias,0,sizeof(struct wm_config_alias));
  if (!(alias->name=malloc(kc+1))) return -1;
  memcpy(alias->name,k,kc);
  alias->name[kc]=0;
  alias->namec=kc;
  memcpy(alias->bdaddr,bdaddr,6);
  config->aliasc++;

  return 0;
}
