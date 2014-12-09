CC = g++
include_dirs = -I/usr/include/opencv -I/usr/include/opencv2
pkg_config_lib = `pkg-config --cflags opencv --libs opencv` 

IFLAGS = $(include_dirs)
LIBS = $(pkg_config_lib)

target = picSend
objects = picSend.o

all: $(objects)
	$(CC) -o $(target)  $(LIBS) -lpthread $(objects)

%.o: %.c 
	$(CC) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(target) *.o
