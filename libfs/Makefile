# Target library
lib := libfs.a
CC := gcc
CFLAGS := -Wextra

#I need to add this later 
# CFLAGS += -Wall -Werror

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

%.o: %.c	
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(lib) $(objs)
