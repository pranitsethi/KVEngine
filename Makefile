CXX = g++
CXXFLAGS = -std=c++0x -O0 -I.
DEPS = KV_Engine.h
OBJ =  KV_Engine.o 

PATH += hiredis

LIBS= libhiredis.a 

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

Engine: $(OBJ)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

clean:
	rm -f *.o Engine core
