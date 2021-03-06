/* 
 *	Copyright (c) 2018, Exile Heavy Industries
 *	All rights reserved.
 *
 *	Redistribution and use in source and binary forms, with or without
 *	modification, are permitted (subject to the limitations in the disclaimer
 *	below) provided that the following conditions are met:
 *
 *	* Redistributions of source code must retain the above copyright notice, this
 *		list of conditions and the following disclaimer.
 *
 *	* Redistributions in binary form must reproduce the above copyright notice,
 *		this list of conditions and the following disclaimer in the documentation
 *		and/or other materials provided with the distribution.
 *
 *	* Neither the name of the copyright holder nor the names of its contributors may be used
 *		to endorse or promote products derived from this software without specific
 *		prior written permission.
 *
 *	NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS
 *	LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *	THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *	GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 *	OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *	DAMAGE.
 */

/* 
 * TODO: add logic to detect and expand '::' in ip6 addresses
 * TODO: Look at more performant listing options
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> /* ensure uint8_t and similar are available */
#include <string.h>
#include <unistd.h>

#define HELP 0x0002
#define LISTHOSTS 0x0001
#define NOHEADER 0x0004
#define IP4SEP '.'
#define IP6SEP ':'

extern char *__progname;

/*
 * single, 66 Byte struct to more efficiently pass 
 * information around, also removes the need to copy local variables
 * should be converted to using uint16_t for arrays so the ip6 info
 * is better preserved and presented, currently bit values are displayed
 * accurately, but values are misleading in decimal
 */
typedef struct addrinfo { 
		uint16_t addr[8];
		uint16_t mask[8];
		uint16_t ntwk[8];
		uint16_t bdst[8];
		uint8_t maskbits;
		uint8_t class; /* this is indirectly used to determine if it's an ip4 or ip6 address, 
											by telling us how many segments we should worry about,
											as such it should only be 4 or 8, at least for now */
} addr;

static int brdcast(addr *addr);
static int buildaddr(char *arg, addr *ip);
static int cook(uint8_t flags, char *args);
static int hostaddrs(const addr *addr);
static int netmask(addr *addr);
static int netwkaddr(addr *addr);
static int printinfo(addr *addr);
static void usage(void);

int 
main(int argc, char **argv) { 
	uint8_t flags;
	int ch, ret;

	ch = flags = ret = 0; 

	while ((ch = getopt(argc, argv, "hln")) != -1) { 
		switch(ch) { 
			case 'h':
				/* 0000 0010 */
				flags ^= HELP;
				flags &= HELP; /* only allow the one flag to be set */
				break;
			case 'l':
				/* 0100 0001 */
				flags ^= LISTHOSTS;
				flags &= LISTHOSTS;
				break;
			case 'n':
				if (flags >> 2 == NOHEADER) {
					break;
				} else {
					flags ^= NOHEADER;
					break;
				}
			default:
				flags ^= flags;
				break;
		}
	}

	/* Parse the arguments */
	if (argv[1] == NULL || flags == HELP) {
		usage();
	} else {
		/* this seems to have broken the cook() function test to run usage() */
		for (argv += optind; (ret != HELP) && (*argv != NULL); argv++) {
			ret = cook(flags, *argv);
		}
		/* this should return 0 in most cases */
		return(ret);
	}
}

static int
brdcast (addr *addr) {	
	register int i; 

	/* this is when all the host bits are set, so we should be able to just twiddle any leftover octets to get this working */
	for (i = 0; i < addr->class; i++) { 
		addr->mask[i] = ~addr->mask[i];
		addr->bdst[i] = (addr->class == 4) ? (addr->addr[i] | (addr->mask[i] ^ 0xFF00)) : (addr->addr[i] | addr->mask[i]); 
		addr->mask[i] = ~addr->mask[i];
	}
	return(0);
}

