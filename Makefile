# magic place

CC = gcc

EXECUTABLE = file_server

OBJ_PATH=objs/
INC_PATH=include/
SRC_PATH=src/

TMP_DEPEND=$(EXECUTABLE)_depend

OBJS = $(OBJ_PATH)file_list.o \
       $(OBJ_PATH)globals.o \
       $(OBJ_PATH)handle_request.o \
       $(OBJ_PATH)helper.o \
       $(OBJ_PATH)main.o \
       $(OBJ_PATH)msg.o \
       $(OBJ_PATH)http.o \
       $(OBJ_PATH)handle_post.o \
       $(OBJ_PATH)handle_get.o

GPROF_FILE = performance.txt

# posix threads
LFLAGS = -lpthread

# default flags
CFLAGS = -I$(INC_PATH) \
         -Wall \
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
OBJS   += $(OBJ_PATH)ncurses_msg.o
LFLAGS += -lcurses

# CFLAGS += -std=c99
# CFLAGS += -pedantic
# CFLAGS += -Wredundant-decls
# CFLAGS += -Werror
# CFLAGS += -Wpadded
CFLAGS += -fPIC
CFLAGS += -ggdb
CFLAGS += -pg
#CFLAGS += -O3
CFLAGS += -D_FILE_OFFSET_BITS=64

.PHONY : all
all : depend $(EXECUTABLE)

.PHONY : $(EXECUTABLE)
$(EXECUTABLE) : $(OBJS)
	$(CC) -o $@ $(OBJS) $(LFLAGS)

-include .depend

.PHONY : depend
depend:
	mkdir -p $(OBJ_PATH)
	$(CC) -E -MM $(SRC_PATH)*.c -I$(INC_PATH) > $(TMP_DEPEND)
	sed 's|\(.*\):|$(OBJ_PATH)\1:|' $(TMP_DEPEND)> .depend
	rm $(TMP_DEPEND)

$(OBJ_PATH)%.o: $(SRC_PATH)%.c
	gcc -c $(CFLAGS) $(SRC_PATH)$*.c -o $(OBJ_PATH)$*.o

.PHONY : clean
clean :
	rm $(OBJS) $(EXECUTABLE) .depend

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

install_local : $(EXECUTABLE)
	cp $(EXECUTABLE) ~/bin/
