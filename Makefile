# magic place

CC = gcc

OBJS = file_list.o handle_request.o helper.o main.o messages.o types.o

EXECUTABLE = file_server

GPROF_FILE = performance.txt

CFLAGS = -Wall \
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
         -O3
       # -Werror \
       # -pg \
       # -g3 \
       # -g \
       # -Wpadded
       # -Wdeclaration-after-statement

LFLAGS = -lpthread

OBJS   += ncurses_messages.o
CFLAGS += -DNCURSES
LFLAGS += -lcurses

.PHONY : all
all : depend $(EXECUTABLE)

.PHONY : $(EXECUTABLE)
$(EXECUTABLE) : $(OBJS)
	$(CC) -o $@ $(OBJS) $(LFLAGS)

-include .depend

.PHONY : depend
depend:
	$(CC) -E -MM *.c > .depend

.PHONY : clean
clean :
	rm $(OBJS) $(EXECUTABLE)

# targets to run
run_$(EXECUTABLE) : $(EXECUTABLE)
	./$<

perf : clean $(EXECUTABLE)
	gprof $(EXECUTABLE) gmon.out > $(GPROF_FILE)
	$(EDITOR) $(GPROF_FILE)
