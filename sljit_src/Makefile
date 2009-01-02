# x86-32
CC = gcc
CFLAGS = -O2 -Wall -DSLJIT_CONFIG_X86_32
LDFLAGS=

# x86-64
#CC = gcc
#CFLAGS = -O2 -Wall -DSLJIT_CONFIG_X86_64
#LDFLAGS=

# ARM
#CC = arm-linux-gcc
#CFLAGS = -O2 -Wall -DSLJIT_CONFIG_ARM
#LDFLAGS=

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
	rm -f $(BINDIR)/*.o $(addprefix $(BINDIR)/, $(TARGET))

sljit_test: $(addprefix $(BINDIR)/, main.o sljitTest.o sljitLir.o)
	$(CC) $(LDFLAGS) $^ -o $(BINDIR)/$@ -lm

