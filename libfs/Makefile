# Target library
lib := libfs.a
CC := gcc
CLFAGS := -g -WALL -Werror -Wextra
objs := disk.o fs.o 

all: $(lib)

## TODO: Phase 1

## Debug flag
ifneq ($(D),1)
CFLAGS	+= -O2
else
CFLAGS	+= -g
endif

$(lib): $(objs)
		ar rcs $@ $^

%.o: %.C	
		$(CC) $(CFLAGS) -c -o $@ $<

clean:
		rm -rf $(lib) $(objs)
