all:
.SILENT:
PRECMD=echo "  $(@F)" ; mkdir -p "$(@D)" ;

CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit
LD:=gcc
LDPOST:=

CFILES:=$(wildcard src/*.c)
OFILES:=$(patsubst src/%.c,mid/%.o,$(CFILES))
-include $(OFILES:.o=.d)

mid/%.o:src/%.c;$(PRECMD) $(CC) -o $@ $<

all:wiimote
wiimote:$(OFILES);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)

clean:;rm -rf mid wiimote

test:wiimote;./wiimote lamb

#------------------------------------------------------------------------------

SUDO:=sudo

# If you change an installation target, be sure to 'make uninstall' first.
APPINSTALL:=/usr/local/bin/wiimote

install:wiimote;\
  if [ -e "$(APPINSTALL)" ] ; then \
    echo "Previous installation detected. Please 'make uninstall' first." ; \
    exit 1 ; \
  fi ; \
  $(SUDO) cp wiimote "$(APPINSTALL)" || exit 1 ; \
  $(SUDO) mkdir -p /etc/wiimote || exit 1 ; \
  $(SUDO) cp src/devices /etc/wiimote/devices || exit 1 ; \
  echo "Installed successfully. Run 'wiimote DEVICE', and press 1+2 on the wiimote to connect..."

uninstall:;\
  $(SUDO) rm -f "$(APPINSTALL)" || exit 1 ; \
  if [ -f /etc/wiimote/devices ] ; then \
    echo "We are deleting your /etc/wiimote/devices. Dumping its contents, in case you forgot to back it up..." ; \
    echo "#-----------------------------------" ; \
    cat /etc/wiimote/devices ; \
    echo "#-----------------------------------" ; \
  fi ; \
  $(SUDO) rm -rf /etc/wiimote || exit 1
