all:
	g++ np_simple.cpp -o np_simple
	g++ np_single_proc.cpp -o np_single_proc
	g++ np_multi_proc.cpp -o np_multi_proc -lrt
cmds:
	cp /bin/ls bin/
	cp /bin/cat bin/
	g++ commands/noop.cpp -o bin/noop
	g++ commands/number.cpp -o bin/number
	g++ commands/removetag.cpp -o bin/removetag
	g++ commands/removetag0.cpp -o bin/removetag0
1:
	g++ np_simple.cpp -o np_simple
2:
	g++ np_single_proc.cpp -o np_single_proc
3:
	g++ np_multi_proc.cpp -o np_multi_proc -lrt
clean:
	rm -rf bin
	rm -f np_simple
	rm -f np_single_proc
	rm -f np_multi_proc