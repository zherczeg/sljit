CC = gcc

# -fshort-wchar is not necessary
# general:
CFLAGS = -O2 -Wall
LDFLAGS=

# ARM
#CC = arm-elf-gcc
#CFLAGS = -O2 -Wall -DTARGET_ARM

TARGET = sljit_test
BINDIR = ../sljit_bin

all: $(TARGET)

$(BINDIR) :
	mkdir $(BINDIR)

BINDIR/sljitLir.o : sljitLir.h sljitNativeX86_32.c
	$(CC) $(CFLAGS) -c -o $@ sljitLir.c

$(addprefix $(BINDIR)/, %.o) : %.c $(BINDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(BINDIR)/*.o $(addprefix $(BINDIR)/, $(TARGETS))

sljit_test: $(addprefix $(BINDIR)/, main.o sljitTest.o sljitLir.o)
	$(CC) $(LDFLAGS) $^ -o $(BINDIR)/$@ -lm

