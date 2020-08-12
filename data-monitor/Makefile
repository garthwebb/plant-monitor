CXX=g++
CFLAGS=
HEADER_DIR=/usr/local/include/RF24
LIB_DIR=/usr/local/lib
LIB=rf24

LIBS=-l$(LIB)
collector: collector.cpp
	$(CXX) $(CFLAGS) -I$(HEADER_DIR) -L$(LIB_DIR) $@.cpp $(LIBS) -o $@
