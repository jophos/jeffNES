#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <NEShead.h>

int thingy(int argc, char *argv[]){

	if(CHAR_BIT!=8){
		fprintf(stderr,"ERROR: This program only supports systems which have 8 bits per byte\n");
		return -1;
	}

	if(argc!=2){
		fprintf(stderr, "USAGE: %s ROM.nes\n", argv[0]);
		return -1;
	}

	printf("ROM image: %s\n", argv[1]);

	FILE *fp=fopen(argv[1], "rb");
	if(fp==0){
		fprintf(stderr, "Could not open: %s\n", argv[1]);
		return -1;
	}

	char buffer[5];  // Should use uint8_t instead of char.
	if(fread(buffer, 1, 4, fp)!=4){
		fprintf(stderr, "%s does not seem to have a valid iNES header\n", argv[1]);
	  	return -1;	
	}// may need to use sizeof(uint8_t) for the size of each element to be portable.  size_t, however, is an integral type so cannot represent sizeof(uint8_t) if the platform has sizeof(char)>8 bits.

	buffer[4]=0; // Ensure that the string is null terminated;
	if(strcmp(buffer,"NES\x1A")==0){
		printf("iNES header detected\n");
	} else {
		fprintf(stderr, "%s does not seem to have a valid iNES header\n", argv[1]);
		return -1;
	}
	if(fread(&prg_rosze, 1, 1, fp)!=1){
		fprintf(stderr, "%s does not seem to have a valid iNES header:\n", argv[1]);
		fprintf(stderr, "- Failed to read size of PRG_ROM\n");
		return -1;
	}
	printf("Number of 16KB PRG-ROM banks: %u\n", prg_rosze);

	if(fread(&chr_rosze, 1, 1, fp)!=1){
		fprintf(stderr, "%s does not seem to have a valid iNES header:\n", argv[1]);
		fprintf(stderr, "- Failed to read size, or lack thereof, of CHR_ROM\n");
		return -1;
	}
	printf("Number of 8KB CHR-ROM banks: %u\n", chr_rosze);

	uint8_t flags6, flags7, prg_rasze, flags9, flags10;

	if(fread(&flags6, 1, 1, fp)!=1){
		fprintf(stderr, "%s does not seem to have a valid iNES header:\n", argv[1]);
		fprintf(stderr, "- Failed to read FLAGS 6\n");
		return -1;
	}
	if(fread(&flags7, 1, 1, fp)!=1){
		fprintf(stderr, "%s does not seem to have a valid iNES header:\n", argv[1]);
		fprintf(stderr, "- Failed to read FLAGS 7\n");
		return -1;
	}
	if(fread(&prg_rasze, 1, 1, fp)!=1){
		fprintf(stderr, "%s does not seem to have a valid iNES header:\n", argv[1]);
		fprintf(stderr, "- Failed to read size of PRG_RAM\n");
		return -1;
	}
	if(fread(&flags9, 1, 1, fp)!=1){
		fprintf(stderr, "%s does not seem to have a valid iNES header:\n", argv[1]);
		fprintf(stderr, "- Failed to read FLAGS 9\n");
		return -1;
	}

	mapper = ((flags6 & 0xF0) >> 4) | (flags7 & 0xF0);
	printf("Mapper Number: %u\n", mapper);

	if((flags6 & 0x08)!=0){
		printf("Four-screen mirroring enabled.\n");
	}
	else if((flags6 & 0x01)!=0){
		printf("Vertical mirroring enabled.\n");
	}
	else {
		printf("Horizontal mirroring enabled.\n");
	}

	if((flags6 & 0x02)!=0){
		printf("Battery-backed RAM at memory locations $6000-$7FFF\n");
	}
	else{
		printf("No battery-backed RAM detected.\n");
	}

	train=0;
	if((flags6 & 0x04)!=0){
		printf("512-byte trainer located at $7000-$71FF\n");
		train=1;
	}
	else{
		printf("No trainer detected.\n");
	}

	if((flags7 & 0x01)!=0){
		printf("VS Unisystem\n");
		fprintf(stderr, "ERROR: VS Unisystem ROMS are not currently supported\n");
		return -1;
	}
	
	int plych=0;
	if((flags7 & 0x02)!=0){
		printf("PlayChoice-10\n");
		fprintf(stderr, "ERROR: PlayChoice ROMS are not currently supported\n");
		return -1;
		plych=1;
	}

	char NES2=0;
	if((flags7 & 0x08)!=0 && (flags7 & 0x04)==0){
		NES2=1;
		printf("Header uses NES 2.0 specification.\n");
		fprintf(stderr, "ERROR: NES 2.0 is not supported.\n");
		return -1;
	}

	if(prg_rasze==0){
		prg_rasze=1;
	}

	printf("Number of 8KB PRG-RAM banks: %u\n", prg_rasze);

	if(fread(&flags10, 1, 1, fp)!=1){
		fprintf(stderr, "%s does not seem to have a valid iNES header:\n", argv[1]);
		fprintf(stderr, "- Failed to read FLAGS 10\n");
		return -1;
	}
	// NOTE: flags10 is unofficial, and currently ignored.

	if(fread(buffer, 1, 5, fp)!=5){
		fprintf(stderr, "%s does not seem to have a valid iNES header:\n", argv[1]);
		fprintf(stderr, "- Failed to read last 5 bytes of header\n");
		return -1;
	}

	int i;
	if (NES2==0){
		for(i=1; i<5; i++){
			if(buffer[i]!=0){
				fprintf(stderr, "ERROR: Bytes 11-15 of the header should all be zero, unless the NES2.0 specification is used.\n");
				return -1;
			}
		}
	}

	if(train==1){
		unsigned char train_dat[512];
		if(fread(train_dat, 1, 512, fp)!=512){
			fprintf(stderr, "Failed to read trainer.\n");
			return -1;
		}
	}

	prg_rom_dat=(unsigned char*)malloc(16384*prg_rosze);
	chr_rom_dat=(unsigned char*)malloc(8192*chr_rosze);

	if(prg_rom_dat==NULL || chr_rom_dat==NULL){
		fprintf(stderr, "ERROR: Failed to allocate memory for PRG_ROM and/or CHR_ROM data.\n");
		return -1;
	}

	if(fread(prg_rom_dat, 1, 16384*prg_rosze, fp)!=(16384*prg_rosze)){
		fprintf(stderr, "%s does not seem to have the correct iNES format:\n", argv[1]);
		fprintf(stderr, "- Failed to read PRG_ROM.\n");
		return -1;
	}
	if(fread(chr_rom_dat, 1, 8192*chr_rosze, fp)!=(8192*chr_rosze)){
		fprintf(stderr, "%s does not seem to have the correct iNES format:\n", argv[1]);
		fprintf(stderr, "- Failed to read CHR_ROM.\n");
		return -1;

	}

	if(plych==1){
		unsigned char plych_dat[8192];
		if(fread(plych_dat, 1, 8192, fp)!=8192){
			fprintf(stderr, "%s does not seem to have the correct iNES format:\n", argv[1]);
			fprintf(stderr, "- Failed to read PlayChoice INST-ROM.\n");
			return -1;
		}
	}

	fclose(fp);

	return 0;
}
