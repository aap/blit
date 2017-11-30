#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <SDL.h>
#include "args.h"

#define nil NULL
#define nelem(x) (sizeof(x)/sizeof(x[0]))

typedef unsigned char uchar;
typedef uint8_t u8int;
typedef uint16_t u16int;
typedef uint32_t u32int;
typedef uint64_t u64int;
typedef int8_t s8int;
typedef int16_t s16int;
typedef int32_t s32int;
typedef long long vlong;
typedef unsigned long long uvlong;

typedef struct Point Point;
struct Point
{
	int x;
	int y;
};

typedef struct Rectangle Rectangle;
struct Rectangle
{
	Point min;
	Point max;
};

Rectangle Rect(int minx, int miny, int maxx, int maxy);


/* Ring buffer for key presses and transmission */
#define QUEUESZ 1000
typedef struct Queue Queue;
struct Queue
{
	int r, w;
	int data[QUEUESZ];
};
extern Queue keyqueue, uartrxqueue, uarttxqueue;
int quspace(Queue *q);
int quempty(Queue *q);
int quget(Queue *q);
void quput(Queue *q, int x);


extern u32int curpc, irq;
extern int trace, debug;

extern ushort ram[128*1024];

extern int daddr;
extern ushort dstat;
extern uchar invert;

extern int mousex, mousey, mousebut;

extern int vblctr, uartrxctr;
extern int baud;
extern int diag;

enum {
	INTKEY = 1,
	INTMOUSE = 2,
	INTUART = 4,
	INTVBL = 8,
};

enum {
	SX = 800,
	SY = 1024,
	FREQ = 8000*1000,
	VBLDIV = FREQ / 60,
};

void sysfatal(char *fmt, ...);

void keycheck(void);
void meminit(void);
u16int memread(u32int a);
void memwrite(u32int a, u16int v, u16int m);
int intack(int l);

void cpureset(void);
int step(void);


int telnetinit(char *host, int port);
void telnet(void);
