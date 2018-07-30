SRC = $(wildcard ./*.c)
OBJ = $(patsubst %.c, %.o, $(SRC))
BIN = cloud_client
LIB = -lconfig -lpthread -L../../common_lib -lcondition -lcommon -lqueue
CFLAG = -g -Wall


$(BIN):$(OBJ)
	gcc -o $@ $^ $(LIB) $(CFLAG)

./%.o:./%.c
	gcc -c $< -o $@ $(CFLAG)

.PHONY:clean
clean:
	rm -rf $(BIN) $(OBJ)


