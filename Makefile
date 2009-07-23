server: server.c static.c
	gcc -I/usr/local/include -L/usr/local/lib server.c static.c -levent -o $@ -g -O0
