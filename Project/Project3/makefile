CXX=g++
CXXFLAGS=-std=c++14 -Wall -pedantic -pthread -lboost_system
CXX_INCLUDE_DIRS=/usr/local/include
CXX_INCLUDE_PARAMS=$(addprefix -I , $(CXX_INCLUDE_DIRS))
CXX_LIB_DIRS=/usr/local/lib
CXX_LIB_PARAMS=$(addprefix -L , $(CXX_LIB_DIRS))

cmds:
	cp /bin/ls bin/
	cp /bin/cat bin/
	g++ extra_files/command/delayedremovetag.cpp -o bin/delayedremovetag
	g++ extra_files/command/noop.cpp -o bin/noop
	g++ extra_files/command/number.cpp -o bin/number
	g++ extra_files/command/removetag.cpp -o bin/removetag
	g++ extra_files/command/removetag0.cpp -o bin/removetag0

http:
	$(CXX) http_server.cpp -o http_server $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)
console:
	$(CXX) console.cpp -o console.cgi $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)

part1:
	$(CXX) http_server.cpp -o http_server $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)
	$(CXX) console.cpp -o console.cgi $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)

part2:
	$(CXX) cgi_server.cpp -o cgi_server -lws2_32 -lwsock32 -std=c++14

clean:
	rm -f http_server console.cgi cgi_server