static int
buildaddr(char *arg, addr *ip) { 
	register int i,j,k;
	char buf[5];
	uint8_t hexval[4];

	memset(hexval,0,sizeof(hexval));
	*buf = k = 0;

	if (ip->class == 4) {
		for (i = 0,j = 0; arg[i] != 0; i++) { 
			if (arg[i] <= 57 && arg[i] >= 48) { 
				buf[j] = arg[i];
				j++;
			} else if (arg[i] == IP4SEP) { 
				buf[j++] = 0;
				j ^= j;
				ip->addr[k] = (uint16_t)atoi(buf); 
				k++;
				memset(buf, 0, sizeof(buf));
			} 
			if (arg[i] == '/') { 
				/* this is where the subnet mask is set */
				buf[j++] = 0;
				ip->addr[k] = (uint16_t)atoi(buf); /* write the last octet to ip->addr */
				j ^= j; /* reset j */
				k++;
				memset(buf, 0, sizeof(buf));
				/* get and write the subnet mask bits */
				buf[0] = arg[i+1];
				buf[1] = arg[i+2];
				buf[3] = 0;
				ip->maskbits = (uint8_t)atoi(buf);
				memset(buf, 0, sizeof(buf));
			} 
			/* if no / was encountered, make sure we flush *buf to the struct */
			if (atoi(buf) > 0) { 
				ip->addr[k] = (uint16_t)atoi(buf);
			}
		}
	} else if (ip->class == 8) {
		for (i = 0,j = 0; arg[i] != 0; i++) {
			if (arg[i] <= 57 && arg[i] >= 48) {
				buf[j] = arg[i]; 
				j++;
			} else if (arg[i] >= 97 && arg[i] <= 102) {
				buf[j] = (arg[i] - 32); 
				j++;
			} else if (arg[i] >= 65 && arg[i] <= 70) { 
				buf[j] = arg[i]; 
				j++;
			}
			/* this gets tricky, since ipv6 allows for "::" to compress a run of 0's */
			if (arg[i] == IP6SEP || arg[i + 1] == '/') {  
				for (j = 0; j < 5; j++) { 
					switch (buf[j]) { 
						/* should only have capitalized hex values */
						case 'A':
							hexval[j] = 10;
							break;
						case 'B':
							hexval[j] = 11;
							break;
						case 'C':
							hexval[j] = 12;
							break;
						case 'D':
							hexval[j] = 13;
							break;
						case 'E':
							hexval[j] = 14;
							break;
						case 'F':
							hexval[j] = 15;
							break;
						case '0':
							hexval[j] = 0;
							break;
						case '1':
							hexval[j] = 1;
							break;
						case '2':
							hexval[j] = 2;
							break;
						case '3':
							hexval[j] = 3;
							break;
						case '4':
							hexval[j] = 4;
							break;
						case '5':
							hexval[j] = 5;
							break;
						case '6':
							hexval[j] = 6;
							break;
						case '7':
							hexval[j] = 7;
							break;
						case '8':
							hexval[j] = 8;
							break;
						case '9':
							hexval[j] = 9;
							break;
						default:
							break;
					}
				}
				ip->addr[k] = (hexval[3] + (hexval[2] * 16) + (hexval[1] * 16 * 16) + (hexval[0] * 16 * 16 * 16));
				k++;
				j ^= j;
				memset(buf,0,sizeof(buf));
				memset(hexval,0,sizeof(hexval));
			}
			if (arg[i] == '/') {
				for (j = 0,i += 1; arg[i] != 0; i++) { 
					buf[j] = arg[i]; 
					j++;
				}
				ip->maskbits = (uint8_t)atoi(buf);
				break;
			}
		}
	}
	return(0);
}

static int 
cook(uint8_t flags, char *args) { 
	addr ip;

	if (flags == HELP || args == NULL) { 
		usage();
		return(HELP);
	} else { 
		if (strchr(args, IP4SEP) == NULL && strchr(args, IP6SEP) == NULL) { 
			fprintf(stderr,"%s: invalid IP address!\n",args);
			return(1);
		} 
		if (strchr(args, IP4SEP) != NULL) { 
			ip.class = 4; 
		} else if (strchr(args, IP6SEP) != NULL) { 
			ip.class = 8;
		}

		buildaddr(args, &ip);

		switch(flags) { 
			case 0:
				/* this should be the same as the default case, run all functions */
				netmask(&ip);
				brdcast(&ip);
				netwkaddr(&ip);
				printinfo(&ip);
				break;
			case LISTHOSTS:
				netmask(&ip);
				brdcast(&ip);
				netwkaddr(&ip);
				printinfo(&ip);
				hostaddrs(&ip);
				break;
			case LISTHOSTS | NOHEADER:
				netmask(&ip);
				brdcast(&ip);
				netwkaddr(&ip);
				hostaddrs(&ip);
				break;
			default:
				netmask(&ip);
				brdcast(&ip);
				netwkaddr(&ip);
				printinfo(&ip);
				break;
		}
	}
	return(0);
}

