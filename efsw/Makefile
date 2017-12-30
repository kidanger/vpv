CPPFLAGS = -Iinclude -Isrc

SRC      = $(shell ls src/efsw/*.cpp src/efsw/platform/posix/*.cpp)
OBJ      = $(SRC:.cpp=.o)
LIB      = efsw.a

$(LIB)   : $(OBJ) ; $(AR) qc $(LIB) $(OBJ)
clean    :        ; $(RM) $(LIB) $(OBJ)
