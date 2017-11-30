SRC=blit.c cpu.c mem.c telnet.c
blit: $(SRC) blit.h
	$(CC) -o $@ $(SRC) `sdl2-config --cflags --libs`
