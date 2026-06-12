CC     = gcc
CFLAGS = -Wall -g

ssi: ssi.c
	$(CC) $(CFLAGS) -o ssi ssi.c -lreadline -lhistory -ltermcap

clean:
	rm -f ssi *.o
