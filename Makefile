server: server.c static.c info.c
	gcc -I/usr/local/include -L/usr/local/lib server.c static.c info.c -levent -lexpat -o $@ -g -O0
