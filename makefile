CC = arm-linux-gcc-4.4.3
OBJECTS = rp_main.o mr_common.o rp_timer.o rp_fhr.o

routingp: $(OBJECTS)
	$(CC) -o routingp $(OBJECTS) -lpthread -g

rp_main.o: rp_main.c rp_common.h
	$(CC) -c rp_main.c -g

mr_commom.o: mr_common.c mr_common.h
	$(CC) -c mr_common.c -g

rp_timer.o: rp_timer.c rp_timer.h mr_common.h
	$(CC) -c rp_timer.c -g

rp_fhr.o: rp_fhr.c rp_fhr.h mr_common.h
	$(CC) -c rp_fhr.c -g

clean :
	rm $(OBJECTS)
