all:
.SILENT:
PRECMD=echo "  $(@F)" ; mkdir -p $(@D) ;

CC:=gcc -c -MMD -O2 -Isrc -Werror -Wimplicit
LD:=gcc
LDPOST:=

CFILES:=$(wildcard src/*.c)
OFILES:=$(patsubst src/%.c,mid/%.o,$(CFILES))
-include $(OFILES:.o=.d)

mid/%.o:src/%.c;$(PRECMD) $(CC) -o $@ $<

EXE:=out/wiimote
all:$(EXE)
$(EXE):$(OFILES);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)

clean:;rm -rf mid out
#run:$(EXE);$(EXE) --verbosity=5 --no-daemonize 00:17:ab:39:67:fd # Banana
run:$(EXE);$(EXE) --verbosity=4 --no-daemonize --nunchuk-separate 00:1e:35:72:07:cf # Lamb
