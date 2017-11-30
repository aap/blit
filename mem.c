#include "blit.h"

u32int irq;
u32int irql[8] = {
	[1] INTVBL,
	[2] INTKEY,
	[4] INTMOUSE,
	[5] INTUART,
};
int diag;

ushort ram[128*1024];
ushort rom[3*4096];
int mousex, mousey, mousebut;

int yes;
u8int kbdctrl, uartctrl;
enum {
	ACIATXMASK = 0x60,
	ACIATXIRQ = 0x20,
	ACIARXIRQ = 0x80,
};

int keybuf = -1;
int uartrxbuf = -1;
int uarttxbuf = -1;

#include "romdump.inc"
#include "diagbits.inc"

void
meminit(void)
{
	int i, x;
	FILE *fp;
	char *s;
	ushort *p, *q;
	char buf[100];

	p = rom;
	if(diag){
/*
		fp = fopen("diagbits", "rb");
		if(fp == nil) sysfatal("couldn't open file diagbits");
		fread(rom, 1, sizeof(rom), fp);
		fclose(fp);
*/
		memcpy(rom, diagbits, diagbits_len);
		return;
	}
	memcpy(rom, romdump, romdump_len);
/*
	for(i = 0; i < 6; i++){
		sprintf(buf, "rom%d", i);
		fp = fopen(buf, "rb");
		if(fp == nil) sysfatal("couldn't open file %s", buf);
		q = p;
		for(;;){
			s = fgets(buf, 100, fp);
			if(s == nil || strlen(buf) == 0) break;
			x = strtol(s, nil, 8);
			if((i & 1) != 0)
				*p |= x << 8;
			else
				*p |= x;
			p++;
		}
		if((i & 1) == 0) p = q;
		fclose(fp);
	}
*/
//	write(3, rom, sizeof(rom));
}

void
keycheck(void)
{
	if(keybuf < 0 && !quempty(&keyqueue))
		keybuf = quget(&keyqueue);
	if(keybuf >= 0 && (kbdctrl & ACIARXIRQ) != 0)
		irq |= INTKEY;
	else
		irq &= ~INTKEY;

	if(uartrxbuf < 0 && uartrxctr <= 0){
		if(!quempty(&uartrxqueue))
			uartrxbuf = quget(&uartrxqueue);
		uartrxctr = FREQ * 11 / baud;
	}
	if(uarttxbuf >= 0 && quspace(&uarttxqueue)){
		quput(&uarttxqueue, uarttxbuf);
		uarttxbuf = -1;
	}
	if(uartrxbuf >= 0 && (uartctrl & ACIARXIRQ) != 0 ||
	   uarttxbuf < 0 && (uartctrl & ACIATXMASK) == ACIATXIRQ)
		irq |= INTUART;
	else
		irq &= ~INTUART;
}

u16int
memread(u32int a)
{
	int rc;

	a &= 0x3fffff;
	if(a < 8) a += 0x40000;
	if(a < 0x40000) return ram[a/2];
	if(a >= 0x40000 && a < 0x40000 + sizeof(rom))
		return rom[(a - 0x40000)/2];
	switch(a & ~1){
	case 01400000: return mousey;
	case 01400002: return mousex;
	case 01400010: /* uart status */
		rc = 0;
		if(uartrxbuf >= 0) rc |= 1;
		if(uarttxbuf < 0) rc |= 2;
		return rc | rc << 8;
	case 01400012: /* uart data */
		rc = uartrxbuf;
		uartrxbuf = -1;
		yes=1;
		return rc | rc << 8;
	case 01400020:
	case 01400024:
		irq &= ~INTMOUSE;
		return mousebut | mousebut << 8;
	case 01400026: return 0; /* mouse: unknown purpose */
	case 01400030: return daddr >> 2; /* display address */
	case 01400040: return dstat; /* display status */
	case 01400060: /* keyboard status */
		rc = 2;
		if(keybuf >= 0) rc |= 1;
		return rc | rc << 8;
	case 01400062: /* keyboard data */
		rc = keybuf;
		keybuf = -1;
		return rc | rc << 8;
	}
	printf("read %.8o (curpc = %.6x)\n", a, curpc & 0x3fffff);
	return 0;
}

void
memwrite(u32int a, u16int v, u16int m)
{
	extern Rectangle updated;
	int x, y;

	a &= 0x3fffff;
	if(a < 0x40000){
		if(a >= daddr){
			y = (a - daddr) / 100;
			x = (((a & ~1) - daddr) % 100) * 8;
			if(updated.min.x > x) updated.min.x = x;
			if(updated.max.x < x+16) updated.max.x = x+16;
			if(updated.min.y > y) updated.min.y = y;
			if(updated.max.y <= y) updated.max.y = y+1;
		}
		ram[a/2] = ram[a/2] & ~m | v & m;
		return;
	}
	switch(a & ~1){
	case 01400010: uartctrl = v; return;
	case 01400012: uarttxbuf = (uchar) v; return;
	case 01400024: return; /* mouse: purpose unknown */
	case 01400026: return; /* mouse: purpose unknown */
	case 01400030: daddr = ((daddr >> 2) & ~m | v & m) << 2; updated = Rect(0, 0, SX, SY); return;
	case 01400040: dstat = dstat & ~m | v & m; invert = -(dstat & 1); updated = Rect(0, 0, SX, SY); return;
	case 01400056: /* sound; exact function unknown */ return;
	case 01400060: kbdctrl = v; return;
	case 01400062: /* reset keyboard */ return;
	case 01400070: irq &= ~INTVBL; return;
	case 01400156: /* sound; exact function unknown */ return;
	}
	printf("write %.8o = %.4x (mask = %.4x, curpc = %.6x)\n", a, v, m, curpc & 0x3fffff);
}

int
intack(int l)
{
	return 24+l;
}
