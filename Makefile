server: server.c static.c info.c during.c after.c
	gcc -I/usr/local/include -L/usr/local/lib server.c static.c info.c during.c after.c -levent -lexpat -o $@ -g -O0
