all:
	g++ npshell.cpp -o npshell
cmds:
	cp /bin/ls bin/
	cp /bin/cat bin/
	g++ commands/noop.cpp -o bin/noop
	g++ commands/number.cpp -o bin/number
	g++ commands/removetag.cpp -o bin/removetag
	g++ commands/removetag0.cpp -o bin/removetag0
clean:
	rm -f npshell