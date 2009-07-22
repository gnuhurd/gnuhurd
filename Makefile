server: server.c
	gcc -I/opt/local/include -L/opt/local/lib server.c -levent -o server
