
# All Target
all:
	g++ -std=c++11 -O3 -g -c -o  ParameterServer.o  ParameterServer.cc
	g++ -std=c++11 -O3 -g -c -o ../common/dma_common.o ../common/dma_common.cc
	g++ -std=c++11 -O3 -g -c -o ../common/HashTable.o ../common/HashTable.cc
	g++ -std=c++11 -O3 -g -o app  ParameterServer.o ../common/HashTable.o ../common/dma_common.o -lpthread -libverbs


# Clean Target
clean:
	rm *.o
	rm app
