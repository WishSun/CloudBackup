SRC=$(wildcard ./*.c)
OBJ=$(patsubst %.c, %.o, $(SRC))
BIN=./liblog.a

#生成静态库
$(BIN):$(OBJ)
	ar -cr $@ $^    

./%.o:./%.c
	gcc -c $< -o $@

#删除所有无关文件
clean:
	rm -rf $(OBJ) $(BIN)
