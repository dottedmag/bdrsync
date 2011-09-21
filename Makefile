CFLAGS=-O2 -g -W -Wall -Werror --std=c99

all: bdrsync

clean:
	-rm bdrsync

README.html: README.md
	markdown $< > $@
