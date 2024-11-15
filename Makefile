all: server user

server:
	cd src/server; make all

user:
	cd src/user; make all

clean:
	cd src/server; make clean
	cd src/user; make clean

.PHONY: server user all clean