# magic place

CC = gcc

EXECUTABLE = file_server

OBJS = file_list.o \
       handle_request.o \
       helper.o \
       main.o \
       msg.o \
       http_response.o \
       handle_post.o \
       handle_get.o

GPROF_FILE = performance.txt

# posix threads
LFLAGS = -lpthread

# default flags
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
         -Wstack-protector \
         -Woverlength-strings \
         -Waddress \
         -Wdeclaration-after-statement

# NCURSES
CFLAGS += -DNCURSES
OBJS   += ncurses_msg.o
LFLAGS += -lcurses

# CFLAGS += -std=c99
# CFLAGS += -pedantic
# CFLAGS += -Wredundant-decls
# CFLAGS += -Werror
# CFLAGS += -Wpadded
CFLAGS += -fPIC
CFLAGS += -ggdb
CFLAGS += -pg
CFLAGS += -O3
CFLAGS += -D_FILE_OFFSET_BITS=64

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
	rm $(OBJS) $(EXECUTABLE) .depend

# targets to run
run_$(EXECUTABLE) : $(EXECUTABLE)
	./$<

perf :
	gprof $(EXECUTABLE) gmon.out > $(GPROF_FILE)

graph : perf
	gprof2dot $(GPROF_FILE) -n0 -e0 > graph.dot
	dot -Tsvg graph.dot -o graph.svg
	sfdp -Gsize=100! \
             -Gsplines=true \
             -Goverlap=prism \
             -Tpng graph.dot \
             > graph.png
