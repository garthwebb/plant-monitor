include Makefile.inc

LIBS=-l$(LIB)
collector: collector.cpp
        $(CXX) $(CFLAGS) -I$(HEADER_DIR) -L$(LIB_DIR) $@.cpp $(LIBS) -o $@