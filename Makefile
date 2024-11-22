CXX = g++ -std=c++14

export PKG_CONFIG_PATH=/usr/lib/pkgconfig/:/usr/local/share/pkgconfig/

export LD_LIBRARY_PATH=/usr/local/lib

CXXFLAGS = -g -Wall -pedantic -Irdkafka -Iboost #-ansi
LDFLAGS = $(shell pkg-config --cflags glib-2.0 nlohmann_json)
LDLIBS = $(shell pkg-config --libs glib-2.0 rdkafka)

main.o: main.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LDLIBS) main.cpp -o main

clean:
	$(RM) main *~
