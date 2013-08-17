#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <SDL.h>
#include <NEShead.h>

#define HEIGHT 239
#define WIDTH 256

int cpu_powerup();
int cpu_reset();
int ppu_powerup();
void reset();
uint8_t* mem(uint16_t addrs);
uint8_t* pmem(uint16_t addrs);
uint8_t* memap(uint16_t addrs);
uint8_t pop();
void opdec(uint8_t opcode);
void znchk(uint8_t reg);
uint8_t addc(uint8_t a, uint8_t b);
uint8_t sub(uint8_t a, uint8_t b);
uint8_t subc(uint8_t a, uint8_t b);
uint16_t rel(uint8_t x);
int prep_SDL();
void nmi();
int draw_sprite(int x, int y, uint16_t pat);
//unsigned char *chr_rom_dat;

uint16_t PC; // program counter
uint8_t mar, mdr, ir, cu, alu;
uint8_t SP, A, X, Y, P;
uint8_t* MEM;
uint8_t* VRAM;
uint8_t* OAM;
uint8_t PPUSTATUS;
uint8_t PPUCTRL;
uint8_t PPUMASK;
uint8_t PPUSCROLL;
uint8_t PPUADDR;
uint8_t PPUDATA;
uint16_t Loopy_T;
uint16_t Loopy_V;
uint16_t Loopy_X;

uint8_t pad1;
uint8_t pad1real;

uint16_t vradd;
int x2002read=0;
int x2004write=0;
int x2005write=0;
int x2006write=0;
int x2007write=0;
int x4014write=0;  // 15/6/13  This was originally initialised to 1, but I can't currently think of a valid reason why, and I think it caused reading of uninitialized memory.
int addscrolllatch=0;
int nmiflag=0;
int bkgndadrs;
int sprtadrs;
int strobe=0;

int count;

SDL_Renderer* renderer;
SDL_Window *mainwindow;
SDL_Surface *surf;

enum {
	C_FLAG = 0x01,
	Z_FLAG = 0x02,
	I_FLAG = 0x04,
	D_FLAG = 0x08,
	B_FLAG = 0x10,
	// Bit 0x20 of the status register is not used.
	V_FLAG = 0x40,
	N_FLAG = 0x80 		
};

enum {
	PAD_LEFT = 0x01,
	PAD_RIGHT = 0x02,
	PAD_UP = 0x04,
	PAD_DOWN = 0x08,
	PAD_A = 0x10,
	PAD_B = 0x20,
	PAD_START = 0x40,
	PAD_SELECT = 0x80 		
};

int ppu_powerup(){  //  15/6/13 I think this whole function needs checking!!!!
	// 15/6/13 was PPUCTRL&=0x40;, but I can't think of a valid reason.
	PPUCTRL=0x00;
	*mem(0x2000)=0x00; // 15/6/13 This is also PPUCTRL, I should rethink this design.
	PPUMASK&=0x06;
	PPUSTATUS&=~(0x40);
	PPUSTATUS|=0xA0;
	*mem(0x2003)=0;
	PPUSCROLL=0;
	PPUADDR=0;
}


uint8_t fetch(){// TEMP DODGY
	PC++;
	return *mem(PC);
}
int asl(uint8_t temp){ //  TEMP DODGY
	fprintf(stderr, "ERROR: opcode not recognised");
	exit(-1);
	return 0;
}
int add(){ //  TEMP DODGY
	fprintf(stderr, "ERROR: opcode not recognised");
	exit(-1);
	return 0;
}
uint8_t addc(uint8_t a, uint8_t b){
	int carry;
	if((P&C_FLAG)!=0){
		carry=1;
	} else {
		carry=0;
	}

	uint16_t tot=(uint16_t)a+(uint16_t)b+carry;

	int aneg=0;
	int bneg=0;
	if((a&0x80)!=0){
		aneg=1;
	}
	if((b&0x80)!=0){
		bneg=1;
	}

	//printf("%X\n", tot);
	if ((tot&0x0100)!=0){
		P|=C_FLAG;
	} else {
		P&=~C_FLAG;
	}
	tot&=0xFF;

	if((tot&0x80)!=0){
		if(aneg==0 && bneg==0){
			P|=V_FLAG;
		} else {
			P&=~V_FLAG;
		}
	} else {
		if(aneg==1 && bneg==1){
			P|=V_FLAG;
		} else {
			P&=~V_FLAG;
		}
	}

	znchk(tot);
	return tot;
}
int and(){ //  TEMP DODGY
	fprintf(stderr, "ERROR: opcode not recognised");
	exit(-1);
	return 0;
}

