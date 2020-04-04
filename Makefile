CPPFLAGS += -DSLJIT_CONFIG_AUTO=1 -Isljit_src
REGEX_CFLAGS = $(CFLAGS) -fshort-wchar

BINDIR = bin
SRCDIR = sljit_src
TESTDIR = test_src
REGEXDIR = regex_src
EXAMPLEDIR = doc/tutorial

TARGET = $(BINDIR)/sljit_test $(BINDIR)/regex_test
EXAMPLE_TARGET = $(BINDIR)/func_call $(BINDIR)/first_program $(BINDIR)/branch $(BINDIR)/loop $(BINDIR)/array_access $(BINDIR)/func_call $(BINDIR)/struct_access $(BINDIR)/temp_var $(BINDIR)/brainfuck

SLJIT_HEADERS = $(SRCDIR)/sljitLir.h $(SRCDIR)/sljitConfig.h $(SRCDIR)/sljitConfigInternal.h

SLJIT_LIR_FILES = $(SRCDIR)/sljitLir.c $(SRCDIR)/sljitUtils.c \
	$(SRCDIR)/sljitExecAllocator.c $(SRCDIR)/sljitProtExecAllocator.c \
	$(SRCDIR)/sljitNativeARM_32.c $(SRCDIR)/sljitNativeARM_T2_32.c $(SRCDIR)/sljitNativeARM_64.c \
	$(SRCDIR)/sljitNativeMIPS_common.c $(SRCDIR)/sljitNativeMIPS_32.c $(SRCDIR)/sljitNativeMIPS_64.c \
	$(SRCDIR)/sljitNativePPC_common.c $(SRCDIR)/sljitNativePPC_32.c $(SRCDIR)/sljitNativePPC_64.c \
	$(SRCDIR)/sljitNativeSPARC_common.c $(SRCDIR)/sljitNativeSPARC_32.c \
	$(SRCDIR)/sljitNativeTILEGX_64.c \
	$(SRCDIR)/sljitNativeX86_common.c $(SRCDIR)/sljitNativeX86_32.c $(SRCDIR)/sljitNativeX86_64.c

.PHONY: all clean examples

all: $(TARGET)

clean:
	-$(RM) $(BINDIR)/*.o $(BINDIR)/sljit_test $(BINDIR)/regex_test $(EXAMPLE_TARGET)

$(BINDIR)/.keep :
	mkdir -p $(BINDIR)
	@touch $@

$(BINDIR)/sljitLir.o : $(BINDIR)/.keep $(SLJIT_LIR_FILES) $(SLJIT_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $(SRCDIR)/sljitLir.c

$(BINDIR)/sljitMain.o : $(TESTDIR)/sljitMain.c $(BINDIR)/.keep $(SLJIT_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $(TESTDIR)/sljitMain.c

$(BINDIR)/sljitTest.o : $(TESTDIR)/sljitTest.c $(BINDIR)/.keep $(SLJIT_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $(TESTDIR)/sljitTest.c

$(BINDIR)/regexMain.o : $(REGEXDIR)/regexMain.c $(BINDIR)/.keep $(SLJIT_HEADERS)
	$(CC) $(CPPFLAGS) $(REGEX_CFLAGS) -c -o $@ $(REGEXDIR)/regexMain.c

$(BINDIR)/regexJIT.o : $(REGEXDIR)/regexJIT.c $(BINDIR)/.keep $(SLJIT_HEADERS) $(REGEXDIR)/regexJIT.h
	$(CC) $(CPPFLAGS) $(REGEX_CFLAGS) -c -o $@ $(REGEXDIR)/regexJIT.c

$(BINDIR)/sljit_test: $(BINDIR)/.keep $(BINDIR)/sljitMain.o $(BINDIR)/sljitTest.o $(BINDIR)/sljitLir.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(BINDIR)/sljitMain.o $(BINDIR)/sljitTest.o $(BINDIR)/sljitLir.o -o $@ -lm -lpthread

$(BINDIR)/regex_test: $(BINDIR)/.keep $(BINDIR)/regexMain.o $(BINDIR)/regexJIT.o $(BINDIR)/sljitLir.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(BINDIR)/regexMain.o $(BINDIR)/regexJIT.o $(BINDIR)/sljitLir.o -o $@ -lm -lpthread

examples: $(EXAMPLE_TARGET)

$(BINDIR)/first_program: $(EXAMPLEDIR)/first_program.c $(BINDIR)/.keep $(BINDIR)/sljitLir.o
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(EXAMPLEDIR)/first_program.c $(BINDIR)/sljitLir.o -o $@ -lm -lpthread

$(BINDIR)/branch: $(EXAMPLEDIR)/branch.c $(BINDIR)/.keep $(BINDIR)/sljitLir.o
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(EXAMPLEDIR)/branch.c $(BINDIR)/sljitLir.o -o $@ -lm -lpthread

$(BINDIR)/loop: $(EXAMPLEDIR)/loop.c $(BINDIR)/.keep $(BINDIR)/sljitLir.o
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(EXAMPLEDIR)/loop.c $(BINDIR)/sljitLir.o -o $@ -lm -lpthread

$(BINDIR)/array_access: $(EXAMPLEDIR)/array_access.c $(BINDIR)/.keep $(BINDIR)/sljitLir.o
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(EXAMPLEDIR)/array_access.c $(BINDIR)/sljitLir.o -o $@ -lm -lpthread

$(BINDIR)/func_call: $(EXAMPLEDIR)/func_call.c $(BINDIR)/.keep $(BINDIR)/sljitLir.o
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(EXAMPLEDIR)/func_call.c $(BINDIR)/sljitLir.o -o $@ -lm -lpthread

$(BINDIR)/struct_access: $(EXAMPLEDIR)/struct_access.c $(BINDIR)/.keep $(BINDIR)/sljitLir.o
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(EXAMPLEDIR)/struct_access.c $(BINDIR)/sljitLir.o -o $@ -lm -lpthread

$(BINDIR)/temp_var: $(EXAMPLEDIR)/temp_var.c $(BINDIR)/.keep $(BINDIR)/sljitLir.o
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(EXAMPLEDIR)/temp_var.c $(BINDIR)/sljitLir.o -o $@ -lm -lpthread

$(BINDIR)/brainfuck: $(EXAMPLEDIR)/brainfuck.c $(BINDIR)/.keep $(BINDIR)/sljitLir.o
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(EXAMPLEDIR)/brainfuck.c $(BINDIR)/sljitLir.o -o $@ -lm -lpthread
