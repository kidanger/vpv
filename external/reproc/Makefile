CPPFLAGS = -Ireproc/include -Ireproc++/include

SRC      = $(shell ls reproc/src/*.c | grep -v windows)
OBJ      = $(SRC:.c=.o) reproc++/src/reproc.o
LIB      = reproc.a

$(LIB)   : $(OBJ) ; $(AR) qc $(LIB) $(OBJ)
clean    :        ; $(RM) $(LIB) $(OBJ)
