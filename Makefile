# magic place

CC = gcc

OBJECT_FILES = content_encoding.o file_list.o handle_request.o helper.o

EXECUTABLE = main

GPROF_FILE = performance.txt

CFLAGS = -Wall \
         -O2 \
         -Wstrict-prototypes \
         -Wmissing-prototypes \
         -Wno-main \
         -Wno-uninitialized \
         -Wbad-function-cast \
         -Wcast-align \
         -Wcast-qual \
         -Wextra \
         -Wmissing-declarations \
         -Wpointer-arith \
         -Wshadow \
         -Wsign-compare \
         -Wuninitialized \
         -Wunused \
         -Wno-unused-parameter \
         -Wnested-externs \
         -Wunreachable-code \
         -Winline \
         -Wdisabled-optimization \
         -Wconversion \
         -Wfloat-equal \
         -Wswitch \
         -Wswitch-default \
         -Wtrigraphs \
         -Wsequence-point \
         -Wimplicit \
         -Wredundant-decls \
         -Wstack-protector \
         -Woverlength-strings \
         -Waddress \
         -ggdb \
         -pg \
         -g3 \
       # -Wpadded
       # -Wdeclaration-after-statement
       # -Werror

LFLAGS = -lbsd

# targets to compile

%.o : %.c %.h
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXECUTABLE) : main.c $(OBJECT_FILES)
	$(CC) -o $@ $< $(OBJECT_FILES) $(CFLAGS) $(LFLAGS)

clean :
	rm $(OBJECT_FILES) $(EXECUTABLE)

# targets to run
run_$(EXECUTABLE) : $(EXECUTABLE)
	./$<

perf : clean run_$(EXECUTABLE)
	gprof $(EXECUTABLE) gmon.out > $(GPROF_FILE)
	$(EDITOR) $(GPROF_FILE)

# random info

# $@ means target (left of :)
# $< means first depency (first right of :)
# %.c %.o %.h means current target name
