SRC = $(wildcard ./*.c)
OBJ = $(patsubst %.c, %.o, $(SRC))
BIN = mid_server
LIB = -lzlog -lconfig -lpthread -L../../common_lib -lmyList -lcommon -llog -lqueue
LFLAG = -g -Wall

$(BIN):$(OBJ)
	gcc -o $@ $^ $(LIB) $(LFLAG)

./%.o:./%.c
	gcc -c $< -o $@ $(LFLAG)

.PHONY:clean
clean:
	rm -rf $(BIN) $(OBJ)