int disasm(uint8_t opcode, char* str){
	switch (opcode){
		// ADC
		case 0x69: // #$aa
			sprintf(str, "ADC #$%02X", *mem(PC+1));
			break;
		case 0x65: // $aa
			sprintf(str, "ADC $%02X = %02X", *mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0x75: // $aa,X
			sprintf(str, "ADC $%02X,X @ %02X = %02X", *mem(PC+1), *mem(PC+1)+X, *mem(*mem(PC+1)+X));
			break;
		case 0x6D: // $aaaa
			sprintf(str, "ADC $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2) << 8) | *mem(PC+1)));
			break;
		case 0x7D: // $aaaa,X
			sprintf(str, "ADC $%02X%02X,X @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+X, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+X));
			break;
		case 0x79: // $aaaa,Y
			sprintf(str, "ADC $%02X%02X,Y @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+Y, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+Y));
			break;
		case 0x61: // ($aa,X)
			sprintf(str, "ADC ($%02X,X) @ %02X = %02X%02X = %02X", *memap(PC+1), 0xFF & *memap(PC+1)+X, *memap((uint8_t)(*memap(PC+1)+X+1)), *memap(0xFF & (*memap(PC+1)+X)), *memap(((*memap(0xFF & (*memap(PC+1)+X+1))) << 8) | *memap(0xFF & (*memap(PC+1)+X))));
			break;
		case 0x71: // ($aa),Y
			sprintf(str, "ADC ($%02X),Y = %04X @ %04X = %02X",*mem(PC+1),(*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)), (((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)&0xFFFF, *memap((((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)) & 0xFF);
			break;
		// AND
		case 0x29: // #aa
			sprintf(str, "AND #$%02X  ", *mem(PC+1));
			break;
		case 0x25: // $aa
			sprintf(str, "AND $%02X = %02X", *mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0x35: // $aa,X
			sprintf(str, "AND $%02X,X @ %02X = %02X", *mem(PC+1), *mem(PC+1)+X, *mem(*mem(PC+1)+X));
			break;
		case 0x2D:{ // $aaaa
				sprintf(str, "AND $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2) << 8) | *mem(PC+1)));
				break;
			}
		case 0x3D:{ // $aaaa,X
				sprintf(str, "AND $%02X%02X,X @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+X, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+X));
				break;
			}
		case 0x39:{ // $aaaa,Y
				sprintf(str, "AND $%02X%02X,Y @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+Y, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+Y));
				break;
			}
		case 0x21:{ // (aa,X)
				sprintf(str, "AND ($%02X,X) @ %02X = %02X%02X = %02X", *memap(PC+1), 0xFF & *memap(PC+1)+X, *memap((uint8_t)(*memap(PC+1)+X+1)), *memap(0xFF & (*memap(PC+1)+X)), *memap(((*memap(0xFF & (*memap(PC+1)+X+1))) << 8) | *memap(0xFF & (*memap(PC+1)+X))));
				break;
			}
		case 0x31: // (aa),Y
			sprintf(str, "AND ($%02X),Y = %04X @ %04X = %02X",*mem(PC+1),(*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)), (((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)&0xFFFF, *memap((((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)) & 0xFF);
			break;
		// ASL
		case 0x0A: // A
			sprintf(str, "ASL A");
			break;
		case 0x06: // $aa
			sprintf(str, "ASL $%02X = %02X",*mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0x16: // $aa,X
			sprintf(str, "ASL $%02X,X @ %02X = %02X", *mem(PC+1), *mem(PC+1)+X, *mem(*mem(PC+1)+X));
			break;
		case 0x0E: // $aaaa
			sprintf(str, "ASL $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2) << 8) | *mem(PC+1)));
			break;
		case 0x1E: // $aaaa,X
			sprintf(str, "ASL $%02X%02X,X @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+X, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+X));
			break;
		// BCC
		case 0x90:
			sprintf(str, "BCC $%02X  ", rel(*mem(PC+1))+2);
			break;
		// BCS
		case 0xB0:
			sprintf(str, "BCS $%02X  ", rel(*mem(PC+1))+2);
			break;
		// BEQ
		case 0xF0:
			sprintf(str, "BEQ $%02X  ", rel(*mem(PC+1))+2);
			break;
		// BIT
		case 0x24: // $aa
			sprintf(str, "BIT $%02X = %02X",*mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0x2C: // $aaaa
			sprintf(str, "BIT $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2) << 8) | *mem(PC+1)));
			break;
		// BMI
		case 0x30:
			sprintf(str, "BMI $%04X", rel(*mem(PC+1))+2);
			break;
		// BNE
		case 0xD0:
			sprintf(str, "BNE $%04X", rel(*mem(PC+1))+2);
			break;
		// BPL
		case 0x10:{
				//sprintf(str, "BPL $%02X\n",*mem(PC+1));
				sprintf(str, "BPL $%02X  ", rel(*mem(PC+1))+2);
				//uint8_t tmp;
				//tmp = fetch();
				//if((P & N_FLAG)==0){
				//	if((tmp & N_FLAG)!=0){ 
				//		PC+=(tmp-256); // Crudely take away negative number from PC.

				//	}
				//	else{
				//		PC+=tmp;
				//	}
				//}
				break;
			}
		// BRK
		case 0x00:
			sprintf(str, "BRK");
			break;
		// BVC
		case 0x50:
			sprintf(str, "BVC $%04X", rel(*mem(PC+1))+2);
			break;
		// BVS
		case 0x70:
			sprintf(str, "BVS $%04X", rel(*mem(PC+1))+2);
			break;
		// CLC
		case 0x18:
			sprintf(str, "CLC");
			break;
		// CLD
		case 0xD8:
			sprintf(str, "CLD");
			//P&=~D_FLAG;
			break;
		// CLI
		case 0x58:
			sprintf(str, "CLI");
			break;
		// CLV
		case 0xB8:
			sprintf(str, "CLV");
			break;
		// CMP
		case 0xC9: // #$aa
			sprintf(str, "CMP #$%02X", *mem(PC+1));
			break;
		case 0xC5: // $aa
			sprintf(str, "CMP $%02X = %02X", *mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0xD5: // $aa,X
			sprintf(str, "CMP $%02X,X @ %02X = %02X", *mem(PC+1), *mem(PC+1)+X, *mem(*mem(PC+1)+X));
			break;
		case 0xCD: // $aaaa
			sprintf(str, "CMP $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2) << 8) | *mem(PC+1)));
			break;
		case 0xDD: // $aaaa,X
			sprintf(str, "CMP $%02X%02X,X @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+X, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+X));
			break;
		case 0xD9: // $aaaa,Y
			sprintf(str, "CMP $%02X%02X,Y @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+Y, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+Y));
			break;
		case 0xC1: // ($aa,X)
			sprintf(str, "CMP ($%02X,X) @ %02X = %02X%02X = %02X", *memap(PC+1), 0xFF & *memap(PC+1)+X, *memap((uint8_t)(*memap(PC+1)+X+1)), *memap(0xFF & (*memap(PC+1)+X)), *memap(((*memap(0xFF & (*memap(PC+1)+X+1))) << 8) | *memap(0xFF & (*memap(PC+1)+X))));
			break;
		case 0xD1: // ($aa),Y
			sprintf(str, "CMP ($%02X),Y = %04X @ %04X = %02X",*mem(PC+1),(*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)), (((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)&0xFFFF, *memap((((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)) & 0xFF);
			break;
		// CPX
		case 0xE0: // #$aa
			sprintf(str, "CPX #$%02X", *mem(PC+1));
			break;
		case 0xE4: // $aa
			sprintf(str, "CPX $%02X = %02X", *mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0xEC: // $aaaa
			sprintf(str, "CPX $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2) << 8) | *mem(PC+1)));
			break;
		// CPY
		case 0xC0: // #$aa
			sprintf(str, "CPY #$%02X", *mem(PC+1));
			break;
		case 0xC4: // $aa
			sprintf(str, "CPY $%02X = %02X", *mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0xCC: // $aaaa
			sprintf(str, "CPY $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2) << 8) | *mem(PC+1)));
			break;
		// DEC
		case 0xC6: // $aa
			sprintf(str, "DEC $%02X = %02X", *mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0xD6: // $aa,X
			sprintf(str, "DEC $%02X,X @ %02X = %02X", *mem(PC+1), *mem(PC+1)+X, *mem(*mem(PC+1)+X));
			break;
		case 0xCE: // $aaaa
			sprintf(str, "DEC $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2) << 8) | *mem(PC+1)));
			break;
		case 0xDE: // $aaaa,X
			sprintf(str, "DEC $%02X%02X,X @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+X, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+X));
			break;
		// DEX
		case 0xCA:
			sprintf(str, "DEX");
			break;
		// DEY
		case 0x88:
			sprintf(str, "DEY");
			break;
		// EOR
		case 0x49: // #aa
			sprintf(str, "EOR #$%02X",*mem(PC+1));
			break;
		case 0x45: // $aa
			sprintf(str, "EOR $%02X = %02X",*mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0x55: // $aa,X
			sprintf(str, "EOR $%02X,X @ %02X = %02X",*mem(PC+1), *mem(PC+1)+X, *mem((*mem(PC+1)+X) & 0xFF));
			break;
		case 0x4D: // $aaaa
			sprintf(str, "EOR $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2) << 8) | *mem(PC+1)));
			break;
		case 0x5D: // $aaaa,X
			sprintf(str, "EOR $%02X%02X,X @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+X, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+X));
			break;
		case 0x59: // $aaaa,Y
			sprintf(str, "EOR $%02X%02X,Y @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+Y, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+Y));
			break;
		case 0x41: // ($aa,X)
			sprintf(str, "EOR ($%02X,X) @ %02X = %02X%02X = %02X", *memap(PC+1), 0xFF & *memap(PC+1)+X, *memap((uint8_t)(*memap(PC+1)+X+1)), *memap(0xFF & (*memap(PC+1)+X)), *memap(((*memap(0xFF & (*memap(PC+1)+X+1))) << 8) | *memap(0xFF & (*memap(PC+1)+X))));
			break;
		case 0x51: // ($aa),Y
			sprintf(str, "EOR ($%02X),Y = %04X @ %04X = %02X",*mem(PC+1),(*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)), (((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)&0xFFFF, *memap((((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)) & 0xFF);
			break;
		// INC
		case 0xE6: // $aa
			sprintf(str, "INC $%02X = %02X", *mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0xF6: // $aa,X
			sprintf(str, "INC $%02X,X @ %02X = %02X",*mem(PC+1), *mem(PC+1)+X, *mem((*mem(PC+1)+X) & 0xFF));
			break;
		case 0xEE: // $aaaa
			sprintf(str, "INC $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2) << 8) | *mem(PC+1)));
			break;
		case 0xFE: // $aaaa,X
			sprintf(str, "INC $%02X%02X,X @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+X, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+X));
			break;
		// INX
		case 0xE8:
			sprintf(str, "INX");
			break;
		// INY
		case 0xC8:
			sprintf(str, "INY");
			break;
		// JMP
		case 0x4C: // $aaaa
			sprintf(str, "JMP $%02X%02X",*mem(PC+2),*mem(PC+1));
			break;
		case 0x6C: // ($aaaa)
			sprintf(str, "JMP ($%02X%02X) = %04X", *mem(PC+2), *mem(PC+1), (*memap((((uint16_t)*mem(PC+2) << 8) | *mem(PC+1))+1)<<8) | *memap(((uint16_t)*mem(PC+2) << 8) | *mem(PC+1)));
			break;
		// JSR
		case 0x20:
			sprintf(str, "JSR $%02X%02X", *mem(PC+2), *mem(PC+1));
			break;
		// LDA
		case 0xA9:
			sprintf(str, "LDA #$%02X",*mem(PC+1));
			break;
		case 0xA5:
			sprintf(str, "LDA $%02X = %02X", *mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0xB5: // $aa,X
			sprintf(str, "LDA $%02X,X @ %02X = %02X",*mem(PC+1), (*mem(PC+1)+X)&0xFF, *mem((*mem(PC+1)+X) & 0xFF));
			break;
		case 0xAD:{ // $aaaa
				sprintf(str, "LDA $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2) << 8) | *mem(PC+1)));
				break;
			}
		case 0xBD: // $aaaa,X
			sprintf(str, "LDA $%02X%02X,X @ %04X = %02X", *mem(PC+2), *mem(PC+1),(((*mem(PC+2)<<8)|*mem(PC+1))+X)&0xFFFF, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+X));
			break;
		case 0xB9: // $aaaa,Y
			sprintf(str, "LDA $%02X%02X,Y @ %04X = %02X", *mem(PC+2), *mem(PC+1),(((*mem(PC+2)<<8)|*mem(PC+1))+Y)&0xFFFF, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+Y));
			break;
		case 0xA1: // ($aa,X)
			sprintf(str, "LDA ($%02X,X) @ %02X = %02X%02X = %02X", *memap(PC+1), 0xFF & *memap(PC+1)+X, *memap((uint8_t)(*memap(PC+1)+X+1)), *memap(0xFF & (*memap(PC+1)+X)), *memap(((*memap(0xFF & (*memap(PC+1)+X+1))) << 8) | *memap(0xFF & (*memap(PC+1)+X))));
			break;
		case 0xB1: // ($aa),Y
			sprintf(str, "LDA ($%02X),Y = %04X @ %04X = %02X",*mem(PC+1),(*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)), (((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)&0xFFFF, *memap((((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)) & 0xFF);
			break;
		// LDX
		case 0xA2:
			sprintf(str, "LDX #$%02X  ",*mem(PC+1));
			break;
		case 0xA6: // $aa
			sprintf(str, "LDX $%02X = %02X",*mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0xB6: // $aa,Y
			sprintf(str, "LDX $%02X,Y @ %02X = %02X",*mem(PC+1), (*mem(PC+1)+Y)&0xFF, *mem((*mem(PC+1)+Y) & 0xFF));
			break;
		case 0xAE: // $aaaa
			sprintf(str, "LDX $%02X%02X = %02X", *mem(PC+2), *mem(PC+1), *memap((*mem(PC+2) << 8) | *mem(PC+1)));
			break;
		case 0xBE: // $aaaa,Y
			sprintf(str, "LDX $%02X%02X,Y @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+Y, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+Y));
			break;
		// LDY
		case 0xA0:
			sprintf(str, "LDY #$%02X",*mem(PC+1));
			break;
		case 0xA4: // $aa
			sprintf(str, "LDY $%02X = %02X",*mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0xB4: // $aa,X
			sprintf(str, "LDY $%02X,X @ %02X = %02X",*mem(PC+1), (*mem(PC+1)+X)&0xFF, *mem((*mem(PC+1)+X) & 0xFF));
			break;
		case 0xAC: // $aaaa
			sprintf(str, "LDY $%02X%02X = %02X", *mem(PC+2), *mem(PC+1), *memap((*mem(PC+2) << 8) | *mem(PC+1)));
			break;
		case 0xBC: // $aaaa,X
			sprintf(str, "LDY $%02X%02X,X @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+X, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+X));
			break;
		// LSR
		case 0x4A: // A
			sprintf(str, "LSR A");
			break;
		case 0x46: // $aa
			sprintf(str, "LSR $%02X = %02X",*mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0x56: // $aa,X
			sprintf(str, "LSR $%02X,X @ %02X = %02X",*mem(PC+1), *mem(PC+1)+X, *mem((*mem(PC+1)+X) & 0xFF));
			break;
		case 0x4E: // $aaaa
			sprintf(str, "LSR $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2)<<8) | *mem(PC+1)));
			break;
		case 0x5E: // $aaaa,X
			sprintf(str, "LSR $%02X%02X,X @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+X, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+X));
			break;
		// NOP
		case 0xEA:
			sprintf(str, "NOP");
			break;
		// ORA
		case 0x09: // #aa
			sprintf(str, "ORA #$%02X",*mem(PC+1));
			break;
		case 0x05: // $aa
			sprintf(str, "ORA $%02X = %02X",*mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0x15: // $aa,X
			sprintf(str, "ORA $%02X,X @ %02X = %02X",*mem(PC+1), *mem(PC+1)+X, *mem((*mem(PC+1)+X) & 0xFF));
			break;
		case 0x0D: // $aaaa
			sprintf(str, "ORA $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2)<<8) | *mem(PC+1)));
			break;
		case 0x1D: // $aaaa,X
			sprintf(str, "ORA $%02X%02X,X @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+X, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+X));
			break;
		case 0x19: // $aaaa,Y
			sprintf(str, "ORA $%02X%02X,Y @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+Y, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+Y));
			break;
		case 0x01: // ($aa,X)
			sprintf(str, "ORA ($%02X,X) @ %02X = %02X%02X = %02X", *mem(PC+1), 0xFF & (*mem(PC+1)+X), *mem(0xFF & (*mem(PC+1)+X+1)), *mem(0xFF & (*mem(PC+1)+X)), *mem((*mem(0xFF & (*mem(PC+1)+X+1)) << 8) + *mem(0xFF & (*mem(PC+1)+X))));
			break;
		case 0x11: // ($aa),Y
			sprintf(str, "ORA ($%02X),Y = %04X @ %04X = %02X",*mem(PC+1),(*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)), (((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)&0xFFFF, *memap((((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)) & 0xFF);
			break;
		// PHA
		case 0x48:
			sprintf(str, "PHA");
			break;
		// PHP
		case 0x08:
			sprintf(str, "PHP");
			break;
		// PLA
		case 0x68:
			sprintf(str, "PLA");
			break;
		// PLP
		case 0x28:
			sprintf(str, "PLP");
			break;
		// ROL
		case 0x2A: // A
			sprintf(str, "ROL A");
			break;
		case 0x26: // $aa
			sprintf(str, "ROL $%02X = %02X", *mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0x36: // $aa,X
			sprintf(str, "ROL $%02X,X @ %02X = %02X",*mem(PC+1), *mem(PC+1)+X, *mem((*mem(PC+1)+X) & 0xFF));
			break;
		case 0x2E: // $aaaa
			sprintf(str, "ROL $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2)<<8) | *mem(PC+1)));
			break;
		case 0x3E: // $aaaa,X
			sprintf(str, "ROL $%02X%02X,X @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+X, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+X));
			break;
		// ROR
		case 0x6A: // A
			sprintf(str, "ROR A");
			break;
		case 0x66: // $aa
			sprintf(str, "ROR $%02X = %02X", *mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0x76: // $aa,X
			sprintf(str, "ROR $%02X,X @ %02X = %02X",*mem(PC+1), *mem(PC+1)+X, *mem((*mem(PC+1)+X) & 0xFF));
			break;
		case 0x6E: // $aaaa
			sprintf(str, "ROR $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2)<<8) | *mem(PC+1)));
			break;
		case 0x7E: // $aaaa,X
			sprintf(str, "ROR $%02X%02X,X @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+X, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+X));
			break;
		// RTI
		case 0x40:
			sprintf(str, "RTI");
			break;
		// RTS
		case 0x60:
			sprintf(str, "RTS");
			break;
		// SBC
		case 0xE9: // $aa
			sprintf(str, "SBC #$%02X", *mem(PC+1));
			break;
		case 0xE5: // $aa
			sprintf(str, "SBC $%02X = %02X", *mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0xF5: // $aa,X
			sprintf(str, "SBC $%02X,X @ %02X = %02X",*mem(PC+1), *mem(PC+1)+X, *mem((*mem(PC+1)+X) & 0xFF));
			break;
		case 0xED: // $aaaa
			sprintf(str, "SBC $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2)<<8) | *mem(PC+1)));
			break;
		case 0xFD: // $aaaa,X
			sprintf(str, "SBC $%02X%02X,X @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+X, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+X));
			break;
		case 0xF9: // $aaaa,Y
			sprintf(str, "SBC $%02X%02X,Y @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+Y, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+Y));
			break;
		case 0xE1: // ($aa,X)
			sprintf(str, "SBC ($%02X,X) @ %02X = %02X%02X = %02X", *mem(PC+1), 0xFF & (*mem(PC+1)+X), *mem(0xFF & (*mem(PC+1)+X+1)), *mem(0xFF & (*mem(PC+1)+X)), *mem((*mem(0xFF & (*mem(PC+1)+X+1)) << 8) + *mem(0xFF & (*mem(PC+1)+X))));
			break;
		case 0xF1: // ($aa),Y
			sprintf(str, "SBC ($%02X),Y = %04X @ %04X = %02X",*mem(PC+1),(*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)), (((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)&0xFFFF, *memap((((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)) & 0xFF);
			break;
		// SEC
		case 0x38:
			sprintf(str, "SEC");
			break;
		// SED
		case 0xF8:
			sprintf(str, "SED");
			break;
		// SEI
		case 0x78:
			sprintf(str, "SEI");
			//P|=I_FLAG;
			break;
		// STA
		case 0x85: // $aa
			sprintf(str, "STA $%02X = %02X", *mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0x95: // $aa,X
			sprintf(str, "STA $%02X,X @ %02X = %02X",*mem(PC+1), (*mem(PC+1)+X)&0xFF, *mem((*mem(PC+1)+X) & 0xFF));
			break;
		case 0x8D:{ // $aaaa	
				sprintf(str, "STA $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2)<<8) | *mem(PC+1)));
				//printf("STA $%02X%02X = %02X\n",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2)<<8) | *mem(PC+1)));
				break;
			}
		case 0x9D: // $aaaa,X
			sprintf(str, "STA $%02X%02X,X @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+X, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+X));
			break;
		case 0x99: // $aaaa,Y
			sprintf(str, "STA $%02X%02X,Y @ %04X = %02X", *mem(PC+2), *mem(PC+1),((*mem(PC+2)<<8)|*mem(PC+1))+Y, *memap(((*mem(PC+2)<<8)|*mem(PC+1))+Y));
			break;
		case 0x81: // ($aa,X)
			sprintf(str, "STA ($%02X,X) @ %02X = %02X%02X = %02X", *mem(PC+1), 0xFF & (*mem(PC+1)+X), *mem(0xFF & (*mem(PC+1)+X+1)), *mem(0xFF & (*mem(PC+1)+X)), *mem((*mem(0xFF & (*mem(PC+1)+X+1)) << 8) + *mem(0xFF & (*mem(PC+1)+X))));
			break;
		case 0x91: // ($aa),Y
			sprintf(str, "STA ($%02X),Y = %04X @ %04X = %02X",*mem(PC+1),(*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)), (((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)&0xFFFF, *memap((((*mem((*mem(PC+1)+1) & 0xFF)<<8) | *mem(*mem(PC+1)))+Y)) & 0xFF);
			break;
		// STX
		case 0x86: // $aa
			sprintf(str, "STX $%02X = %02X", *mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0x96: // $aa,Y
			sprintf(str, "STX $%02X,Y @ %02X = %02X",*mem(PC+1), (*mem(PC+1)+Y)&0xFF, *mem((*mem(PC+1)+Y) & 0xFF));
			break;
		case 0x8E: // $aaaa
			sprintf(str, "STX $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2) << 8) | *mem(PC+1)));
			break;
		// STY
		case 0x84: // $aa
			sprintf(str, "STY $%02X = %02X", *mem(PC+1), *mem(*mem(PC+1)));
			break;
		case 0x94: // $aa,X
			sprintf(str, "STY $%02X,X @ %02X = %02X",*mem(PC+1), (*mem(PC+1)+X)&0xFF, *mem((*mem(PC+1)+X) & 0xFF));
			break;
		case 0x8C: // $aaaa
			sprintf(str, "STY $%02X%02X = %02X",*mem(PC+2),*mem(PC+1), *memap((*mem(PC+2) << 8) | *mem(PC+1)));
			break;
		// TAX
		case 0xAA:
			sprintf(str, "TAX");
			break;
		// TAY
		case 0xA8:
			sprintf(str, "TAY");
			break;
		// TSX
		case 0xBA:
			sprintf(str, "TSX");
			break;
		// TXA
		case 0x8A:
			sprintf(str, "TXA");
			break;
		// TXS
		case 0x9A:
			sprintf(str, "TXS");
			break;
		// TYA
		case 0x98:
			sprintf(str, "TYA");
			break;
		default:
			fprintf(stderr, "ERROR: opcode not recognised: %02X\n\n", opcode);
			exit(-1);
	}
}

void pltt(int val, int* R, int* G, int* B){
	//
	//  I don't think I have considered palette mirroring yet
	//
	switch(val){
		case 0x00:
			*R=0x75;
			*G=0x75;
			*B=0x75;
			break;
		case 0x01:
			*R=0x27;
			*G=0x1B;
			*B=0x8F;
			break;
		case 0x02:
			*R=0x00;
			*G=0x00;
			*B=0xAB;
			break;
		case 0x03:
			*R=0x47;
			*G=0x00;
			*B=0x9F;
			break;
		case 0x04:
			*R=0x8F;
			*G=0x00;
			*B=0x77;
			break;
		case 0x05:
			*R=0xAB;
			*G=0x00;
			*B=0x13;
			break;
		case 0x06:
			*R=0xA7;
			*G=0x00;
			*B=0x00;
			break;
		case 0x07:
			*R=0x7F;
			*G=0x0B;
			*B=0x00;
			break;
		case 0x08:
			*R=0x43;
			*G=0x2F;
			*B=0x00;
			break;
		case 0x09:
			*R=0x00;
			*G=0x47;
			*B=0x00;
			break;
		case 0x0A:
			*R=0x00;
			*G=0x51;
			*B=0x00;
			break;
		case 0x0B:
			*R=0x00;
			*G=0x3F;
			*B=0x17;
			break;
		case 0x0C:
			*R=0x1B;
			*G=0x3F;
			*B=0x5F;
			break;
		case 0x0D:
		case 0x0E:
		case 0x0F:
			*R=0x00;
			*G=0x00;
			*B=0x00;
			break;
		case 0x10:
			*R=0xBC;
			*G=0xBC;
			*B=0xBC;
			break;
		case 0x11:
			*R=0x00;
			*G=0x73;
			*B=0xEF;
			break;
		case 0x12:
			*R=0x23;
			*G=0x3B;
			*B=0xEF;
			break;
		case 0x13:
			*R=0x83;
			*G=0x00;
			*B=0xF3;
			break;
		case 0x14:
			*R=0xBF;
			*G=0x00;
			*B=0xBF;
			break;
		case 0x15:
			*R=0xE7;
			*G=0x00;
			*B=0x5B;
			break;
		case 0x16:
			*R=0xDB;
			*G=0x2B;
			*B=0x00;
			break;
		case 0x17:
			*R=0xCB;
			*G=0x4F;
			*B=0x0F;
			break;
		case 0x18:
			*R=0x8B;
			*G=0x73;
			*B=0x00;
			break;
		case 0x19:
			*R=0x00;
			*G=0x97;
			*B=0x00;
			break;
		case 0x1A:
			*R=0x00;
			*G=0xAB;
			*B=0x00;
			break;
		case 0x1B:
			*R=0x00;
			*G=0x93;
			*B=0x3B;
			break;
		case 0x1C:
			*R=0x00;
			*G=0x83;
			*B=0x8B;
			break;
		case 0x1D:
		case 0x1E:
		case 0x1F:
			*R=0x00;
			*G=0x00;
			*B=0x00;
			break;
		case 0x20:
			*R=0xFF;
			*G=0xFF;
			*B=0xFF;
			break;
		case 0x21:
			*R=0x3F;
			*G=0xBF;
			*B=0xFF;
			break;
		case 0x22:
			*R=0x5F;
			*G=0x97;
			*B=0xFF;
			break;
		case 0x23:
			*R=0xA7;
			*G=0x8B;
			*B=0xFD;
			break;
		case 0x24:
			*R=0xF7;
			*G=0x7B;
			*B=0xFF;
			break;
		case 0x25:
			*R=0xFF;
			*G=0x77;
			*B=0xB7;
			break;
		case 0x26:
			*R=0xFF;
			*G=0x77;
			*B=0x63;
			break;
		case 0x27:
			*R=0xFF;
			*G=0x9B;
			*B=0x3B;
			break;
		case 0x28:
			*R=0xF3;
			*G=0xBF;
			*B=0x3F;
			break;
		case 0x29:
			*R=0x83;
			*G=0xD3;
			*B=0x13;
			break;
		case 0x2A:
			*R=0x4F;
			*G=0xDF;
			*B=0x4B;
			break;
		case 0x2B:
			*R=0x58;
			*G=0xF8;
			*B=0x98;
			break;
		case 0x2C:
			*R=0x00;
			*G=0xEB;
			*B=0xDB;
			break;
		case 0x2D:
		case 0x2E:
		case 0x2F:
			*R=0x00;
			*G=0x00;
			*B=0x00;
			break;
		case 0x30:
			*R=0xFF;
			*G=0xFF;
			*B=0xFF;
			break;
		case 0x31:
			*R=0xAB;
			*G=0xE7;
			*B=0xFF;
			break;
		case 0x32:
			*R=0xC7;
			*G=0xD7;
			*B=0xFF;
			break;
		case 0x33:
			*R=0xD7;
			*G=0xCB;
			*B=0xFF;
			break;
		case 0x34:
			*R=0xFF;
			*G=0xC7;
			*B=0xFF;
			break;
		case 0x35:
			*R=0xFF;
			*G=0xC7;
			*B=0xDB;
			break;
		case 0x36:
			*R=0xFF;
			*G=0xFB;
			*B=0xB3;
			break;
		case 0x37:
			*R=0xFF;
			*G=0xDB;
			*B=0xAB;
			break;
		case 0x38:
			*R=0xFF;
			*G=0xE7;
			*B=0xA3;
			break;
		case 0x39:
			*R=0xE3;
			*G=0xFF;
			*B=0xA3;
			break;
		case 0x3A:
			*R=0xAB;
			*G=0xF3;
			*B=0xBF;
			break;
		case 0x3B:
			*R=0xB3;
			*G=0xFF;
			*B=0xCF;
			break;
		case 0x3C:
			*R=0x9F;
			*G=0xFF;
			*B=0xF3;
			break;
		case 0x3D:
		case 0x3E:
		case 0x3F:
			*R=0x00;
			*G=0x00;
			*B=0x00;
			break;
	}
}

int ppu(){

	if(x2002read==1){
		PPUSTATUS|=0x80;
		x2002read=0;
	}
	if(x2004write==1){
		OAM[*mem(0x2003)]=*mem(0x2004);
		x2004write=0;
	}
	if(x2005write==1){
		if(addscrolllatch==0){
			Loopy_X=((Loopy_X & 0xFFF8) | (0x07 & PPUSCROLL));
			Loopy_T=((Loopy_T & 0xFFE0) | ((0xF8 & PPUSCROLL) >> 3));
		} else {
			Loopy_T=((Loopy_T & 0x8C1F) | ((0x07 & PPUSCROLL) << 12) | ((0xF8 & PPUSCROLL) << 2));
			//printf("Loopy_X=%04X  Loopy_T:%04X\n", Loopy_X, Loopy_T);
		}

		addscrolllatch=~addscrolllatch;
		x2005write=0;
	}
	if(x2006write==1){
		if(addscrolllatch==0){
			vradd=((0x3F00 & (PPUADDR<<8)) | (0x00FF & vradd));
		} else {
			vradd=((0x3F00 & vradd) | (0x00FF & PPUADDR));
		}

		x2006write=0;
		addscrolllatch=~addscrolllatch;
	}
	if(x2007write==1){
		//printf("VRAM ADDRESS: $%04X   PPUDATA=%02X\n", vradd, PPUDATA);
		*pmem(vradd)=PPUDATA;

		if((PPUCTRL & 0x04)!=0){
			vradd+=32;
		} else {
			vradd++;
		}
		x2007write=0;
	}
	if(x4014write==1){ // Perform DMA copy
		memcpy(OAM,memap(*memap(0x4014)*0x100),256*sizeof(uint8_t));
		x4014write=0;
	}

	PPUCTRL = *mem(0x2000);
	//printf("$2000: %02X\n", PPUCTRL);


	if((PPUCTRL & 0x10)!=0){
		//printf("Background is stored in pattern table $1000\n");
		bkgndadrs=0x1000;
	} else {
		bkgndadrs=0x0000;
		//printf("Background is stored in pattern table $0000\n");
	}

	if((PPUCTRL & 0x08)!=0){
		sprtadrs=0x1000;
	} else {
		sprtadrs=0x0000;
	}

	int i=0;

	PPUSTATUS|=0x80; // FUDGE VBLANK
	
	static uint32_t lasttime=0;
	uint32_t now;

	//PPUSTATUS^=(PPUSTATUS&0x40);
	//if(((PPUCTRL & 0x80)!=0) && nmiflag==0 && (now=SDL_GetTicks())>=(lasttime + (1000.0/60.0))){ // Generate an NMI at the start of a VBLANK
	if(((PPUCTRL & 0x80)!=0) && nmiflag==0){ // Generate an NMI at the start of a VBLANK
		PPUSTATUS&=~0x40; // Reset Sprite 0 detect flag
		
		//pad1=0; // reset the joypad flags at the start of each frame.
		//lasttime=now;

		/*
		for(i=0x0000; i<0x03C0; i++){
			draw_sprite(i%32, i/32, *pmem(i+0x2000)*16 + bkgndadrs);
		}
		for(i=0x0000; i<50; i++){
			draw_sprite(43, i, i*16);
			draw_sprite(46, i, (i*16)+0x1000);
		}*/

		if((PPUCTRL & 0x20)!=0){
			// sprite size is 8x8


		} else {
			// sprite size is 8x16
		}

		blarg();
		//PPUSTATUS|=0x40; // SERIOUS FUDGING

		SDL_RenderPresent(renderer);
		// A DELAY MAY BE REQUIRED
		nmiflag=1;
		nmi();
	}

	//draw_sprite(20, 20);
	//SDL_RenderPresent(renderer);
	//SDL_RenderPresent(renderer);

}

int online(uint8_t spr_y, uint8_t sl){
	if (sl>(spr_y+7) || sl<spr_y){
		return 0;
	}
	return 1;
}

int onpixel(uint8_t spr_x, uint8_t x){
	if (x>(spr_x+7) || x<spr_x){
		return 0;
	}
	return 1;
}

void blarg(){
	//int i,j,sl;
	int i,j;
	int sprt_x, sprt_y;
	int R;
	int G;
	int B;
	uint16_t pix;
	uint16_t attrib;

	static int sl=0;
	for(sl=0;sl<239;sl++){
		//  Find sprites that are on this line
		//  organise sprites in order of priority: both index number and bit 5 of sprite attribute 2
		//  get the 8 most high priority sprites
		//  Of these sprites, draw them from lowest priority to highest priority.
		int sprpnt=0;
		sprpnt=0;
		int spridx[8];
		for(i=0;i<64;i++){
				//if(i==0)
					//fprintf(stderr, "sprt_y=%02X\n",sprt_y);
			sprt_y=OAM[i*4]+1; // Remember that OAM[i*4] refers to the y coordinate of the top left of the sprite minus one.
			if(online(sprt_y,sl)!=0){
				spridx[sprpnt]=i;
				sprpnt++;
			}
			if(sprpnt>=9){  // PERHAPS THIS SHOULD BE >=9
				PPUSTATUS|=0x20;
				break;
			}

		}

		for(i=0;i<256;i++){

			if((6-(i%8))>=0){
				pix=((*pmem((*pmem(0x2000 + (sl/8)*32 + (i/8))*16 + (sl%8))+bkgndadrs + 8)>>(6-(i%8))) & 0x02) | ((*pmem((*pmem(0x2000 + (sl/8)*32 +(i/8))*16 + (sl%8))+bkgndadrs)>>(7-(i%8))) & 0x01);
			} else {
				pix=((*pmem((*pmem(0x2000 + (sl/8)*32 + (i/8))*16 + (sl%8))+bkgndadrs + 8)<<1) & 0x02) | ((*pmem((*pmem(0x2000 + (sl/8)*32 +(i/8))*16 + (sl%8))+bkgndadrs)) & 0x01);
			}
			attrib=(*pmem(0x23C0 + ((sl/32)*8) + (i/32)) >> (((sl/16)%2)*4)) >> (((i/16)%2)*2);
			pix|=((attrib&0x03)<<2);

			pltt(*pmem((pix&0xFF) | 0x3F00), &R, &G, &B);
			SDL_SetRenderDrawColor(renderer, R, G, B, 255);


			for(j=sprpnt-1;j>=0;j--){
				sprt_x=OAM[spridx[j]*4 + 3];
				if(onpixel(sprt_x, i)!=0){

					if((OAM[spridx[j]*4 + 2]&0x40)!=0){ // flip sprite horizontally
						i=2*sprt_x+7-i;
					}

					sprt_y=OAM[spridx[j]*4]+1; // Remember that OAM[i*4] refers to the y coordinate of the top left of the sprite minus one.
				
					pix=(((*pmem(OAM[spridx[j]*4+1]*16+(sl-sprt_y)+sprtadrs+8)>>(7-(i-sprt_x)))<<1) & 0x02) | ((*pmem(OAM[spridx[j]*4+1]*16+(sl-sprt_y)+sprtadrs)>>(7-(i-sprt_x))) & 0x01);  // I shift the more significant bit of the pixel right by up to seven, then left by one to prevent a negative shift.  Previously I just used >>(6-(i-sprt_x)), which causes a glitch when i=sprt_x+1 

					if(sprt_y>=(0xEF+1) && sprt_y<=(0xFF+1)){
						pix=0;  // Sprites are never displayed on the first scanline and cannot be partially off the top of the screen.
					}

					if(pix!=0){
						if(spridx[j]==0){
							PPUSTATUS|=0x40; // Sprite 0 hit detection - need to check background first.
						}

						attrib=(OAM[spridx[j]*4 + 2]&0x03);
						pix|=(attrib<<2);

						pltt(*pmem((pix&0xFF) | 0x3F10), &R, &G, &B);
						SDL_SetRenderDrawColor(renderer, R, G, B, 255);
					}
					

					if((OAM[spridx[j]*4 + 2]&0x40)!=0){ // flip sprite horizontally
						i=2*sprt_x+7-i;
					}

				}
			}


			SDL_RenderDrawPoint(renderer, i, sl);
		}
	}
	/*static uint32_t ticks;
	ticks=SDL_GetTicks();
	if (sl<239){
		sl++;
	}else{
		sl=0;
		while(1){
			if(SDL_GetTicks()>=(ticks+(1000.0/60.0))){
				ticks=SDL_GetTicks();
				break;
			}
		}
	}
	*/
}

int main(int argc, char *argv[]){

	thingy(argc, argv); // Parse the header file.

	MEM = (uint8_t*) malloc((0xFFFF+1)*sizeof(uint8_t));
	VRAM = (uint8_t*) malloc((0x3FFF+1)*sizeof(uint8_t));

	OAM = (uint8_t*) malloc(256*sizeof(uint8_t));
	
	// Ensure that memory is initialized - this may be a dubious fix though if the 6502 code is incorrectly accessing memory.
	int i;
	for(i=0;i<(0xFFFF+1);i++){
		MEM[i]=0;
	}
	for(i=0;i<(0x3FFF+1);i++){
		VRAM[i]=0;
	}
	for(i=0;i<256;i++){
		OAM[i]=0;
	}


	if(prg_rosze==2){
		memcpy(&MEM[0x8000], prg_rom_dat, 16384*prg_rosze);
	} else if (prg_rosze==1){ // only temporary
		memcpy(&MEM[0x8000], prg_rom_dat, 16384*prg_rosze);
		memcpy(&MEM[0xC000], prg_rom_dat, 16384*prg_rosze);
	}else{
		fprintf(stderr, "ERROR: only 2 16KB prg_rom banks are currently supported\n");
	}

	if(chr_rosze==1){
		memcpy(&VRAM[0x0000], chr_rom_dat, 8192*chr_rosze);
	} else {
		fprintf(stderr, "ERROR: only 1 8KB chr_rom bank is currently supported\n");
	}

	reset();
	count=0;

	prep_SDL();

	cpu_powerup();
	ppu_powerup();

	int done=0;
	//PC=0xC000;
	//P=0x24;
	while(!done){
		if(strobe==1){
			//fprintf(stderr, "hello chaps\n");
		}
		//pad1=0;
		SDL_Event event;
		while(SDL_PollEvent(&event)){
				switch(event.type){
					case SDL_QUIT:
						done=1;
						break;
					
					case SDL_KEYDOWN:
						switch(event.key.keysym.sym){
							case SDLK_LEFT:
								pad1|=PAD_LEFT;
								break;
							case SDLK_RIGHT:
								pad1|=PAD_RIGHT;
								break;
							case SDLK_UP:
								pad1|=PAD_UP;
								break;
							case SDLK_DOWN:
								pad1|=PAD_DOWN;
								break;
							case SDLK_z:
								pad1|=PAD_A;
								break;
							case SDLK_x:
								pad1|=PAD_B;
								break;
							case SDLK_RETURN:
								pad1|=PAD_START;
								break;
							case SDLK_RSHIFT:
								pad1|=PAD_SELECT;
								break;
						}
						
						break;
				}
		}
	

		//SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		//SDL_RenderClear(renderer);


		//rnd_spts(chr_rom_dat);


		char str[50];

		//printf("%04X  %02X   ", PC, *mem(PC));
		disasm(*mem(PC), str);
		//printf("%-32s", str);
		//printf("A:%02X X:%02X Y:%02X P:%02X SP:%02X\n", A, X, Y, P, SP);
		opdec(*mem(PC));
		
		ppu();
		PC++;

		count++;

		/*
		uint32_t ticks=SDL_GetTicks();
		if(count>=29780){
			while(1){
				if(SDL_GetTicks()>=(ticks+(1000.0/60.0)*10.0)){
					ticks=SDL_GetTicks();
					break;
				}
			}
			count=0;
		}*/

		//SDL_RenderPresent(renderer);

		//SDL_Delay(1);
	}

	printf("\nPC=%x mem(PC)=%x P=%x A=%x X=%X Y=%X\n", PC, *mem(PC), P, A, X, Y);
	printf("SP=%x\n", SP);
	printf("mem(PC+1)=%x\n", *mem(PC+1));
	printf("mem(PC+2)=%x\n", *mem(PC+2));
	printf("mem(PC+3)=%x\n", *mem(PC+3));
	printf("mem(PC+4)=%x\n", *mem(PC+4));
	
	return 0;
}

void reset(){
	PC = (*mem(0xFFFD) << 8) | *mem(0xFFFC);
	
	//printf("%02X %02X\n", *mem(0xFFFB), *mem(0xFFFA));
	//printf("%02X %02X\n", *mem(0xFFFD), *mem(0xFFFC));
	//printf("%02X %02X\n", *mem(0xFFFF), *mem(0xFFFE));
	//exit(-1);

	return;
}

void push(uint8_t data){
	*mem(0x100+SP)=data;
	SP--;

	return;
}

uint8_t pop(){
	SP++;
	return *mem(0x100+SP);
}

void irq(){

	if((P & I_FLAG) == 0){
		push(PC & 0xFF);
		push((PC >> 8) & 0xFF);
		push(P | 0x20);

		P|=I_FLAG;

		PC = (*mem(0xFFFF) << 8) + *mem(0xFFFE);
	}

	return;
}

void nmi(){
	//printf("NMI\n");
	// ignore interrupt disable flag.

	// if bit 7 of PPU control register 1 ($2000) is not clear:
	push((PC >> 8) & 0xFF);
	push(PC & 0xFF);
	push(P | 0x20);

	P|=I_FLAG;

	PC = (*mem(0xFFFB) << 8) + *mem(0xFFFA) - 1;

	return;
}

void rti(){

	P=((P & 0x30) | (pop() & 0xCF));
	PC=pop();
	PC|=(pop() << 8);
	PC--;

	nmiflag=0;

	return;
}

int draw_sprite(int x, int y, uint16_t pat){
	int i,j;
	int R;
	int G;
	int B;
	for(i=0;i<8;i++){
		for(j=0;j<8;j++){
			R=0;
			G=0;
			B=0;
			if(((*pmem((pat+j))&(0x80>>i))!=0) && ((*pmem(pat+j+8)&(0x80>>i))!=0)){
				B=255;
			} else if((*pmem((pat+j))&(0x80>>i))!=0){
				G=255;
			} else if((*pmem(pat+j+8)&(0x80>>i))!=0){
				R=255;
			}
			SDL_SetRenderDrawColor(renderer, R, G, B, 255);
			SDL_RenderDrawPoint(renderer,(x*8)+i,(y*8)+j);
		}
	}
}

int rnd_spts(unsigned char *chr_rom_dat){

	int i,j;
	int k=0;
	int l=0;
	for(i=0; i<512*8; i++){
		for(j=0; j<8; j++){
			if((chr_rom_dat[i+l] & (int)pow(2,j))==0 && (chr_rom_dat[i+l+8] & (int)pow(2,j))==0){
				SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			}
			else if((chr_rom_dat[i+l] & (int)pow(2,j))==0 && (chr_rom_dat[i+l+8] & (int)pow(2,j))!=0){
				SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
			}
			else if((chr_rom_dat[i+l] & (int)pow(2,j))!=0 && (chr_rom_dat[i+l+8] & (int)pow(2,j))==0){
				SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
			}
			else if((chr_rom_dat[i+l] & (int)pow(2,j))!=0 && (chr_rom_dat[i+l+8] & (int)pow(2,j))!=0){
				SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
			}
			SDL_RenderDrawPoint(renderer,(7-j)+8*(i/128),i%128);
		}
		k++;
		if(k==8){
			l+=8;
			k=0;
		}
	}
}

int prep_SDL(){
	surf = SDL_CreateRGBSurface(0, WIDTH, HEIGHT, 32, 0, 0, 0 ,0);

	if(SDL_Init(SDL_INIT_VIDEO) < 0){
		fprintf(stderr, "Failed to init SDL\n");
		exit(-1);
	}

	mainwindow = SDL_CreateWindow("jeffNES", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);

	if(!mainwindow){
		fprintf(stderr, "Failed to create SDL window\n");
		exit(-1);
	}

	renderer = SDL_CreateRenderer(mainwindow, -1, 0);

	SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);

	SDL_RenderClear(renderer);

	return 0;
}

uint8_t* pmem(uint16_t addrs){
	addrs&=0x3FFF;// Wrap around memory
	return &VRAM[addrs];
}

uint8_t* memread(uint16_t addrs){
	return mem(addrs);
}

uint8_t memwrite(uint16_t addrs, uint8_t data){
	

	if(addrs==0x4016){
		if((data&0x01)!=0){ // if strobe is high
			//printf("HEY GUYS\n");
			strobe=1;
			pad1real=pad1;
		} else {
			strobe=0;
			pad1=0;
		}
		return data; // don't actually write data to memory?
	}
	
	*mem(addrs)=data;

	return data;
}

uint8_t* mem(uint16_t addrs){
	static int joyread=0;

	if((addrs >= 0x0800) && (addrs<0x1000)){
		return &MEM[addrs-0x0800];
	}
	else if((addrs >= 0x1000) && (addrs<0x1800)){
		return &MEM[addrs-0x1000];
	}
	else if((addrs >= 0x1800) && (addrs<0x2000)){
		return &MEM[addrs-0x1800];
	}
	else{
		if(addrs==0x2002){ // CPU is reading PPU status register, so have to reset V-blank flag
			x2002read=1;
			addscrolllatch=0;
			return &PPUSTATUS;
		} else if(addrs==0x2004){
			x2004write=1;
			return &MEM[0x2004];
		} else if(addrs==0x2005){
			x2005write=1;
			return &PPUSCROLL;
		} else if(addrs==0x2006){
			x2006write=1;
			return &PPUADDR;
		} else if(addrs==0x2007){
			x2007write=1;
			return &PPUDATA;
		} else if(addrs==0x4014){
			x4014write=1;
			return &MEM[0x4014];
		} else if(addrs==0x4016){

			if(strobe==1){
				joyread=0;
			}

			switch(joyread){
				case 0:
					if((pad1real&PAD_A)!=0){
						//fprintf(stderr, "HI MUM!\n");
						MEM[0x4016]|=0x01;
						break;
					}
					MEM[0x4016]&=~0x01;
					break;
				case 1:
					if((pad1real&PAD_B)!=0){
						MEM[0x4016]|=0x01;
						break;
					}
					MEM[0x4016]&=~0x01;
					break;
				case 2:
					if((pad1real&PAD_SELECT)!=0){
						MEM[0x4016]|=0x01;
						break;
					}
					MEM[0x4016]&=~0x01;
					break;
				case 3:
					if((pad1real&PAD_START)!=0){
						MEM[0x4016]|=0x01;
						break;
					}
					MEM[0x4016]&=~0x01;
					break;
				case 4:
					if((pad1real&PAD_UP)!=0){
						MEM[0x4016]|=0x01;
						break;
					}
					MEM[0x4016]&=~0x01;
					break;
				case 5:
					if((pad1real&PAD_DOWN)!=0){
						MEM[0x4016]|=0x01;
						break;
					}
					MEM[0x4016]&=~0x01;
					break;
				case 6:
					if((pad1real&PAD_LEFT)!=0){
						MEM[0x4016]|=0x01;
						break;
					}
					MEM[0x4016]&=~0x01;
					break;
				case 7:
					if((pad1real&PAD_RIGHT)!=0){
						fprintf(stderr, "pressed RIGHT\n");
						MEM[0x4016]|=0x01;
						break;
					}
					MEM[0x4016]&=~0x01;
					break;
				case 8: // START OF PLAYER 3 DATA (if connected)
					MEM[0x4016]&=~0x01;
					break;
				case 9:
					MEM[0x4016]&=~0x01;
					break;
				case 10:
					MEM[0x4016]&=~0x01;
					break;
				case 11:
					MEM[0x4016]&=~0x01;
					break;
				case 12:
					MEM[0x4016]&=~0x01;
					break;
				case 13:
					MEM[0x4016]&=~0x01;
					break;
				case 14:
					MEM[0x4016]&=~0x01;
					break;
				case 15:
					MEM[0x4016]&=~0x01;
					break; // END OF PLAYER 3 DATA (if connected)
				case 16:
					//fprintf(stderr, "HI MUM!\n");
					//MEM[0x4016]|=0x01;
					MEM[0x4016]&=~0x01;
					break;
				case 17:
					//MEM[0x4016]|=0x01;
					MEM[0x4016]&=~0x01;
					break;
				case 18:
					//MEM[0x4016]|=0x01;
					MEM[0x4016]&=~0x01;
					break;
				case 19:
					//MEM[0x4016]|=0x01;
					MEM[0x4016]&=~0x01;
					break;
				case 20:
					//fprintf(stderr, "HI MUM!\n");
					MEM[0x4016]&=~0x01;
					break;
				case 21:
					MEM[0x4016]&=~0x01;
					break;
				case 22:
					MEM[0x4016]&=~0x01;
					break;
				case 23:
					MEM[0x4016]&=~0x01;
					break;
			}
			//fprintf(stderr,"%X\n", MEM[0x4016]);
			joyread++;
			//joyread=0;
			if(joyread>23){
				joyread=0;
			}
		}
		return &MEM[addrs];
	}
}

uint8_t* memap(uint16_t addrs){

	if((addrs >= 0x0800) && (addrs<0x1000)){
		return &MEM[addrs-0x0800];
	}
	else if((addrs >= 0x1000) && (addrs<0x1800)){
		return &MEM[addrs-0x1000];
	}
	else if((addrs >= 0x1800) && (addrs<0x2000)){
		return &MEM[addrs-0x1800];
	}
	else{
		if(addrs==0x2002){ // CPU is reading PPU status register, so have to reset V-blank flag
			return &PPUSTATUS;
		} else if(addrs==0x2005){
			return &PPUSCROLL;
		} else if(addrs==0x2006){
			return &PPUADDR;
		} else if(addrs==0x2007){
			return &PPUDATA;
		}
		return &MEM[addrs];
	}
}

int cpu_powerup(){
	P=0x34; //(IRQ disabled)*
	A=0;
	X=0;
	Y=0;
	SP=0xFD;
	int i;
	for(i=0; i<=0x07FF; i++){
		MEM[i]=0xFF;
	}
	MEM[0x0008]=0xF7;
	MEM[0x0009]=0xEF;
	MEM[0x000a]=0xDF;
	MEM[0x000f]=0xBF;

	MEM[0x4017]=0x00;
	MEM[0x4015]=0x00;

	for(i=0x4000; i<=0x400F; i++){
		MEM[i]=0x00;
	}

	return 0;
}

int cpu_reset(){
	SP-=3;
	P|=I_FLAG;
	MEM[0x4015]=0;

	return 0;
}

uint8_t sub(uint8_t a, uint8_t b){
	uint16_t twosb=((0xFF&~(uint16_t)b)+1);
	uint16_t tot=(uint16_t)a+twosb;

	//printf("%X\n", tot);
	if ((tot&0x0100)!=0){
		P|=C_FLAG;
	} else {
		P&=~C_FLAG;
	}
	tot&=0xFF;
	znchk(tot);
	return tot;
}

uint8_t subc(uint8_t a, uint8_t b){
	uint8_t carry;
	if((P&C_FLAG)!=0){
		carry=1;
	} else {
		carry=0;
	}

	uint16_t onesb=(0xFF&~(uint16_t)b);
	uint16_t tot=(uint16_t)a+onesb+carry;

	int aneg=0;
	int onesbneg=0;
	if((a&0x80)!=0){
		aneg=1;
	}
	if((onesb&0x80)!=0){
		onesbneg=1;
	}

	//printf("%X\n", tot);
	if ((tot&0x0100)!=0){
		P|=C_FLAG;
	} else {
		P&=~C_FLAG;
	}
	tot&=0xFF;

	if((tot&0x80)!=0){
		if(aneg==0 && onesbneg==0){
			P|=V_FLAG;
		} else {
			P&=~V_FLAG;
		}
	} else {
		if(aneg==1 && onesbneg==1){
			P|=V_FLAG;
		} else {
			P&=~V_FLAG;
		}
	}

	znchk(tot);
	return tot;
}

uint16_t rel(uint8_t x){
	if((x & N_FLAG)!=0){ 
		return PC-((0xFF&~x)+1);
	}
	else{
		return PC+x;
	}
}

void opdec(uint8_t opcode){
	switch (opcode){
		// ADC
		case 0x69: // #aa
			A=addc(A,fetch());
			break;
		case 0x65: // $aa
			A=addc(A,*mem(fetch()));
			break;
		case 0x75: // $aa,X
			A=addc(A,*mem((fetch() + X)));
			break;
		case 0x6D:{ // $aaaa
				uint16_t tmp = fetch();
				A=addc(A,*mem(tmp | ((uint16_t)fetch() << 8)));
				break;
			}
		case 0x7D:{ // $aaaa,X
				uint16_t tmp = fetch();
				A=addc(A, *mem((tmp | ((uint16_t)fetch() << 8)) + X));
				znchk(A);
				break;
			}
		case 0x79:{ // $aaaa,Y
				uint16_t tmp = fetch();
				A=addc(A, *mem((tmp | ((uint16_t)fetch() << 8)) + Y));
				znchk(A);
				break;
			}
		case 0x61:{ // ($aa,X)
				uint8_t tmp = fetch();
				A=addc(A, *mem((*mem(0xFF & (tmp+X+1)) << 8)+*mem(0xFF & (tmp+X))));
				znchk(A);
				break;
			}
		case 0x71:{ // ($aa),Y
				uint8_t tmp = fetch();
				A=addc(A,*mem((((*mem((tmp+1) & 0xFF) << 8) | (*mem(tmp) & 0xFF))+Y) & 0xFFFF));
				znchk(A);
				break;
			}
		// AND
		case 0x29: // #$aa
			A&=fetch();
			znchk(A);
			break;
		case 0x25: // $aa
			A&=*mem(fetch());
			znchk(A);
			break;
		case 0x35: // $aa,X
			A&=*mem((fetch() + X));
			znchk(A);
			break;
		case 0x2D:{ // $aaaa
				uint16_t tmp = fetch();
				A&=*mem(tmp | ((uint16_t)fetch() << 8));
				znchk(A);
				break;
			}
		case 0x3D:{ // aaaa,X
				uint16_t tmp = fetch();
				A&=*mem((tmp | ((uint16_t)fetch() << 8)) + X);
				znchk(A);
				break;
			}
		case 0x39:{ // aaaa,Y
				uint16_t tmp = fetch();
				A&=*mem((tmp | ((uint16_t)fetch() << 8)) + Y);
				znchk(A);
				break;
			}
		case 0x21:{ // (aa,X)
				uint8_t tmp = fetch();
				A&=*mem((*mem(0xFF & (tmp+X+1)) << 8)+*mem(0xFF & (tmp+X)));
				znchk(A);
				break;
			}
		case 0x31:{ // (aa),Y
				uint8_t tmp = fetch();
				A&=*mem((((*mem((tmp+1) & 0xFF) << 8) | (*mem(tmp) & 0xFF))+Y) & 0xFFFF);
				znchk(A);
				break;
			}
		// ASL
		case 0x0A: // A
			if((A&0x80)!=0){
				P|=C_FLAG;
			} else {
				P&=~C_FLAG;
			}
			A=(A<<1);
			znchk(A);
			break;
		case 0x06:{ // $aa
				uint8_t tmp = fetch();
				if((*mem(tmp)&0x80)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=(*mem(tmp)<<1);
				znchk(*mem(tmp));
				break;
			}
		case 0x16:{ // $aa,X
				uint8_t tmp=((fetch()+X) & 0xFF);
				if((*mem(tmp)&0x80)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=(*mem(tmp)<<1);
				znchk(*mem(tmp));
				break;
			}
		case 0x0E:{ // $aaaa
				uint16_t tmp = fetch();
				tmp=(tmp | ((uint16_t)fetch() << 8));
				if((*mem(tmp)&0x80)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=(*mem(tmp)<<1);
				znchk(*mem(tmp));
				break;
			}
		case 0x1E:{ // $aaaa,X
				uint16_t tmp = fetch();
				tmp=(tmp | ((uint16_t)fetch() << 8)) + X;
				if((*mem(tmp)&0x80)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=(*mem(tmp)<<1);
				znchk(*mem(tmp));
				break;
			}
		// BCC
		case 0x90:{
				uint8_t tmp = fetch();
				if((P&C_FLAG)==0){
					PC=rel(tmp);
				}
				break;
			}
		// BCS
		case 0xB0:{
				uint8_t tmp = fetch();
				if((P&C_FLAG)!=0){
					PC=rel(tmp);
				}
				break;
			}
		// BEQ
		case 0xF0:{
				uint8_t tmp = fetch();
				if((P&Z_FLAG)!=0){
					PC=rel(tmp);
				}
				break;
			}
		// BIT
		case 0x24:{
				uint8_t tmp;
				tmp = fetch();
				tmp=*mem(tmp);

				if((A & tmp) == 0){
					P |= Z_FLAG;
				}else{
					P &= ~Z_FLAG;
				}

				if((tmp & V_FLAG)!=0){
					P |= V_FLAG;
				}else{
					P &= ~V_FLAG;
				}

				if((tmp & N_FLAG)!=0){
					P |= N_FLAG;
				}else{
					P &= ~N_FLAG;
				}

				break;
			  }
		case 0x2C:{
				uint8_t tmp;
				tmp = fetch();
				tmp=*mem((fetch() << 8) | tmp);
			

				if((A & tmp) == 0){
					P |= Z_FLAG;
				}else{
					P &= ~Z_FLAG;
				}

				if((tmp & V_FLAG)!=0){
					P |= V_FLAG;
				}else{
					P &= ~V_FLAG;
				}

				if((tmp & N_FLAG)!=0){
					P |= N_FLAG;
				}else{
					P &= ~N_FLAG;
				}

				break;
			  }
		// BMI
		case 0x30:{
				uint8_t tmp;
				tmp = fetch();
				if((P & N_FLAG)!=0){
					PC=rel(tmp);
				}
				break;
			  }
		// BNE
		case 0xD0:{
				uint8_t tmp;
				tmp = fetch();
				if((P & Z_FLAG)==0){
					PC=rel(tmp);
				}
				break;
			  }
		// BPL
		case 0x10:{
				uint8_t tmp;
				tmp = fetch();
				if((P & N_FLAG)==0){
					PC=rel(tmp);
				}
				break;
			}
		// BRK
		case 0x00:
			fprintf(stderr, "ERROR: opcode not recognised");
			exit(-1);
			break;
		// BVC
		case 0x50:{
				uint8_t tmp;
				tmp = fetch();
				if((P & V_FLAG)==0){
					PC=rel(tmp);
				}
				break;
			}
		// BVS
		case 0x70:{
				uint8_t tmp;
				tmp = fetch();
				if((P & V_FLAG)!=0){
					PC=rel(tmp);
				}
				break;
			}
		// CLC
		case 0x18:
			P&=~C_FLAG;
			break;
		// CLD
		case 0xD8:
			P&=~D_FLAG;
			break;
		// CLI
		case 0x58:
			P&=~I_FLAG;
			break;
		// CLV
		case 0xB8:
			P&=~V_FLAG;
			break;
		// CMP
		case 0xC9:{ // #aa
				uint16_t M = fetch();
				sub(A, M);
				break;
			}
		case 0xC5:{ // $aa
                                sub(A,*mem(fetch()));
				break;
			}
		case 0xD5:{ // $aa,X
				uint8_t tmp=((fetch()+X) & 0xFF);
				sub(A, *mem(tmp));
				break;
			}
		case 0xCD:{ // $aaaa
	      			uint16_t tmp = fetch();
                                tmp=(((uint16_t)fetch() << 8) | tmp);
				sub(A,*mem(tmp));
				break;
			}
		case 0xDD:{ // $aaaa,X
				uint16_t tmp = fetch();
				sub(A, *mem((tmp | ((uint16_t)fetch() << 8)) + X));
				break;
			}
		case 0xD9:{ // $aaaa,Y
				uint16_t tmp = fetch();
				sub(A, *mem((tmp | ((uint16_t)fetch() << 8)) + Y));
				break;
			}
		case 0xC1:{ // ($aa,X)
				uint8_t tmp = fetch();
				sub(A, *mem((*mem(0xFF & (tmp+X+1)) << 8)+*mem(0xFF & (tmp+X))));
				break;
			}
		case 0xD1:{ // ($aa),Y
				uint8_t tmp = fetch();
				sub(A, *mem((((*mem((tmp+1) & 0xFF) << 8) | (*mem(tmp) & 0xFF))+Y) & 0xFFFF));
				break;
			}
		// CPX
		case 0xE0:{ // #aa
				uint16_t M = fetch();
				sub(X, M);
				break;
			}
		case 0xE4:{ // $aa
				sub(X, *mem(fetch()));
				break;
			}
		case 0xEC:{ // $aaaa
	      			uint16_t tmp = fetch();
                                tmp=(((uint16_t)fetch() << 8) | tmp);
				sub(X,*mem(tmp));
				break;
			}
		// CPY
		case 0xC0:{ // #aa
				uint16_t M = fetch();
				sub(Y, M);
				break;
			}
		case 0xC4:{ // $aa
				sub(Y, *mem(fetch()));
				break;
			}
		case 0xCC:{ // $aaaa
	      			uint16_t tmp = fetch();
                                tmp=(((uint16_t)fetch() << 8) | tmp);
				sub(Y,*mem(tmp));
				break;
			}
		// DEC
		case 0xC6:{ // $aa
	      			uint16_t tmp = fetch();
                                *mem(tmp)=*mem(tmp)-1;  // UNSURE IF THIS IS SUFFICIENT
				znchk(*mem(tmp));
				break;
			}
		case 0xD6:{ // $aa,X
				uint8_t tmp=((fetch()+X) & 0xFF);
                                *mem(tmp)=*mem(tmp)-1;  // UNSURE IF THIS IS SUFFICIENT
				znchk(*mem(tmp));
				break;
			}
		case 0xCE:{ // $aaaa
	      			uint16_t tmp = fetch();
                                tmp=(((uint16_t)fetch() << 8) | tmp);
                                *mem(tmp)=*mem(tmp)-1;  // UNSURE IF THIS IS SUFFICIENT
				znchk(*mem(tmp));
				break;
			}
		case 0xDE:{ // $aaaa,X
				uint16_t tmp = fetch();
				tmp=(tmp | ((uint16_t)fetch() << 8)) + X;
                                *mem(tmp)=*mem(tmp)-1;  // UNSURE IF THIS IS SUFFICIENT
				znchk(*mem(tmp));
				break;
			}
		// DEX
		case 0xCA:
			X--;
			znchk(X);
			break;
		// DEY
		case 0x88:
			Y--;
			znchk(Y);
			break;
		// EOR
		case 0x49:{ // #aa
				uint8_t tmp=fetch();
				A=A^tmp;
				znchk(A);
				break;
			}
		case 0x45:{ // $aa
				uint8_t tmp=fetch();
				A=A^*mem(tmp);
				znchk(A);
				break;
			}
		case 0x55:{ // $aa,X
				uint8_t tmp=((fetch()+X) & 0xFF);
				A^=*mem(tmp);
				znchk(A);
				break;
			}
		case 0x4D:{ // $aaaa
	      			uint16_t tmp = fetch();
                                tmp=(((uint16_t)fetch() << 8) | tmp);
                                A^=*mem(tmp);
				znchk(A);
				break;
			}
		case 0x5D:{ // $aaaa,X
				uint16_t tmp = fetch();
				A^=*mem((tmp | ((uint16_t)fetch() << 8)) + X);
				znchk(A);
				break;
			}
		case 0x59:{ // $aaaa,Y
				uint16_t tmp = fetch();
				A^=*mem((tmp | ((uint16_t)fetch() << 8)) + Y);
				znchk(A);
				break;
			}
		case 0x41:{ // ($aa,X)
				uint8_t tmp = fetch();
				A^=*mem((*mem(0xFF & (tmp+X+1)) << 8)+*mem(0xFF & (tmp+X)));
				znchk(A);
				break;
			}
		case 0x51:{ // ($aa),Y
				uint8_t tmp = fetch();
				A^=*mem((((*mem((tmp+1) & 0xFF) << 8) | (*mem(tmp) & 0xFF))+Y) & 0xFFFF);
				znchk(A);
				break;
			}
		// INC
		case 0xE6:{ // $aa
	      			uint16_t tmp = fetch();
                                *mem(tmp)=*mem(tmp)+1;  // UNSURE IF THIS IS SUFFICIENT
				znchk(*mem(tmp));
				break;
			}
		case 0xF6:{ // $aa,X
				uint8_t tmp=((fetch()+X) & 0xFF);
                                *mem(tmp)=*mem(tmp)+1;  // UNSURE IF THIS IS SUFFICIENT
				znchk(*mem(tmp));
				break;
			}
		case 0xEE:{ // $aaaa
	      			uint16_t tmp = fetch();
                                tmp=(((uint16_t)fetch() << 8) | tmp);
                                *mem(tmp)=*mem(tmp)+1;  // UNSURE IF THIS IS SUFFICIENT
				znchk(*mem(tmp));
				break;
			}
		case 0xFE:{ // $aaaa,X
				uint16_t tmp = fetch();
				tmp=(tmp | ((uint16_t)fetch() << 8)) + X;
                                *mem(tmp)=*mem(tmp)+1;  // UNSURE IF THIS IS SUFFICIENT
				znchk(*mem(tmp));
				break;
			}
		// INX
		case 0xE8:
			X++;
			znchk(X);
			break;
		// INY
		case 0xC8:
			Y++;
			znchk(Y);
			break;
		// JMP
		case 0x4C:{ //$aaaa
	      			uint16_t tmp = fetch();
                                PC=(((uint16_t)fetch() << 8) | tmp)-1;
				break;
			}
		case 0x6C:{ // ($aaaa) 
	      			uint16_t tmp = fetch();
				if(tmp==0xFF){ // An original 6502 does not correctly fetch the target address if the indirect vector falls on a page boundary!
					tmp=(((uint16_t)fetch() << 8) | tmp);
					PC=(((uint16_t)*mem(tmp & 0xFF00) << 8) | *mem(tmp))-1;
					break;
				}
                                tmp=(((uint16_t)fetch() << 8) | tmp);
                                PC=(((uint16_t)*mem(tmp+1) << 8) | *mem(tmp))-1;
				break;
			}
		// JSR
		case 0x20:{
				push((uint8_t)(((PC+2) >> 8) & 0xFF));
				push((uint8_t)(0xFF & (PC+2)));
	      			uint16_t tmp = fetch();
				PC=(((uint16_t)fetch() << 8) | tmp)-1;
				break;
			}
		// LDA
		case 0xA9:
			A=fetch();
			znchk(A);
			break;
		case 0xA5:{ // $aa
                                A=*mem(fetch());
				znchk(A);
				break;
			}
		case 0xB5:{ // $aa,X
				uint8_t tmp=((fetch()+X) & 0xFF);
				A=*mem(tmp);
				znchk(A);
				break;
			}
		case 0xAD:{ // $aaaa
	      			uint16_t tmp = fetch();
                                A=*mem(tmp | ((uint16_t)fetch() << 8));
				znchk(A);
				break;
			}
		case 0xBD:{ // $aaaa,X
				uint16_t tmp = fetch();
				A=*mem(((tmp | ((uint16_t)fetch() << 8)) + X) & 0xFFFF);
				znchk(A);
				break;
			}
		case 0xB9:{ // $aaaa,Y
				uint16_t tmp = fetch();
				A=*mem(((tmp | ((uint16_t)fetch() << 8)) + Y) & 0xFFFF);
				znchk(A);
				break;
			}
		case 0xA1:{ // ($aa,X)
				uint8_t tmp = fetch();
				A=*mem((*mem(0xFF & (tmp+X+1)) << 8)+*mem(0xFF & (tmp+X)));
				znchk(A);
				break;
			}
		case 0xB1:{ // ($aa),Y
				uint8_t tmp = fetch();
				A=*mem((((*mem((tmp+1) & 0xFF) << 8) | (*mem(tmp) & 0xFF))+Y) & 0xFFFF);
				znchk(A);
				break;
			}
		// LDX
		case 0xA2: // #$aa
			X=fetch();
			znchk(X);
			break;
		case 0xA6:{ // $aa
				X=*mem(fetch());
				znchk(X);
				break;
			}
		case 0xB6:{ // $aa,Y
				uint8_t tmp=((fetch()+Y) & 0xFF);
				X=*mem(tmp);
				znchk(X);
				break;
			}
		case 0xAE:{ // $aaaa
				uint16_t tmp = fetch();
				X=*mem(((uint16_t)fetch() << 8) | tmp);
				znchk(X);
				break;
			}
		case 0xBE:{ // $aaaa,Y
				uint16_t tmp = fetch();
				X=*mem((((uint16_t)fetch() << 8) | tmp) + Y);
				znchk(X);
				break;
			}
		// LDY
		case 0xA0:
			// LDY #$aa
			Y=fetch();
			znchk(Y);
			break;
		case 0xA4:{ // $aa
				Y=*mem(fetch());
				znchk(Y);
				break;
			}
		case 0xB4:{ // $aa,X
				uint8_t tmp=((fetch()+X) & 0xFF);
				Y=*mem(tmp);
				znchk(Y);
				break;
			}
		case 0xAC:{ // $aaaa
				uint16_t tmp = fetch();
				Y=*mem(((uint16_t)fetch() << 8) | tmp);
				znchk(Y);
				break;
			}
		case 0xBC:{ // $aaaa,X
				uint16_t tmp = fetch();
				Y=*mem((((uint16_t)fetch() << 8) | tmp) + X);
				znchk(Y);
				break;
			}
		// LSR
		case 0x4A:
			if((A&0x01)!=0){
				P|=C_FLAG;
			} else {
				P&=~C_FLAG;
			}
			A=A>>1;
			znchk(A);
			break;
		case 0x46:{ // $aa
				uint8_t tmp=fetch();
				if((*mem(tmp)&0x01)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=*mem(tmp)>>1;
				znchk(*mem(tmp));
				break;
			}
		case 0x56:{ // $aa,X
				uint8_t tmp=((fetch()+X) & 0xFF);
				if((*mem(tmp)&0x01)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=*mem(tmp)>>1;
				znchk(*mem(tmp));
				break;
			}
		case 0x4E:{ // $aaaa
				uint16_t tmp = fetch();
				tmp=(((uint16_t)fetch() << 8) | tmp);
				if((*mem(tmp)&0x01)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=*mem(tmp)>>1;
				znchk(*mem(tmp));
				break;
			}
		case 0x5E:{ // $aaaa,X
				uint16_t tmp = fetch();
				tmp=(((uint16_t)fetch() << 8) | tmp) + X;
				if((*mem(tmp)&0x01)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=*mem(tmp)>>1;
				znchk(*mem(tmp));
				break;
			}
		// NOP
		case 0xEA:
			break;
		// ORA
		case 0x09:{ // #aa
				uint8_t tmp=fetch();
				A=A|tmp;
				znchk(A);
				break;
			}
		case 0x05:{ // $aa
				uint8_t tmp=fetch();
				A=A|*mem(tmp);
				znchk(A);
				break;
			}
		case 0x15:{ // $aa,X
				uint16_t tmp = ((fetch()+X) & 0xFF);
				A=A|*mem(tmp);
				znchk(A);
				break;
			}
		case 0x0D:{ // $aaaa
				uint16_t tmp = fetch();
				A|=*mem(((uint16_t)fetch() << 8) | tmp);
				znchk(A);
				break;
			}
		case 0x1D:{ // $aaaa,X
				uint16_t tmp = fetch();
				A|=*mem((((uint16_t)fetch() << 8) | tmp) + X);
				znchk(A);
				break;
			}
		case 0x19:{ // $aaaa,Y
				uint16_t tmp = fetch();
				A|=*mem((((uint16_t)fetch() << 8) | tmp) + Y);
				znchk(A);
				break;
			}
		case 0x01:{ // ($aa,X)
				uint8_t tmp = fetch();
				A|=*mem((*mem(0xFF & (tmp+X+1)) << 8)+*mem(0xFF & (tmp+X)));
				znchk(A);
				break;
			}
		case 0x11:{ // ($aa),Y
				uint8_t tmp = fetch();
				A|=*mem((((*mem((tmp+1) & 0xFF) << 8) | (*mem(tmp) & 0xFF))+Y) & 0xFFFF);
				znchk(A);
				break;
			}
		// PHA
		case 0x48:
			push(A);
			break;
		// PHP
		case 0x08:
			push(P | 0x30);
			break;
		// PLA
		case 0x68:
			A=pop();
			znchk(A);
			break;
		// PLP
		case 0x28:
			P=((P & 0x30) | (pop() & 0xCF));
			break;
		// ROL A
		case 0x2A:{
				uint8_t oldc;
				if((P&C_FLAG)!=0){
					oldc=0x01;
				} else {
					oldc=0x00;
				}

				if((A&0x80)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				A=(A<<1);
				A|=oldc;
				znchk(A);
				break;
			}
		case 0x26:{ // $aa
				uint16_t tmp = fetch();
				uint8_t oldc;
				if((P&C_FLAG)!=0){
					oldc=0x01;
				} else {
					oldc=0x00;
				}

				if((*mem(tmp)&0x80)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=(*mem(tmp)<<1);
				*mem(tmp)|=oldc;
				znchk(*mem(tmp));
				break;
			}
		case 0x36:{ // $aa,X
				uint16_t tmp = ((fetch()+X) & 0xFF);
				uint8_t oldc;
				if((P&C_FLAG)!=0){
					oldc=0x01;
				} else {
					oldc=0x00;
				}

				if((*mem(tmp)&0x80)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=(*mem(tmp)<<1);
				*mem(tmp)|=oldc;
				znchk(*mem(tmp));
				break;
			}
		case 0x2E:{ // $aaaa
				uint16_t tmp = fetch();
				tmp=(((uint16_t)fetch() << 8) | tmp);
				uint8_t oldc;
				if((P&C_FLAG)!=0){
					oldc=0x01;
				} else {
					oldc=0x00;
				}

				if((*mem(tmp)&0x80)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=(*mem(tmp)<<1);
				*mem(tmp)|=oldc;
				znchk(*mem(tmp));
				break;
			}
		case 0x3E:{ // $aaaa,X
				uint16_t tmp = fetch();
				tmp=((((uint16_t)fetch() << 8) | tmp) + X);

				uint8_t oldc;
				if((P&C_FLAG)!=0){
					oldc=0x01;
				} else {
					oldc=0x00;
				}

				if((*mem(tmp)&0x80)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=(*mem(tmp)<<1);
				*mem(tmp)|=oldc;
				znchk(*mem(tmp));
				break;
			}
		// ROR
		case 0x6A:{ // A
				uint8_t oldc;
				if((P&C_FLAG)!=0){
					oldc=0x80;
				} else {
					oldc=0x00;
				}

				if((A&0x01)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				A=(A>>1);
				A|=oldc;
				znchk(A);
				break;
			}
		case 0x66:{ // $aa
				uint16_t tmp = fetch();
				uint8_t oldc;
				if((P&C_FLAG)!=0){
					oldc=0x80;
				} else {
					oldc=0x00;
				}

				if((*mem(tmp)&0x01)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=(*mem(tmp)>>1);
				*mem(tmp)|=oldc;
				znchk(*mem(tmp));
				break;
			}
		case 0x76:{ // $aa,X
				uint16_t tmp = ((fetch()+X) & 0xFF);
				uint8_t oldc;
				if((P&C_FLAG)!=0){
					oldc=0x80;
				} else {
					oldc=0x00;
				}

				if((*mem(tmp)&0x01)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=(*mem(tmp)>>1);
				*mem(tmp)|=oldc;
				znchk(*mem(tmp));
				break;
			}
		case 0x6E:{ // $aaaa
				uint16_t tmp = fetch();
				tmp=(((uint16_t)fetch() << 8) | tmp);
				uint8_t oldc;
				if((P&C_FLAG)!=0){
					oldc=0x80;
				} else {
					oldc=0x00;
				}

				if((*mem(tmp)&0x01)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=(*mem(tmp)>>1);
				*mem(tmp)|=oldc;
				znchk(*mem(tmp));
				break;
			}
		case 0x7E:{ // $aaaa,X
				uint16_t tmp = fetch();
				tmp=((((uint16_t)fetch() << 8) | tmp) + X);

				uint8_t oldc;
				if((P&C_FLAG)!=0){
					oldc=0x80;
				} else {
					oldc=0x00;
				}

				if((*mem(tmp)&0x01)!=0){
					P|=C_FLAG;
				} else {
					P&=~C_FLAG;
				}
				*mem(tmp)=(*mem(tmp)>>1);
				*mem(tmp)|=oldc;
				znchk(*mem(tmp));
				break;
			}
		// RTI
		case 0x40:
			rti();
			break;
		// RTS
		case 0x60:{
				PC=pop();
				PC|=(pop() << 8);
				break;
			}
		// SBC
		case 0xE9:{ // #aa
				uint16_t tmp = fetch();
				A=subc(A, tmp);
				break;
			}
		case 0xE5:{ // $aa
				uint16_t tmp = fetch();
				A=subc(A, *mem(tmp));
				break;
			}
		case 0xF5:{ // $aa,X
				uint16_t tmp = ((fetch()+X) & 0xFF);
				A=subc(A, *mem(tmp));
				break;
			}
		case 0xED:{ // $aaaa
				uint16_t tmp = fetch();
				A=subc(A,*mem(((uint16_t)fetch() << 8) | tmp));
				break;
			}
		case 0xFD:{ // $aaaa,X
				uint16_t tmp = fetch();
				A=subc(A, *mem((((uint16_t)fetch() << 8) | tmp) + X));
				break;
			}
		case 0xF9:{ // $aaaa,Y
				uint16_t tmp = fetch();
				A=subc(A, *mem((((uint16_t)fetch() << 8) | tmp) + Y));
				break;
			}
		case 0xE1:{ // ($aa,X)
				uint8_t tmp = fetch();
				A=subc(A, *mem((*mem(0xFF & (tmp+X+1)) << 8)+*mem(0xFF & (tmp+X))));
				break;
			}
		case 0xF1:{ // ($aa),Y
				uint8_t tmp = fetch();
				A=subc(A, *mem((*mem(tmp+1) << 8)+*mem(tmp)+Y));
				break;
			}
		// SEC
		case 0x38:
			P|=C_FLAG;
			break;
		// SED
		case 0xF8:
			P|=D_FLAG;
			break;
		// SEI
		case 0x78:
			P|=I_FLAG;
			break;
		// STA
		case 0x85:{ // $aa
				uint8_t tmp = fetch();
				//*mem(tmp)=A;
				memwrite(tmp,A);
				break;
			}
		case 0x95:{ // $aa,X
				uint16_t tmp = ((fetch()+X) & 0xFF);
				//*mem(tmp)=A;
				memwrite(tmp,A);
				break;
			}
		case 0x8D:{ // $aaaa	
				uint16_t tmp = fetch();
				//*mem(((uint16_t)fetch() << 8) | tmp)=A;
				tmp|=((uint16_t)fetch() << 8);
				memwrite(tmp,A);
				break;
			}
		case 0x9D:{ // $aaaa,X
				uint16_t tmp = fetch();
				//*mem((((uint16_t)fetch() << 8) | tmp) + X)=A;
				tmp=((((uint16_t)fetch() << 8) | tmp) + X);
				memwrite(tmp,A);
				break;
			}
		case 0x99:{ // $aaaa,Y
				uint16_t tmp = fetch();
				//*mem((((uint16_t)fetch() << 8) | tmp) + Y)=A;
				tmp=((((uint16_t)fetch() << 8) | tmp) + Y);
				memwrite(tmp,A);
				break;
			}
		case 0x81:{ // ($aa,X)
				uint16_t tmp = fetch();
				//*mem((*mem(0xFF & (tmp+X+1)) << 8)+*mem(0xFF & (tmp+X)))=A;
				tmp=((*mem(0xFF & (tmp+X+1)) << 8)+*mem(0xFF & (tmp+X)));
				memwrite(tmp,A);
				break;
			}
		case 0x91:{ // ($aa),Y
				uint16_t tmp = fetch();
				//*mem((*mem(tmp+1) << 8)+*mem(tmp)+Y)=A;
				tmp=((*mem(tmp+1) << 8)+*mem(tmp)+Y);
				memwrite(tmp,A);
				break;
			}
		// STX
		case 0x86:{ // $aa
				uint8_t tmp = fetch();
				//*mem(tmp)=X;
				memwrite(tmp,X);
				break;
			}
		case 0x96:{ // $aa,Y
				uint16_t tmp = fetch();
				//*mem((tmp+Y) & 0xFF)=X;
				tmp=((tmp+Y) & 0xFF);
				memwrite(tmp,X);
				break;
			}
		case 0x8E:{ // $aaaa
				uint16_t tmp = fetch();
				//*mem(((uint16_t)fetch() << 8) | tmp)=X;
				tmp=(((uint16_t)fetch() << 8) | tmp);
				memwrite(tmp,X);
				break;
			}
		// STY
		case 0x84:{ // $aa
				uint8_t tmp = fetch();
				//*mem(tmp)=Y;
				memwrite(tmp,Y);
				break;
			}
		case 0x94:{ // $aa,X
				uint16_t tmp = fetch();
				//*mem((tmp+X) & 0xFF)=Y;
				tmp=((tmp+X) & 0xFF);
				memwrite(tmp,Y);
				break;
			}
		case 0x8C:{ // $aaaa
				uint16_t tmp = fetch();
				//*mem(((uint16_t)fetch() << 8) | tmp)=Y;
				tmp=(((uint16_t)fetch() << 8) | tmp);
				memwrite(tmp,Y);
				break;
			}
		// TAX
		case 0xAA:
			X=A;
			znchk(X);
			break;
		// TAY
		case 0xA8:
			Y=A;
			znchk(Y);
			break;
		// TSX
		case 0xBA:
			X=SP;
			znchk(X);
			break;
		// TXA
		case 0x8A:
			A=X;
			znchk(A);
			break;
		// TXS
		case 0x9A:
			SP=X;
			//znchk(SP);  THIS LINE SEEMS TO BE INCORRECT
			break;
		// TYA
		case 0x98:
			A=Y;
			znchk(A);
			break;
		default:
			fprintf(stderr, "ERROR: opcode not recognised\n\n");
			exit(-1);
	}
}

void znchk(uint8_t reg){ // checks whether the zero (Z), or negative (N), flags should be set.
	if(reg == 0x00){
		P |= Z_FLAG;
	}
	else{
		P &= ~Z_FLAG;
	}

	if(reg & N_FLAG){
		P |= N_FLAG;
	}
	else{
		P &= ~N_FLAG;
	}

	return;
}
