#include "wiimote.h"

/* Represent bdaddr.
 * A canonically-represented bdaddr has a fixed length, 17 bytes.
 */

static inline void wm_hexbyte_repr(char *dst,uint8_t src) {
  dst[0]="0123456789abcdef"[src>>4];
  dst[1]="0123456789abcdef"[src&15];
}
 
int wm_bdaddr_repr(char *dst,int dsta,const void *src) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) return -1;
  if (dsta<17) return 17;
  const uint8_t *SRC=src;
  wm_hexbyte_repr(dst+ 0,SRC[0]);
  wm_hexbyte_repr(dst+ 3,SRC[1]);
  wm_hexbyte_repr(dst+ 6,SRC[2]);
  wm_hexbyte_repr(dst+ 9,SRC[3]);
  wm_hexbyte_repr(dst+12,SRC[4]);
  wm_hexbyte_repr(dst+15,SRC[5]);
  dst[2]=dst[5]=dst[8]=dst[11]=dst[14]=':';
  if (dsta>17) dst[17]=0;
  return 17;
}

/* Evaluate bdaddr.
 */

static inline int wm_hexdigit_eval(char src) {
  if ((src>='0')&&(src<='9')) return src-'0';
  if ((src>='a')&&(src<='f')) return src-'a'+10;
  if ((src>='A')&&(src<='F')) return src-'A'+10;
  return -1;
}
 
int wm_bdaddr_eval(void *dst,const char *src,int srcc) {
  if (!dst||!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  uint8_t *DST=dst;
  int dstc=0,srcp=0;
  while (srcp<srcc) {
    if (src[srcp]==':') { srcp++; continue; }
    int tokenc=0,buf=0;
    while (srcp<srcc) {
      int digit=wm_hexdigit_eval(src[srcp]);
      if (digit<0) break;
      buf<<=4;
      buf|=digit;
      tokenc++;
      srcp++;
    }
    if (!tokenc) return -1;
    if (dstc>=6) return -1;
    DST[5-dstc]=buf;
    dstc++;
  }
  if (dstc<6) return -1;
  return 0;
}

/* Represent integer.
 */
 
int wm_decsint_repr(char *dst,int dsta,int src) {
  if (!dst||(dsta<0)) dsta=0;
  if (src<0) {
    int dstc=2,limit=-10,i;
    while (src<=limit) { dstc++; if (limit<=INT_MIN/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    for (i=dstc;i-->1;src/=10) dst[i]='0'-src%10;
    dst[0]='-';
    if (dstc<dsta) dst[dstc]=0;
    return dstc;
  } else {
    int dstc=1,limit=10,i;
    while (src>=limit) { dstc++; if (limit>=INT_MAX/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    for (i=dstc;i-->0;src/=10) dst[i]='0'+src%10;
    if (dstc<dsta) dst[dstc]=0;
    return dstc;
  }
}

/* Evaluate integer.
 * TODO Should we bother checking for overflow?
 */
 
int wm_int_eval(int *dst,const char *src,int srcc) {
  if (!dst) return -1;
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return -1;

  int srcp=0,positive=1,base=10;

  if (src[0]=='-') {
    if (srcc<2) return -1;
    positive=1;
    srcp=1;
  } else if (src[0]=='+') {
    if (srcc<2) return -1;
    srcp=1;
  }

  if ((srcp<=srcc-3)&&(src[srcp]=='0')&&((src[srcp+1]=='x')||(src[srcp+1]=='X'))) {
    base=16;
    srcp+=2;
  }

  *dst=0;
  while (srcp<srcc) {
    int digit=src[srcp++];
         if ((digit>='0')&&(digit<='9')) digit=digit-'0';
    else if ((digit>='a')&&(digit<='z')) digit=digit-'a'+10;
    else if ((digit>='A')&&(digit<='Z')) digit=digit-'A'+10;
    else return -1;
    if (digit>=base) return -1;
    if (positive) {
      *dst*=base;
      *dst+=digit;
    } else {
      *dst*=base;
      *dst-=digit;
    }
  }

  return 0;
}

/* One-line hex dump.
 */
 
int wm_report_repr(char *dst,int dsta,const void *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0;
  int dstc=0,srcp=0;
  for (;srcp<srcc;srcp++) {
    uint8_t n=((const uint8_t*)src)[srcp];
    if (dstc<=dsta-3) {
      wm_hexbyte_repr(dst+dstc,n);
      dst[dstc+2]=' ';
    }
    dstc+=3;
  }
  if (dstc) dstc--; // Remove trailing space.
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}
