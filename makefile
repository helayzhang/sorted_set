C=gcc
CC=g++
LIBS=
INCLUDES=
CSRCS=*.c
CCSRCS=*.cc
BIN=example.exe
COBJS=$(patsubst %.c, %.o, $(wildcard $(CSRCS)))
CCOBJS=$(patsubst %.cc, %.o, $(wildcard $(CCSRCS)))

$(BIN):	$(COBJS) $(CCOBJS)
	$(CC) -o $@ $(LIBS) $^

%.o:%.cc
	$(CC) -g -std=c++0x -c $< -o $@ $(INCLUDES)

%.o:%.c
	$(C) -g -c $< -o $@ $(INCLUDES)

clean:
	rm $(BIN) $(COBJS) $(CCOBJS)
