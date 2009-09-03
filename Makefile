# x86-32
CC = gcc
CFLAGS = -O2 -Wall -DSLJIT_CONFIG_AUTO
LDFLAGS=

# x86-64
#CC = gcc
#CFLAGS = -O2 -Wall -DSLJIT_CONFIG_AUTO
#LDFLAGS=

# ARM
#CC = arm-linux-gcc
#CFLAGS = -O2 -Wall -DSLJIT_CONFIG_AUTO
#LDFLAGS=

TARGET = sljit_test regex_test

BINDIR = bin
SRCDIR = sljit_src
TESTDIR = test_src
REGEXDIR = regex_src

CFLAGS += -Isljit_src

all: $(BINDIR) $(TARGET)

$(BINDIR) :
	mkdir $(BINDIR)

$(BINDIR)/sljitLir.o : $(addprefix $(SRCDIR)/, sljitLir.h sljitNativeX86_common.c sljitNativeARM.c sljitNativeX86_32.c sljitNativeX86_64.c)
	$(CC) $(CFLAGS) -c -o $@ $(SRCDIR)/sljitLir.c

$(BINDIR)/sljitMain.o : $(TESTDIR)/sljitMain.c
	$(CC) $(CFLAGS) -c -o $@ $^

$(BINDIR)/sljitTest.o : $(TESTDIR)/sljitTest.c
	$(CC) $(CFLAGS) -c -o $@ $^

$(BINDIR)/regexMain.o : $(REGEXDIR)/regexMain.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f $(BINDIR)/*.o $(addprefix $(BINDIR)/, $(TARGET))

sljit_test: $(addprefix $(BINDIR)/, sljitMain.o sljitTest.o sljitLir.o)
	$(CC) $(LDFLAGS) $^ -o $(BINDIR)/$@ -lm

regex_test: $(addprefix $(BINDIR)/, regexMain.o sljitLir.o)
	$(CC) $(LDFLAGS) $^ -o $(BINDIR)/$@ -lm
