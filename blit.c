#include "blit.h"

char *argv0;

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *screen;
u32int *pixels;
u32int fg, bg;
int running;

int baud = 40000;
Queue keyqueue, uartrxqueue, uarttxqueue;
int daddr;
u16int dstat;
u8int invert;
int vblctr, uartrxctr;
Rectangle updated;
int showcursor;

void
sysfatal(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
}

Rectangle
Rect(int minx, int miny, int maxx, int maxy)
{
	Rectangle r = { {minx, miny}, {maxx, maxy} };
	return r;
}

int
quspace(Queue *q)
{
	return (q->w+1)%QUEUESZ != q->r;
}
int
quempty(Queue *q)
{
	return q->r == q->w;
}
int
quget(Queue *q)
{
	int x;
	x = q->data[q->r++];
	q->r %= QUEUESZ;
	return x;
}
void
quput(Queue *q, int x)
{
	q->data[q->w++] = x;
	q->w %= QUEUESZ;
}

/* SDL doesn't seem to be able to handle keyboard layouts well
 * so we have to hardcode something. */

static uchar normal[] = {
	000, 001, 002, 003, 004, 005, 006, 007,
	010, 011, 012, 013, 014, 015, 016, 017,
	020, 021, 022, 023, 024, 025, 026, 027,
	030, 031, 032, 033, 034, 035, 036, 037,
	' ', '!', '"', '#', '$', '%', '&', '\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', 0177
};

/* US ASCII shifted on US layout */
static uchar shifted[] = {
	000, 001, 002, 003, 004, 005, 006, 007,
	010, 011, 012, 013, 014, 015, 016, 017,
	020, 021, 022, 023, 024, 025, 026, 027,
	030, 031, 032, 033, 034, 035, 036, 037,
	' ', '!', '"', '#', '$', '%', '&', '"',
	'(', ')', '*', '+', '<', '_', '>', '?',
	')', '!', '@', '#', '$', '%', '^', '&',
	'*', '(', ':', ':', '<', '+', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '{', '|', '}', '^', '_',
	'~', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '{', '|', '}', '~', 0177
};

struct
{
	int sdlk;
	uchar key;
} keymap[] = {
	{ SDLK_UP, 0xf1 },
	{ SDLK_DOWN, 0xf2 },
	{ SDLK_LEFT, 0xf3 },
	{ SDLK_RIGHT, 0xf4 },
	{ SDLK_F1, 0xf6 }, /* PF1 */
	{ SDLK_F2, 0xf7 }, /* PF2 */
	{ SDLK_F3, 0xf8 }, /* PF3 */
	{ SDLK_F4, 0xf9 }, /* PF4 */
	{ SDLK_F12, 0xfe }, /* SET-UP */
	{ SDLK_PAGEDOWN, 0xb0 }, /* SCROLL */
	{ SDLK_INSERT, 0xe0 }, /* BREAK */
	{ 0, 0 }
};

void
keydown(SDL_Keysym key)
{
	int i;
	int c;

	c = key.sym;
	if(c >= 0 && c < 0200){
		if(key.mod & (KMOD_LSHIFT | KMOD_RSHIFT | KMOD_LCTRL | KMOD_RCTRL))
			c = shifted[c];
		if(key.mod & (KMOD_LCTRL | KMOD_RCTRL))
			c &= 037;
		quput(&keyqueue, c);
		return;
	}
	if(key.sym == SDLK_END){
		running = 0;
		return;
	}
	if(key.sym == SDLK_F11){
		showcursor = !showcursor;
		SDL_ShowCursor(showcursor);
	}
	for(i = 0; keymap[i].sdlk; i++)
		if(keymap[i].sdlk == key.sym)
			quput(&keyqueue, keymap[i].key);
}

void
keyup(SDL_Keysym key)
{
}

void
mousemove(SDL_MouseMotionEvent m)
{
	mousex = SX - m.x - 1;
	mousey = SY - m.y - 1;
}

void
mousebutton(SDL_MouseButtonEvent m)
{
	int bit;
	int old;

	bit = 0;
	if(m.button == SDL_BUTTON_LEFT)
		bit = 4;
	if(m.button == SDL_BUTTON_MIDDLE)
		bit = 2;
	if(m.button == SDL_BUTTON_RIGHT)
		bit = 1;

	old = mousebut;
	if(m.state == SDL_PRESSED)
		mousebut |= bit;
	else
		mousebut &= ~bit;
	if(old != mousebut)
		irq |= INTMOUSE;
}

static void
redraw(void)
{
	ushort *p;
	u32int *q;
	ushort inv;
	int i, n;

	p = ram + daddr/2;
	q = pixels;
	inv = invert ? ~0 : 0;
	n = SX*SY/16;
	while(n--){
		for(i = 0; i < 16; i++)
			*q++ = (*p^inv) & 1<<(15-i) ? fg : bg;
		p++;
	}

	SDL_UpdateTexture(screen, nil, pixels, SX*4);

	SDL_RenderCopy(renderer, screen, nil, nil);
	SDL_RenderPresent(renderer);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-b baud] [-C bg,fg] [-d] [-p port] host\n", argv0);
	exit(1);
}

int
main(int argc, char *argv[])
{
	SDL_Event event;
	char *p;
	int n;
	int port;
	char *host;

	bg = 0xFF000000;
	fg = 0xFFFFFFFF;

	port = 22;
	host = nil;
	ARGBEGIN{
	case 'b':
		baud = strtol(EARGF(usage()), &p, 0);
		if(*p != '\0') usage();
		break;
	case 'C':
		p = EARGF(usage());
		bg = strtol(p, &p, 16) | 0xFF000000;
		if(*p++ != ',') usage();
		fg = strtol(p, &p, 16) | 0xFF000000;
		if(*p++ != '\0') usage();
		break;
	case 'd':
		diag++;
		break;
	case 'p':
		port = strtol(EARGF(usage()), &p, 0);
		if(*p != '\0') usage();
		break;
	}ARGEND;

	if(!diag){
		if(argc != 1) usage();
		host = argv[0];
		if(telnetinit(host, port))
			return 1;
	}

	if(SDL_Init(SDL_INIT_VIDEO) < 0)
		return 1;

	window = SDL_CreateWindow("blit",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		SX, SY, 0);
	if(window == nil)
		return 1;

	renderer = SDL_CreateRenderer(window, -1, 0);
	if(renderer == nil)
		return 1;

	screen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		SX, SY);
	if(screen == nil)
		return 1;

	pixels = malloc(SX*SY*4);

	SDL_ShowCursor(showcursor);

	meminit();
	cpureset();

	running = 1;
	while(running){
		while(SDL_PollEvent(&event))
			switch(event.type){
			case SDL_QUIT:
				running = 0;
				break;
			case SDL_KEYDOWN:
				keydown(event.key.keysym);
				break;
			case SDL_KEYUP:
				keyup(event.key.keysym);
				break;
			case SDL_MOUSEMOTION:
				mousemove(event.motion);
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				mousebutton(event.button);
				break;
			}
		telnet();
		keycheck();
		n = step();
		vblctr += n;
		if(vblctr >= VBLDIV){
			irq |= INTVBL;
			redraw();
			vblctr -= VBLDIV;
		}
		if(uartrxctr > 0)
			uartrxctr -= n;
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
