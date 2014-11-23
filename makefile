CC = g++
include_dirs = -I/usr/include/opencv -I/usr/include/opencv2
pkg_config_lib = `pkg-config --cflags opencv --libs opencv` 

IFLAGS = $(include_dirs)
CFLAGS = $(pkg_config_lib)

objects = picSend.o

all : $(objects)

$(objects) : %.o : %.c
	$(CC) $(IFLAGS) $(CFLAGS) $< -o $@


.PHONY : clean
clean :
	rm picSend
