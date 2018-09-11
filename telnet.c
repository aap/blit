#include "blit.h"

enum {
	SE = 240,
	NOP = 241,
	BRK = 243,
	IP = 244,
	AO = 245,
	AYT = 246,
	EC = 247,
	EL = 248,
	GA = 249,
	SB = 250,
	WILL = 251,
	WONT = 252,
	DO = 253,
	DONT = 254,
	IAC = 255,
	
	XMITBIN = 0,
	ECHO = 1,
	SUPRGA = 3,
	LINEEDIT = 34,
	
};

static int telfd = -1;
int teldebug = 0;

static int
dial(char *host, int port)
{
	char portstr[32];
	int sockfd;
	struct addrinfo *result, *rp, hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(portstr, 32, "%d", port);
	if(getaddrinfo(host, portstr, &hints, &result)){
		perror("error: getaddrinfo");
		return -1;
	}

	for(rp = result; rp; rp = rp->ai_next){
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if(sockfd < 0)
			continue;
		if(connect(sockfd, rp->ai_addr, rp->ai_addrlen) >= 0)
			goto win;
		close(sockfd);
	}
	freeaddrinfo(result);
	perror("error");
	return -1;

win:
	freeaddrinfo(result);
	return sockfd;
}


static int
hasinput(int fd)
{
	fd_set fds;
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	return select(fd+1, &fds, nil, nil, &timeout) > 0;
}

void
telnet(void)
{
	uchar c;
	static int state = 0;

	if(telfd < 0)
		return;
	while(!quempty(&uarttxqueue)){
		c = quget(&uarttxqueue);
		write(telfd, &c, 1);
		if(c == 0xFF)
			write(telfd, &c, 1);
	}
	while(hasinput(telfd) && quspace(&uartrxqueue))
	if(read(telfd, &c, 1) == 1)
		switch(state){
		case 0:
			if(c != IAC)
				quput(&uartrxqueue, c);
			else
				state = 1;
			break;
		case 1:
			switch(c){
			case NOP:
				state = 0;
				break;
			case WILL:
				if(teldebug) fprintf(stderr, "WILL ");
				state = 2;
				break;
			case WONT:
				if(teldebug) fprintf(stderr, "WONT ");
				state = 2;
				break;
			case DO:
				if(teldebug) fprintf(stderr, "DO ");
				state = 2;
				break;
			case DONT:
				if(teldebug) fprintf(stderr, "DONT ");
				state = 2;
				break;
			case IAC:
				quput(&uartrxqueue, c);
				state = 0;
				break;
			default:	
				fprintf(stderr, "unknown telnet command %d\n", c);
				state = 0;
			}
			break;
		case 2:
			if(teldebug) fprintf(stderr, "%d\n", c);
			state = 0;
		}
}

static void
cmd(uchar a, uchar b)
{
	write(telfd, &a, 1);
	write(telfd, &b, 1);
}

int
telnetinit(char *host, int port)
{
	telfd = dial(host, port);
	if(telfd < 0)
		return 1;
	cmd(WILL, XMITBIN);
	cmd(DO, XMITBIN);
	cmd(DONT, ECHO);
	cmd(DO, SUPRGA);
	cmd(WILL, SUPRGA);
	cmd(WONT, LINEEDIT);
	cmd(DONT, LINEEDIT);
	return 0;
}
