/* Copyright (c) 2026 Phaos. See ../LICENSE and ../THIRD_PARTY_NOTICES.md.
 * DOS feasibility renderer. No ISA driver, GUI, Body or moving controls yet.
 * Timed region contains only bh_render over RAM, not file I/O or setup.
 */
#include "bh_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#ifdef BH_ROOM_EXPERIMENT
#include "bh_room.h"
#define bh_state bh_room
#define bh_init bh_room_init
#define bh_render bh_room_render
#define BH_NAME "BHROOM"
#else
#define BH_NAME "BH486"
#endif
static int write_u32(FILE *f,bh_u32 n){
    bh_u8 bytes[4];int i;for(i=0;i<4;++i)bytes[i]=(bh_u8)(n>>(i*8));return fwrite(bytes,1,4,f)==4;
}
static int wav_header(FILE *f,bh_u32 frames,bh_u32 rate){
    static const bh_u8 format[8]={1,0,2,0,0,0,0,0};
    static const bh_u8 alignment[4]={2,0,8,0};
    return fwrite("RIFF",1,4,f)==4&&write_u32(f,36UL+frames*2UL)&&fwrite("WAVEfmt ",1,8,f)==8&&
        write_u32(f,16UL)&&fwrite(format,1,4,f)==4&&write_u32(f,rate)&&write_u32(f,rate*2UL)&&
        fwrite(alignment,1,4,f)==4&&fwrite("data",1,4,f)==4&&write_u32(f,frames*2UL);
}

int main(int argc,char **argv) {
    bh_patch patch;
    bh_state *state;
    bh_u8 *input,*output;
    bh_u32 frames,n,random=1UL,hash=2166136261UL;
    FILE *file;
    char magic[4];
    clock_t start,stop;
    int repeats=1,trial;
    bh_u32 source_bytes=0;
    if(argc<3){puts("BH486 patch.bhp output.raw|wav [repeats] [input.raw]\nUnsigned 8-bit stereo input at patch rate; optional file adds 8 seconds of tail.\nDOS feasibility renderer; NOT a live ISA effects driver.");return 2;}
    if(argc>3)repeats=atoi(argv[3]);if(repeats<1||repeats>100)return 2;
    file=fopen(argv[1],"rb");if(!file)return 3;
    if(fread(magic,1,4,file)!=4||memcmp(magic,"BHP1",4)||fread(&patch,1,sizeof(patch),file)!=sizeof(patch)||fgetc(file)!=EOF){fclose(file);return 4;}
    fclose(file);
    state=(bh_state*)malloc(sizeof(*state));
    if(!state||!bh_init(state,&patch)){free(state);return 5;}
    if(argc>4){long length;file=fopen(argv[4],"rb");if(!file){free(state);return 9;}
        if(fseek(file,0,SEEK_END)||(length=ftell(file))<2||length>1048576L||(length&1)){fclose(file);free(state);return 9;}
        source_bytes=(bh_u32)length;fclose(file);
    }
    frames=source_bytes?source_bytes/2UL+(bh_u32)patch.rate*8UL:(bh_u32)patch.rate*12UL;
    input=(bh_u8*)malloc(frames*2UL);output=(bh_u8*)malloc(frames*2UL);
    if(!input||!output){free(state);free(input);free(output);return 6;}
    for(n=0;n<frames&&!source_bytes;++n){
        random=random*1664525UL+1013904223UL;input[n*2]=n<(bh_u32)patch.rate?(bh_u8)((random>>24)%51UL+103UL):128;
        random=random*1664525UL+1013904223UL;input[n*2+1]=n<(bh_u32)patch.rate?(bh_u8)((random>>24)%51UL+103UL):128;
    }
    if(source_bytes){memset(input,128,frames*2UL);file=fopen(argv[4],"rb");
        if(!file||fread(input,1,source_bytes,file)!=source_bytes){if(file)fclose(file);free(input);free(output);free(state);return 9;}fclose(file);
    }
    printf("%s kernel: packets=%ld bits=%ld rate=%ld frames=%lu state_bytes=%lu patch_bytes=%lu\n",BH_NAME,patch.count,patch.bits,patch.rate,frames,(bh_u32)sizeof(*state),(bh_u32)sizeof(patch));
    for(trial=0;trial<repeats;++trial){
        bh_init(state,&patch);
        start=clock();bh_render(state,input,output,frames);stop=clock();
        printf("TRIAL %d ticks=%ld ticks_per_sec=%ld audio_frames=%lu\n",trial,(long)(stop-start),(long)CLOCKS_PER_SEC,frames);
    }
    for(n=0;n<frames*2UL;++n){hash^=output[n];hash*=16777619UL;}
    printf("FNV1A=%08lX output_bytes=%lu\n",hash,frames*2UL);
    file=fopen(argv[2],"wb");if(!file){free(input);free(output);free(state);return 7;}
    {size_t len=strlen(argv[2]);const char *ext=len>=4?argv[2]+len-4:"";
        if(*ext=='.'&&tolower((unsigned char)ext[1])=='w'&&tolower((unsigned char)ext[2])=='a'&&tolower((unsigned char)ext[3])=='v'){
            if(!wav_header(file,frames,(bh_u32)patch.rate)){fclose(file);free(input);free(output);free(state);return 8;}
        }
    }
    {int failed=fwrite(output,1,frames*2UL,file)!=frames*2UL;if(fclose(file))failed=1;
        if(failed){free(input);free(output);free(state);return 8;}
    }
    free(input);free(output);free(state);return 0;
}
