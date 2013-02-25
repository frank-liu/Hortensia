INC += ./
LIB += ./

CFLAGS += -Wall -c -g -DDEBUG
LFLAGS += -Wall -Bdynamic

OBJS = main.o util.o event.o 

switCH: $(OBJS)
	gcc $(LFLAGS) -L$(LIB) -lm -o $(@) $(OBJS)

%.o: %.c
	gcc $(CFLAGS) -I$(INC) -o $(@) $(<)

clean:
	rm *.o
	rm wifi
