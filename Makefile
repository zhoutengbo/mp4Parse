SRC=main.cpp mp4Parse.cpp  IOContext.cpp common.cpp Codec.cpp Stream.cpp
OBJ=$(SRC:%.cpp=%.o)

all:$(OBJ)
	g++ -g -o main $(OBJ)

%.o:%.cpp
	g++ -g -o $@ -c $<

clean:
	rm -rf main
	rm -rf $(OBJ)

