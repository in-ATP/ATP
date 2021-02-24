# CFLAGS  := -O3 -g
# LD      := g++
# LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lrt -lpthread  -lm

# ROCE_COMMON_PATH = ../common/
# INCLUDES  = -I${ROCE_COMMON_PATH}
# CFLAGS := ${CFLAGS} ${INCLUDES}
# SOURCES := $(wildcard *.c *.h ${ROCE_COMMON_PATH}*.c ${ROCE_COMMON_PATH}*.h)


# all: app 
# app: main.o  p4ml_manager.o ${ROCE_COMMON_PATH}packet.o ${ROCE_COMMON_PATH}dma_common.o ${ROCE_COMMON_PATH}window_manager.o
# 	${LD} $(CFLAGS) -o $@ $^ ${LDFLAGS}


# # Clean Target
# clean:
# 	rm *.o ../common/*.o
# 	rm app

all:
	g++ -std=c++11 -g -O3 -c -o main.o main.cc
	g++ -std=c++11 -g -O3 -c -o p4ml_manager.o p4ml_manager.cc  -mavx
	g++ -std=c++11 -g -O3 -c -o ../common/HashTable.o ../common/HashTable.cc
	g++ -std=c++11 -g -O3 -c -o ../common/dma_common.o ../common/dma_common.cc
	g++ -std=c++11 -g -O3 -I../common/ -o app main.o p4ml_manager.o ../common/HashTable.o ../common/dma_common.o -lrdmacm -libverbs -lrt -lpthread  -lm 

clean:
	rm *.o
	rm app
