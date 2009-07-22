server: server.c
	gcc -I/usr/local/include -L/usr/local/lib server.c -levent -o server -g -O0
