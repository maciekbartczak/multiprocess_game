CFLAGS = -g -Wall -Wextra -pedantic
GAME_SRC = ./game_src
LINK_FLAGS = -lpthread -lncurses -lrt

all: server player

server: $(GAME_SRC)/server.c $(GAME_SRC)/server.h
	gcc $(CFLAGS) $< -o $@ $(LINK_FLAGS)
player: $(GAME_SRC)/player.c $(GAME_SRC)/player.h
	gcc $(CFLAGS) $< -o $@ $(LINK_FLAGS)
