ROOT=.
PLATFORM=$(shell ./systype.sh)
include $(ROOT)/Make.defines.$(PLATFORM)

PROG ?= example

all: $(PROG)

$(PROG): main.c $(LIBAPUE)
	$(CC) mongoose.c $(CFLAGS) -I. -W -Wcomment -DMG_ENABLE_LINES=1 -o $(PROG) main.c $(LDLIBS)

clean:
	rm -rf $(PROG) $(TEMPFILES) *.o *.dSYM *.gcov *.gcno *.gcda *.obj *.exe *.ilk *.pdb

include $(ROOT)/Make.libapue.inc