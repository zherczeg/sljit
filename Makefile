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

SLJIT_HEADERS = $(SRCDIR)/sljitLir.h $(SRCDIR)/sljitConfig.h $(SRCDIR)/sljitConfigInternal.h

SLJIT_LIR_FILES = $(SRCDIR)/sljitLir.c $(SRCDIR)/sljitExecAllocator.c $(SRCDIR)/sljitUtils.c \
	$(SRCDIR)/sljitNativeX86_common.c $(SRCDIR)/sljitNativeX86_32.c $(SRCDIR)/sljitNativeX86_64.c \
	$(SRCDIR)/sljitNativeARM_v5.c $(SRCDIR)/sljitNativeARM_Thumb2.c \
	$(SRCDIR)/sljitNativePPC_common.c $(SRCDIR)/sljitNativePPC_32.c $(SRCDIR)/sljitNativePPC_64.c \
	$(SRCDIR)/sljitNativeMIPS_common.c $(SRCDIR)/sljitNativeMIPS_32.c \
	$(SRCDIR)/sljitNativeSPARC_common.c $(SRCDIR)/sljitNativeSPARC_32.c

all: $(BINDIR) $(TARGET)

$(BINDIR) :
	mkdir $(BINDIR)

$(BINDIR)/sljitLir.o : $(BINDIR) $(SLJIT_LIR_FILES) $(SLJIT_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $(SRCDIR)/sljitLir.c

$(BINDIR)/sljitMain.o : $(TESTDIR)/sljitMain.c $(BINDIR) $(SLJIT_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $(TESTDIR)/sljitMain.c

$(BINDIR)/sljitTest.o : $(TESTDIR)/sljitTest.c $(BINDIR) $(SLJIT_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $(TESTDIR)/sljitTest.c

$(BINDIR)/regexMain.o : $(REGEXDIR)/regexMain.c $(BINDIR) $(SLJIT_HEADERS)
	$(CC) $(CFLAGS) $(REGEX_CFLAGS) -c -o $@ $(REGEXDIR)/regexMain.c

$(BINDIR)/regexJIT.o : $(REGEXDIR)/regexJIT.c $(BINDIR) $(SLJIT_HEADERS) $(REGEXDIR)/regexJIT.h
	$(CC) $(CFLAGS) $(REGEX_CFLAGS) -c -o $@ $(REGEXDIR)/regexJIT.c

clean:
	rm -f $(BINDIR)/*.o $(BINDIR)/sljit_test $(BINDIR)/regex_test

sljit_test: $(BINDIR)/sljitMain.o $(BINDIR)/sljitTest.o $(BINDIR)/sljitLir.o
	$(CC) $(LDFLAGS) $(BINDIR)/sljitMain.o $(BINDIR)/sljitTest.o $(BINDIR)/sljitLir.o -o $(BINDIR)/$@ -lm -lpthread

regex_test: $(BINDIR)/regexMain.o $(BINDIR)/regexJIT.o $(BINDIR)/sljitLir.o
	$(CC) $(LDFLAGS) $(BINDIR)/regexMain.o $(BINDIR)/regexJIT.o $(BINDIR)/sljitLir.o -o $(BINDIR)/$@ -lm -lpthread