static int
hostaddrs(const addr *addr) {
	register uint8_t i, j, k, l;

	/*
	 * If list is nonzero, we're going to print out every address in the range
	 * otherwise, just print the summary, first host and last host
	 * There's almost certainly a better way to do this, but I'm not sure what it'd be at this time
	 * XXX: Also, get it to stop printing the network address of a given subnet
	 *
	 * XXX: These loops can almost certainly be better expressed as a singe loop, possibly two,
	 * using the value of addr->class. Additionally, there may be a way to use bitwise operations 
	 * to display the address possibilities.
	 */
	if (addr->class == 4) { 
		for(i = 0; (addr->ntwk[0] + i) <= addr->bdst[0]; i++) {
			for (j = 0; (addr->ntwk[1] + j) <= addr->bdst[1]; j++) {
				for (k = 0; (addr->ntwk[2] + k) <= addr->bdst[2]; k++) {
					for (l = 0; (addr->ntwk[3] + l) <= (addr->bdst[3] - 1); l++) {
						fprintf(stdout,"%u.%u.%u.%u\n",addr->ntwk[0] + i, addr->ntwk[1] + j, addr->ntwk[2] + k, addr->ntwk[3] + l);
					}
				}
			}
		}
	} else if (addr->class == 8) {
		for (i = 0; (addr->ntwk[0] + i) <= addr->bdst[0]; i++) {
			for (j = 0; (addr->ntwk[1] + j) <= addr->bdst[1]; j++) {
				for (k = 0; (addr->ntwk[2] + k) <= addr->bdst[2]; k++) {
					for (l = 0; (addr->ntwk[3] + l) <= addr->bdst[3]; l++) {
						for (register uint8_t m = 0; (addr->ntwk[4] + m) <= addr->bdst[4]; m++) {
							for (register uint8_t n = 0; (addr->ntwk[5] + n) <= addr->bdst[5]; n++) {
								for (register uint8_t o = 0; (addr->ntwk[6] + o) <= addr->bdst[6]; o++) {
									for (register uint8_t p = 0; (addr->ntwk[7] + p) <= addr->bdst[7] - 1; p++) {
										fprintf(stdout,"%04X:%04X:%04X:%04X:%04X:%04X:%04X:%04X\n",
										addr->ntwk[0] + i, addr->ntwk[1] + j, addr->ntwk[2] + k, addr->ntwk[3] + l,
										addr->ntwk[4] + m, addr->ntwk[5] + n, addr->ntwk[6] + o, addr->ntwk[7] + p);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	/* 
	 * need to loop over each field in order to properly display all addresses
	 */
	return(0);
}

static int
netmask(addr *addr) { 
	register int i;
	uint16_t segsize,zero;

	zero = 0;
	/* assume a mask of all 1's if none was given */
	if (addr->maskbits == 0) {
		addr->maskbits = (addr->class == 4) ? 32 : 128;
	}
	segsize = (addr->class == 4) ? 8 : 16;

	for (i = 0; i < (addr->maskbits / segsize); i++) { 
		/* XOR out the top 8 bits if we're using ipv4 */
		addr->mask[i] = (addr->class == 4) ? ~zero ^ 0xFF00 : ~zero;
	}
	if ((addr->maskbits % segsize) > 0 ) { 
		addr->mask[i] = (addr->class == 4) ? ((~zero << (segsize - (addr->maskbits % segsize))) & 0x00FF) : ((~zero >> ( 16 - (addr->maskbits % 16))) << (16 - (addr->maskbits % 16)));
		i++;
	}
	addr->mask[i] = zero;
	return(0);
}

static int
netwkaddr(addr *addr) { 
	/* should pass in the netmask generated in netmask(), so it should be a simple bitwise operation */
	register int i; 

	for (i = 0; i < addr->class; i++) { 
		addr->ntwk[i] = (addr->addr[i] & addr->mask[i]);
	}
	return(0);
}

/* must find a better means of presenting this information */
static int
printinfo(addr *addr) { 
	if (addr->class == 4) { 
		fprintf(stdout,
				"Address:\t%u.%u.%u.%u\n"
				"Netmask:\t%u.%u.%u.%u\n"
				"Hexmask:\t0x%02X%02X%02X%02X\n"
				"Octmask:\t0%03o%03o%03o%03o\n"
				"Network:\t%u.%u.%u.%u\n"
				"Broadcast:\t%u.%u.%u.%u\n"
				"IP Range:\t%u.%u.%u.%u - %u.%u.%u.%u\n",
				addr->addr[0],addr->addr[1],addr->addr[2],addr->addr[3],
				addr->mask[0],addr->mask[1],addr->mask[2],addr->mask[3],
				addr->mask[0],addr->mask[1],addr->mask[2],addr->mask[3], 
				addr->mask[0],addr->mask[1],addr->mask[2],addr->mask[3],
				addr->ntwk[0],addr->ntwk[1],addr->ntwk[2],addr->ntwk[3],
				addr->bdst[0],addr->bdst[1],addr->bdst[2],addr->bdst[3],
				addr->ntwk[0],addr->ntwk[1],addr->ntwk[2],((addr->maskbits == 32) ? addr->addr[3] : (addr->ntwk[3] + 1)),
				addr->bdst[0],addr->bdst[1],addr->bdst[2],((addr->maskbits == 32) ? addr->addr[3] : (addr->bdst[3] - 1))); 
	} else { 
		fprintf(stderr,
				"Address:\t%04X:%04X:%04X:%04X:%04X:%04X:%04X:%04X\n"
				"Netmask:\t%u:%u:%u:%u:%u:%u:%u:%u\n"
				"Hexmask:\t%04X:%04X:%04X:%04X:%04X:%04X:%04X:%04X\n"
				"Octmask:\t%03o:%03o:%03o:%03o:%03o:%03o:%03o:%03o\n"
				"Network:\t%04X:%04X:%04X:%04X:%04X:%04X:%04X:%04X\n"
				"Broadcast:\t%04X:%04X:%04X:%04X:%04X:%04X:%04X:%04X\n"
				"IP Range:\t%04X:%04X:%04X:%04X:%04X:%04X:%04X:%04X - %04X:%04X:%04X:%04X:%04X:%04X:%04X:%04X\n"
				"addr->maskbits:\t%u\n",
				addr->addr[0],addr->addr[1],addr->addr[2],addr->addr[3],addr->addr[4],addr->addr[5],addr->addr[6],addr->addr[7],
				/* netmask */
				addr->mask[0],addr->mask[1],addr->mask[2],addr->mask[3],addr->mask[4],addr->mask[5],addr->mask[6],addr->mask[7],
				/* conversions */
				addr->mask[0],addr->mask[1],addr->mask[2],addr->mask[3],addr->mask[4],addr->mask[5],addr->mask[6],addr->mask[7],
				addr->mask[0],addr->mask[1],addr->mask[2],addr->mask[3],addr->mask[4],addr->mask[5],addr->mask[6],addr->mask[7],
				/* network */
				addr->ntwk[0],addr->ntwk[1],addr->ntwk[2],addr->ntwk[3],addr->ntwk[4],addr->ntwk[5],addr->ntwk[6],addr->ntwk[7],
				/* broadcast */
				addr->bdst[0],addr->bdst[1],addr->bdst[2],addr->bdst[3],addr->bdst[4],addr->bdst[5],addr->bdst[6],addr->bdst[7],
				/* range */
				addr->ntwk[0],addr->ntwk[1],addr->ntwk[2],addr->ntwk[3],addr->ntwk[4],addr->ntwk[5],addr->ntwk[6],((addr->maskbits == 128) ? addr->addr[7] : (addr->ntwk[7] + 1)),
				addr->bdst[0],addr->bdst[1],addr->bdst[2],addr->bdst[3],addr->bdst[4],addr->bdst[5],addr->bdst[6],((addr->maskbits == 128) ? addr->addr[7] : (addr->bdst[7] - 1)),
				/* maskbits */
				addr->maskbits
				);
	} 

	return(0);
}

static void
usage(void) { 
	fprintf(stdout,"%s: Simple IP address and netmask calculator\n",__progname);
	fprintf(stdout,"\t-h\tThis help message\n"
								 "\t-l\tList all possible host addresses, not just the range (EXTREMELY verbose in larger networks)\n"
								 "\t-n\tDo not display the header (useful for scripting)\n"
								 "\n\tEx: ip 192.168.0.0/24\n"
								 "\t    ip fe80::6a05:caff:fe3f:a9da/64\n");
}
