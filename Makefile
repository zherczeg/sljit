# default compier
CC = gcc

# Cross compiler for ARM
#CC = arm-linux-gcc

# Cross compiler for PPC
#CC = powerpc-linux-gnu-gcc

# Cross compiler for PPC-64
#CC = powerpc64-unknown-linux-gnu-gcc

CFLAGS = -O2 -Wall -DSLJIT_CONFIG_AUTO=1
LDFLAGS=

TARGET = sljit_test regex_test

BINDIR = bin
SRCDIR = sljit_src
TESTDIR = test_src
REGEXDIR = regex_src

CFLAGS += -Isljit_src
REGEX_CFLAGS = -fshort-wchar

all: $(BINDIR) $(TARGET)

$(BINDIR) :
	mkdir $(BINDIR)

$(BINDIR)/sljitLir.o : $(addprefix $(SRCDIR)/, sljitLir.c sljitLir.h sljitConfig.h sljitExecAllocator.c sljitNativeX86_common.c sljitNativeX86_32.c sljitNativeX86_64.c sljitNativeARM_v5.c sljitNativeARM_Thumb2.c sljitNativePPC_common.c sljitNativePPC_32.c sljitNativePPC_64.c sljitNativeMIPS_common.c sljitNativeMIPS_32.c) $(BINDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BINDIR)/sljitMain.o : $(TESTDIR)/sljitMain.c $(BINDIR) $(SRCDIR)/sljitLir.h $(SRCDIR)/sljitConfig.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BINDIR)/sljitTest.o : $(TESTDIR)/sljitTest.c $(BINDIR) $(SRCDIR)/sljitLir.h $(SRCDIR)/sljitConfig.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BINDIR)/regexMain.o : $(REGEXDIR)/regexMain.c $(BINDIR) $(REGEXDIR)/regexJIT.h $(SRCDIR)/sljitConfig.h
	$(CC) $(CFLAGS) $(REGEX_CFLAGS) -c -o $@ $<

$(BINDIR)/regexJIT.o : $(REGEXDIR)/regexJIT.c $(BINDIR) $(SRCDIR)/sljitLir.h $(SRCDIR)/sljitConfig.h $(REGEXDIR)/regexJIT.h
	$(CC) $(CFLAGS) $(REGEX_CFLAGS) -c -o $@ $<

clean:
	rm -f $(BINDIR)/*.o $(addprefix $(BINDIR)/, $(TARGET))

sljit_test: $(addprefix $(BINDIR)/, sljitMain.o sljitTest.o sljitLir.o)
	$(CC) $(LDFLAGS) $^ -o $(BINDIR)/$@ -lm

regex_test: $(addprefix $(BINDIR)/, regexMain.o regexJIT.o sljitLir.o)
	$(CC) $(LDFLAGS) $^ -o $(BINDIR)/$@ -lm
