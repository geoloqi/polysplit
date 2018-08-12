CFLAGS := -O3 -Wall -Wextra $(shell gdal-config --cflags)
# CFLAGS += -ggdb
LIBS := $(shell gdal-config --libs)

all: polysplit

polysplit: polysplit.cpp
	g++ -o polysplit $(CFLAGS) polysplit.cpp $(LIBS)

clean:
	rm -rf polysplit polysplit.dSYM/
