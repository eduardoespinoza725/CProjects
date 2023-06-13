CC = gcc
CFLAGS = -O0 -ggdb -std=gnu11 -Wall -Wextra -D_XOPEN_SOURCE=700
LIBS = -lrt

all: clean trans

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

trans: trans.o
	$(CC) -o $@ $< $(LIBS)

clean:
	rm -f *.o *~ \#* trans
	@echo "@ Check the following link for -D_XOPEN_SOURCE=700"
	@echo "@ http://stackoverflow.com/questions/5378778/what-does-d-xopen-source-do-mean"






CC = gcc
CFLAGS = -O0 -ggdb -std=gnu11 -Wall -Wextra -D_XOPEN_SOURCE=700
LIBS = -lrt

all: clean trans trans_1.2

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

trans: trans.c
	$(CC) -ggdb -O0 -std=gnu11 -o $@ $<

clean:
	rm -f *.o *~ \#* trans trans_1.2
	@echo "@ Check the following link for -D_XOPEN_SOURCE=700"
	@echo "@ http://stackoverflow.com/questions/5378778/what-does-d-xopen-source-do-mean"
