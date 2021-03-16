#include "mcc_generated_files/mcc.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "ctype.h"

#define SPIBUF  SPI1BUF
#define SPICON  SPI1CON1
#define SPISTAT SPI1STAT
#define SPIRFUL SPI1STATbits.SPIRBF

#define ReadSPI()   WriteSPI( 0xFF)
#define ClockSPI()  WriteSPI( 0xFF)
#define SDCS    _LATA2
#define RESET 0
#define INIT 1
#define READ_SINGLE 17
#define WRITE_SINGLE 24
#define DisableSD() SDCS = 1; ClockSPI()
#define EnableSD()  SDCS = 0
#define FALSE   0
#define TRUE    !FALSE
#define FAIL    0
#define DATA_START                   0xFE
#define DATA_ACCEPT                 0x05

#define FE_IDE_ERROR 1
#define FE_NOT_PRESENT 2
#define FE_PARTITION_TYPE 3
#define FE_INVALID_MBR 4
#define FE_INVALID_BR 5 
#define FE_MEDIA_NOT_MNTD 6
#define FE_FILE_NOT_FOUND 7
#define FE_INVALID_FILE 8
#define FE_FAT_EOF 9
#define FE_EOF 10
#define FE_INVALID_CLUSTER 11
#define FE_DIR_FULL        12
#define FE_MEDIA_FULL      13
#define FE_FILE_OVERWRITE  14
#define FE_CANNOT_INIT     15
#define FE_CANNOT_READ_MBR 16
#define FE_MALLOC_FAILED 17
#define FE_INVALID_MODE 18
#define FE_FIND_ERROR 19

#define ATT_RO 1
#define ATT_HIDE 2
#define ATT_SYS 4
#define ATT_VOL 8
#define ATT_DIR 0x10
#define ATT_ARC 0x20
#define ATT_LFN 0x0f
#define FOUND 2
#define NOT_FOUND 1

#define DIR_ESIZE 32
#define DIR_DEL 0xE5
#define DIR_ATTRIB 11
#define DIR_EMPTY 0
#define DIR_NAME 0
#define DIR_EXT 8
#define DIR_SIZE 28
#define DIR_CLST 26
#define DIR_CDATE 0x10
#define DIR_CTIME 0x0E
#define DIR_TIME 0x16
#define DIR_DATE 0x18

#define FAT_MCLST 0xfff8;
#define FAT_EOF 0xffff;

#define BR_SXC      0xd
#define BR_RES      0xe
#define BR_FAT_SIZE 0x16
#define BR_FAT_CPY  0x10
#define BR_MAX_ROOT 0x11
#define FO_SIGN     0x1FE
#define FO_FIRST_P    0x1BE 
#define FO_FIRST_TYPE 0x1C2
#define FO_FIRST_SECT 0x1C6 
#define FO_FIRST_SIZE 0x1CA
    
#define ReadW(a,f) *(unsigned short *)(a+f)
#define ReadL(a,f) *(unsigned long *)(a+f)
#define ReadOddW(a,f) (*(a+f) + (*(a+f+1)<<8))

#ifdef _ISR
#undef _ISR
#define _ISR __attribute__((interrupt,no_auto_psv))
#endif
#ifdef _ISRFAST
#undef _ISRFAST
#define _ISRFAST __attribute__((interrupt,shadow,no_auto_psv))
#endif
#define _FAR __attribute__(( far))
#define FCY 16000000UL

#define  RIFF_DWORD  0x46464952UL
#define  WAVE_DWORD  0x45564157UL
#define  DATA_DWORD  0x61746164UL
#define  FMT_DWORD   0x20746d66UL
#define  WAV_DWORD   0x00564157UL
#define B_SIZE 512

char FError;
unsigned Offset;
unsigned char *BPtr;
int BCount;
int Bytes;
int Stereo;
int Fix;
int Skip;
int Size;
char Play;
char Pause;
char PlayNext;
char Record;
char Began;
char Live;
char Max;
typedef unsigned long LBA;
unsigned char ABuffer[12][B_SIZE];
unsigned char TempBuf[2];
char CurBuf;
char WriteBuf;
volatile int AEmptyFlag;
char lister[80];

typedef struct {
    LBA fat;
    LBA root;
    LBA data;
    unsigned maxroot;
    unsigned maxcls;
    unsigned fatsize;
    unsigned char fatcopy;
    unsigned char sxc;
} MEDIA;
MEDIA *D; 

typedef struct {
    MEDIA * mda;
    unsigned char * buffer;
    unsigned short cluster;
    unsigned short ccls;
    unsigned short sec;
    unsigned short pos;
    unsigned short top;
    long seek;
    long size;
    unsigned short time;
    unsigned short date;
    char name[11];
    char mode;
    unsigned short fpage;
    unsigned short entry;  
} MFILE;

typedef struct {
    long ckID;
    long ckSize;
    long ckType;
} chunk;

typedef struct {
    unsigned short subtype;
    unsigned short channels;
    unsigned long srate;
    unsigned long bps;
    unsigned short align;
    unsigned short bitpsample;
} WAVE_fmt;

unsigned char WriteSPI( unsigned char b) {
    if (SPI1STATbits.SPIROV) SPI1STATbits.SPIROV= 0;
    SPIBUF = b;
    while( !SPIRFUL);
    return SPIBUF;
}

int SendSDCmd( unsigned char c, LBA a) {
    int i, r;
    EnableSD();
    WriteSPI( c | 0x40); 
    WriteSPI( a>>24);
    WriteSPI( a>>16);
    WriteSPI( a>>8);
    WriteSPI( a);
    WriteSPI( 0x95);
    i = 19;
    do {
        r = ReadSPI();      // check if ready
        if ( r != 0xFF) break; } while ( --i > 0);
        return ( r);
    } 

int InitMedia( void) {
    int i, r;
    DisableSD();
    for (i=0;i<10;i++) ClockSPI();
    EnableSD();
    r = SendSDCmd( RESET, 0); DisableSD(); 
    if ( r != 1)
    return 0x84;
    
    i = 10000;  
    do {
        r = SendSDCmd( INIT, 0); DisableSD();
        if ( !r) break; 
    } while( --i > 0); 
    
    if ( i==0) return 0x85;       
    SPISTAT = 0;
    SPICON = 0x013b;
    SPISTAT = 0x8000;
    return 0;
} 

int WriteSECTOR ( LBA a, unsigned char *p) {
    unsigned r, i;
    r = SendSDCmd( WRITE_SINGLE, ( a << 9));
    if ( r == 0) {
        WriteSPI( DATA_START);
        for( i=0; i<512; i++) WriteSPI( *p++);
        ClockSPI();
        ClockSPI();
        if ( (r= ReadSPI() & 0xf) == DATA_ACCEPT) {
            for( i=65000 ; i>0; i--) 
                if ( (r = ReadSPI())) break;
        } 
        else r = FAIL;
    } 
    DisableSD();
    return ( r);      
}
int ReadSECTOR( LBA a, unsigned char *p) {
    int r, i;
    r = SendSDCmd( READ_SINGLE, ( a << 9));
    if ( r == 0) {
    i = 25000;
    do{
        r = ReadSPI();
        if ( r == DATA_START) break; 
    }while( --i>0);
    if ( i) {
            for( i=0; i<512; i++) *p++ = ReadSPI();            
            ReadSPI();
            ReadSPI();
        } 
    }
    DisableSD();
    return ( r == DATA_START);
}

MEDIA * mount ( void){
    LBA psize;
    LBA firsts;
    int i;
    unsigned char *buffer;
    
    D= (MEDIA *) malloc(sizeof(MEDIA));
    if (D==NULL){ FError= FE_MALLOC_FAILED; return NULL; }
    buffer= (unsigned char *)malloc(512);
    if (buffer==NULL) {
        FError= FE_MALLOC_FAILED; free(D); return NULL; 
    }
    if (!ReadSECTOR(0,buffer)) { 
        FError= FE_CANNOT_READ_MBR; free(D); free(buffer); return NULL; 
    }
    if (buffer[FO_SIGN]!= 0x55 ||
            buffer[FO_SIGN+1] != 0xAA) { 
        FError= FE_INVALID_MBR; free(D); free(buffer); return NULL; 
    }     

    psize= ReadL(buffer,FO_FIRST_SIZE);
    i= buffer[FO_FIRST_TYPE];
    switch(i){
        case 0x04:
        case 0x06:
        case 0x0E:
            break;
        default: 
            FError= FE_PARTITION_TYPE; 
            free(D);free(buffer); return NULL;
    }
    firsts= ReadL(buffer,FO_FIRST_SECT);
    if (!ReadSECTOR(firsts,buffer)){ 
        free(D);free(buffer); return NULL; 
    }
    if (buffer[FO_SIGN]!= 0x55 ||
            buffer[FO_SIGN+1] != 0xAA) { 
        FError= FE_INVALID_BR; free(D); free(buffer); return NULL; 
    }
    
    D->sxc= buffer[BR_SXC];
    D->fat= firsts + ReadW(buffer,BR_RES);
    D->fatsize= ReadW(buffer,BR_FAT_SIZE);
    D->fatcopy= buffer[BR_FAT_CPY];
    D->root= D->fat + (D->fatsize * D->fatcopy);
    D->maxroot= ReadOddW(buffer,BR_MAX_ROOT);
    D->data= D->root + (D->maxroot>>4);
    D->maxcls= (psize - (D->data - firsts))/D->sxc;
     
    free( buffer);
    return D;
}

void umount( void){
    free(D);
    D= NULL;
}

unsigned ReadDATA( MFILE *fp){
    LBA l;
    l= fp->mda->data + (LBA)(fp->ccls - 2) * fp->mda->sxc + fp->sec;
    return (ReadSECTOR(l,fp->buffer));
}

unsigned WriteDATA( MFILE *fp){
    LBA l;
    l= fp->mda->data + (LBA)(fp->ccls - 2) * fp->mda->sxc + fp->sec;
    return ( WriteSECTOR( l, fp->buffer));
}

unsigned ReadDIR( MFILE *fp, unsigned e){
    LBA l;
    l= fp->mda->root + (e>>4);
    return ( ReadSECTOR( l, fp->buffer));
}

unsigned WriteDIR( MFILE *fp, unsigned c){
    LBA l;
    l= fp->mda->root + (c>>4);
    return ( WriteSECTOR( l, fp->buffer));
}

unsigned FindDIR( MFILE *fp){
    unsigned eCount;
    unsigned e;
    int i, a;
    MEDIA *mda= fp->mda;
    
    eCount= 0;
    if (!ReadDIR( fp, eCount)) return FAIL;
    while (1){
        e= (eCount & 0xf) * DIR_ESIZE;
        a= fp->buffer[ e + DIR_NAME];
        if ( a == DIR_EMPTY) return NOT_FOUND;
        if ( a != DIR_DEL) { 
            a= fp->buffer[ e+ DIR_ATTRIB];
            if ( !( a & ( ATT_DIR | ATT_HIDE))){
                for (i=DIR_NAME;i<DIR_ATTRIB;i++)
                    if ((fp->name[i]) != (fp->buffer[ e + i])) break;
                if ( i == DIR_ATTRIB){
                    fp->entry= eCount;
                    fp->time= ReadW( fp->buffer, e + DIR_TIME);
                    fp->date= ReadW( fp->buffer, e + DIR_DATE);
                    fp->size= ReadL( fp->buffer, e + DIR_SIZE);
                    fp->cluster= ReadL( fp->buffer, e + DIR_CLST);
                    return FOUND;
                }
            }
        }
        eCount++;
        if ((eCount & 0xf) == 0) 
            if ( !ReadDIR( fp, eCount)) return FAIL;
        
        if ( eCount >= mda->maxroot) return NOT_FOUND;
    }
}

unsigned NewDIR( MFILE *fp){
    MEDIA *mda= fp->mda;
    unsigned eCount, e, a;
    eCount= 0;
    if (!ReadDIR( fp, eCount)) return FAIL;
    while (1){
        e= ( eCount & 0xf) * DIR_ESIZE;
        a= fp->buffer[ e + DIR_NAME];
        if ( ( a== DIR_EMPTY) || ( a == DIR_DEL)) {
            fp->entry= eCount;
            return FOUND;
        }
        eCount++;
        if ( (eCount & 0xf)== 0)  
            if (!ReadDIR( fp, eCount))
                return FAIL;
        if ( eCount > mda->maxroot) return NOT_FOUND;        
    }
    return FAIL;
}

unsigned ReadFAT( MFILE *fp, unsigned ccls){
    unsigned p, c;
    LBA l;
    p= ccls>>8;
    l= fp->mda->fat + p;
    if (!ReadSECTOR ( l, fp->buffer)) return FAT_EOF;
    c= ReadOddW( fp->buffer, ((ccls & 0xff)<<1));
    return c;
}

unsigned NextFAT( MFILE *fp, unsigned n){
    MEDIA *mda= fp->mda;
    unsigned c;
    do{
        c= ReadFAT( fp, fp->ccls);
        if ( c >= 0xfff8){
            FError= FE_FAT_EOF; return FAIL;
        }
        if ( c >= mda->maxcls){
            FError= FE_INVALID_CLUSTER; return FAIL;
        }        
    } while (--n > 0);
    fp->ccls= c;
    return TRUE;
}

unsigned WriteFAT( MFILE *fp, unsigned cls, unsigned v){
    LBA l;
    unsigned p, i;
    ReadFAT( fp, cls);
    p= ( cls & 0xff) * 2;
    fp->buffer[ p]= v;
    fp->buffer[ p+1]= (v>>8);
    l= fp->mda->fat + (cls>>8);
    for (i=0;i<fp->mda->fatcopy; i++, l += fp->mda->fatsize)
        if (!WriteSECTOR( l, fp->buffer)) return FAIL;
    return TRUE;
}

unsigned NewFAT( MFILE *fp){
    unsigned i, c= fp->ccls;
    do{
        c++;
        if ( c>= fp->mda->maxcls) c= 0;
        if ( c == fp->ccls){
            FError= FE_MEDIA_FULL; return FAIL;
        }
        i= ReadFAT( fp, c);
    } while (i != 0);
    
    WriteFAT( fp, c, 0xffff);
    if ( fp->ccls > 0) WriteFAT( fp, fp->ccls, c);
    fp->ccls= c;
    return TRUE;
}

unsigned fcloseM( MFILE *fp){
    unsigned e, r;
    r= FAIL;
    if ( fp->mode == 'w'){
        if ( fp->pos > 0) if (!WriteDATA(fp)) goto ExitClose;
        if (!ReadDIR( fp, fp->entry)) goto ExitClose;
        e= (fp->entry & 0xf) * DIR_ESIZE;
        fp->buffer[ e + DIR_SIZE]= fp->size;
        fp->buffer[ e + DIR_SIZE+1]= fp->size>>8;
        fp->buffer[ e + DIR_SIZE+2]= fp->size>>16;
        fp->buffer[ e + DIR_SIZE+3]= fp->size>>24;
        if (!WriteDIR( fp, fp->entry)) goto ExitClose;        
    }
    r = TRUE;
    ExitClose:
    free(fp->buffer); free( fp); return r;
}

MFILE *fopenM( const char *filename, const char *mode){
    char c;
    int i, r, e;
    unsigned char *b;
    MFILE *fp;
    if (D == NULL) { FError= FE_MEDIA_NOT_MNTD; return NULL; }
    b= (unsigned char *) malloc(512);
    if (b == NULL) { FError= FE_MALLOC_FAILED; return NULL; }
    fp= (MFILE *) malloc(sizeof(MFILE));
    if (fp == NULL) { FError= FE_MALLOC_FAILED; free(b); return NULL; }
    
    fp->mda= D;
    fp->buffer= b;
        
    for (i=0;i<8;i++){
        c= toupper( *filename++);
        if (c == '\0' || c == '.') break;
        else fp->name[i]= c;        
    }
    while (i<8) fp->name[i++]= ' ';
    if (c != '\0') {
        for (i=8;i<11;i++){            
            c= toupper( *filename++);
            if (c=='.') c= toupper( *filename++);
            if (c=='\0') break;
            else fp->name[i]= c;
        }        
    while (i<11) fp->name[i++]= ' ';
    }
        
    if ((*mode == 'r')||(*mode == 'w')) fp->mode= *mode;
    else { FError= FE_INVALID_MODE; goto ExitOpen;}
        
    if ((r= FindDIR(fp)) == FAIL) { 
        FError= FE_FIND_ERROR; goto ExitOpen; 
    }
    
    fp->seek= 0; 
    fp->sec= 0;
    fp->pos= 0;
    
    if (fp->mode == 'r') {        
    if (r == NOT_FOUND){
        FError= FE_FILE_NOT_FOUND; goto ExitOpen;
    }
    else {
        fp->ccls= fp->cluster;
    if (!ReadDATA(fp)) goto ExitOpen;
    if (fp->size - fp->seek <512)
        fp->top= fp->size - fp->seek;
    else fp->top= 512;
    }
    }
    
    else { 
        if (r == NOT_FOUND) { 
            fp->ccls= 0;
            if (NewFAT(fp) == FAIL) {
                FError= FE_MEDIA_FULL; goto ExitOpen;
            }
            fp->cluster= fp->ccls;
            if ( (r= NewDIR(fp)) == FAIL){
                FError= FE_IDE_ERROR; goto ExitOpen;
            }
            if ( r == NOT_FOUND) {
                FError= FE_DIR_FULL; goto ExitOpen;
            }
            else { 
                fp->size= 0;
                e= (fp->entry & 0xf) * DIR_ESIZE;
                for (i=0;i<DIR_ESIZE;i++) 
                    fp->buffer[ e + i]= 0;
                fp->date = 0x378A; // Dec 10th, 2007
                fp->buffer[ e + DIR_DATE]  = fp->date;
                fp->buffer[ e + DIR_DATE+1]= fp->date>>8;
                fp->buffer[ e + DIR_CDATE]  = fp->date;
                fp->buffer[ e + DIR_CDATE+1]= fp->date>>8;
                fp->time = 0x6000; // 12:00:00 PM
                fp->buffer[ e + DIR_TIME]  = fp->time + 1;
                fp->buffer[ e + DIR_TIME+1]= fp->time>>8;
                fp->buffer[ e + DIR_CTIME]  = fp->time;
                fp->buffer[ e + DIR_CTIME+1]= fp->time>>8;
                fp->buffer[ e + DIR_CLST]= fp->ccls;
                fp->buffer[ e + DIR_CLST + 1]= fp->ccls>>8;
                for (i=0;i<DIR_ATTRIB;i++) 
                    fp->buffer[ i + e]= fp->name[i];
                fp->buffer[ e+ DIR_ATTRIB]= ATT_ARC;
                if (!WriteDIR( fp,fp->entry)){
                    FError= FE_IDE_ERROR; goto ExitOpen;
                }
            }
        }
        else { FError= FE_FILE_OVERWRITE; goto ExitOpen; }
    }
    return fp;
    ExitOpen:
            free(fp->buffer); free(fp);return NULL;
}

unsigned fwriteM( void *src, unsigned count, MFILE *fp){
    MEDIA *mda= fp->mda;
    unsigned len, size= count;
    if (fp->mode != 'w') {
        FError= FE_INVALID_FILE; return FAIL;
    }
    while (count>0){
        if ((fp->pos + count)<512) len= count;
        else len= 512 - fp->pos;
        memcpy ( fp->buffer + fp->pos, src, len);
        
        fp->pos += len;
        fp->seek += len;
        count -= len;
        src += len;
        if (fp->seek > fp->size) fp->size= fp->seek;
        if (fp->pos == 512) {
            if (!WriteDATA ( fp)) return FAIL;
            fp->pos= 0;
            fp->sec++;
            if (fp->sec == mda->sxc) {
                fp->sec= 0;
                if (NewFAT( fp) == FAIL) return FAIL;
            }
        }
    }
    return size-count;
}

unsigned freadM( void *dest, unsigned size, MFILE *fp){
    MEDIA *mda= fp->mda;
    unsigned count= size;  
    unsigned len;    
    if ( fp->mode != 'r') {
        FError= FE_INVALID_FILE; return FAIL;
    }
    while (count>0){
        if (fp->seek >= fp->size) {
            FError= FE_EOF; break;
        }
        if (fp->pos == fp->top){
            fp->pos= 0;
            fp->sec++;
            if (fp->sec == mda->sxc) {
                fp->sec= 0;
                if (!NextFAT( fp, 1)) break;
            }
            if ( !ReadDATA( fp)) break;
            if ( (fp->size - fp->seek) < 512)
                fp->top= fp->size - fp->seek;
            else fp->top= 512;            
        }
        if ((fp->pos + count) < fp->top) len= count;
        else len= fp->top - fp->pos;
        memcpy ( dest, fp->buffer + fp->pos, len);
        
        count -= len; fp->pos += len; 
        fp->seek += len; dest += len;
    }
    return size - count;
}

unsigned fseekM( MFILE *fp, unsigned count){
    char buffer[16];
    unsigned d, r;
    while ( count){
        d= (count>=16)? 16: count;
        r= freadM( buffer, d, fp);
        count -= r;
        if ( r!=d) break;
    }
    return count;
}

void InitAudio( long bitrate, int skip, int size, int stereo) {
    CurBuf = 0;             
    BPtr = &ABuffer[ CurBuf][size-1];
    BCount = B_SIZE;        
    AEmptyFlag = 0;
    Skip = skip;
    Fix = (size==2)? 0x80 : 0;                    
    Stereo = stereo;
    Size = size;
    Bytes = size*stereo;

    T3CON = 0x8000;        
    OC2RS= OC1RS= PR3 =  FCY / bitrate;   
    Offset = PR3/2;         
    _T3IF = 0;              
    _T3IE = 1;              
    OC1R = Offset;
    OC2R = Offset;
}
void HaltAudio( void) {
    T3CON = 0;         
   _T3IE = 0;
}

unsigned PlayWAV( char *name) {
    chunk       ck;    
    WAVE_fmt    wav;
    int         last;
    MFILE       *fp;
    unsigned long lc, r, d;
    int skip, size, stereo;
    unsigned long rate;
    
    if ( (fp = fopenM( name, "r")) == NULL)
        return FALSE;
    
    freadM( (void*)&ck, sizeof(chunk), fp);
    if (( ck.ckID != RIFF_DWORD) || ( ck.ckType != WAVE_DWORD))
        goto Exit;
    
    freadM( (void*)&ck, 8, fp);
    if ( ck.ckID != FMT_DWORD) goto Exit;
    freadM( (void*)&wav, sizeof(WAVE_fmt), fp);
    stereo = wav.channels;
    fseekM( fp, ck.ckSize - sizeof(WAVE_fmt));
    
    while( 1) {   
        if ( freadM( (void*)&ck, 8, fp) != 8)
            goto Exit;
        if ( ck.ckID != DATA_DWORD)
            fseekM( fp, ck.ckSize );
        else break;
    } 
    lc = ck.ckSize;
    rate = wav.srate;         
    skip = 1;                  
    while ( rate < 22050) {
        rate <<= 1;          
        skip <<= 1;         
    }
    d = (FCY/rate)-1;
    if ( d > ( 65536L)) {
        fcloseM( fp);
        return FALSE;
    }
    
    CurBuf = 0;
    stereo = wav.channels;
    size  = 1;                  
    if ( wav.bitpsample == 16) size = 2;
    if ( lc < B_SIZE*2) goto Exit;
    r = freadM( ABuffer[0], B_SIZE*2, fp);
    AEmptyFlag = FALSE;     
    lc-= B_SIZE*2 ;            
    InitAudio( rate, skip, size, stereo);
    
    while (lc >=B_SIZE) {  
        if ( PlayNext || !Play) { PlayNext= 0; goto Exit; }
        if ( AEmptyFlag) {
            r = freadM( ABuffer[1-CurBuf], B_SIZE, fp);
            AEmptyFlag = FALSE;
            lc-= B_SIZE;            
        }
    } 
    if( lc>0) {
        r = freadM( ABuffer[1-CurBuf], lc, fp);
        last = ABuffer[1-CurBuf][r-1];
        while(( r<B_SIZE) && (last>0)) 
            ABuffer[1-CurBuf][r++] = last;
        
        AEmptyFlag = 0;
        while (!AEmptyFlag);
    }
    AEmptyFlag = 0;
    while (!AEmptyFlag);    
Exit:
    HaltAudio();
    fcloseM( fp);
    return TRUE;
} 

void InitRec( MFILE *fp){
    chunk ck1, ck2, ck3;
    WAVE_fmt wav;
    
    ck1.ckID= RIFF_DWORD;
    ck1.ckSize= 0;
    ck1.ckType= WAVE_DWORD;
    ck2.ckID= FMT_DWORD;
    ck2.ckSize= 16;
    wav.subtype= 1;
    wav.channels= 2; 
    wav.srate= 44100;
    wav.bps= 44100*2*8/8; 
    wav.align= 2*8/8; 
    wav.bitpsample= 8;
    ck3.ckID= DATA_DWORD;
    ck3.ckSize= 0;     
    
    fwriteM( (void*)&ck1, sizeof( chunk), fp);
    fwriteM( (void*)&ck2, 8, fp);
    fwriteM( (void*)&wav, sizeof( WAVE_fmt), fp);
    fwriteM( (void*)&ck3, 8, fp);    
}

unsigned fcloseWAV ( MFILE *fp){
    unsigned long total= fp->size - 8;
    unsigned long data= fp->size - 44;
    LBA l;
    l= fp->mda->data + (LBA)(fp->cluster - 2) * fp->mda->sxc;
    ReadSECTOR(l,fp->buffer);
    fp->buffer[4]= total & 0xff;
    fp->buffer[5]= (total>>8) & 0xff;
    fp->buffer[6]= (total>>16) & 0xff;
    fp->buffer[7]= (total>>24) & 0xff;
    fp->buffer[40]= data & 0xff;
    fp->buffer[41]= (data>>8) & 0xff;
    fp->buffer[42]= (data>>16) & 0xff;
    fp->buffer[43]= (data>>24) & 0xff;
    WriteSECTOR( l, fp->buffer);
    memcpy( &lister[8*Max], &fp->name[0],8);
    Max++;
    return TRUE;
}

unsigned RecWAV( char *name){
    MFILE *fp;
    CurBuf= 0;
    WriteBuf= 0;
    AEmptyFlag= 0;
    if ( (fp = fopenM( name, "w")) == NULL) return FALSE;
    InitRec( fp);
    _T1IF=0;
    _T1IE= 1;
    T1CONbits.TON= 1;
    AD1CON1bits.ADON= 1;
    
    while ( Record){
        while ( WriteBuf== CurBuf) if ( !Record) break;
        fwriteM( ABuffer[ WriteBuf], 512, fp);
        WriteBuf++;
        if (WriteBuf== 12) WriteBuf= 0;
    }
    fcloseWAV( fp);
    fcloseM( fp);
    return TRUE;
}

unsigned listTYPE( char *list, int max, char *ext ) {
    unsigned eCount;      
    unsigned eOffs;         
    unsigned x, a, r;             
    MFILE *fp;
    unsigned char *b;
    x = 0;
    r = 0;
    if ( D == NULL) {
        FError = FE_MEDIA_NOT_MNTD;
        return 0;
    }
    b = (unsigned char*)malloc( 512);
    if ( b == NULL) {   
        FError = FE_MALLOC_FAILED;
        return 0;
    }
    fp = (MFILE *) malloc( sizeof( MFILE));
    if ( fp == NULL) {   
        FError = FE_MALLOC_FAILED;
        free( b);
        return 0;
    }
    fp->mda = D;
    fp->buffer = b;
    eCount = 0;
    eOffs = 0;
    if ( !ReadDIR( fp, eCount)) {
        FError = FE_FIND_ERROR;
        goto ListExit;
    }
    ReadDIR( fp, eCount);
    while ( TRUE) {    
        a =  fp->buffer[ eOffs + DIR_NAME];        
        if ( a == DIR_EMPTY)  break;
        if ( a != DIR_DEL) {
          a = fp->buffer[ eOffs + DIR_ATTRIB];
          if ( !( a & (ATT_HIDE|ATT_DIR))) {   
            if ( !memcmp( &fp->buffer[ eOffs+DIR_EXT], ext, 3)) {  
               memcpy( &list[x*8], 
                       &fp->buffer[ eOffs+DIR_NAME], 8);
               if ( ++x >= max)
                   break;
            }                        
          } 
        } 
        eCount++;
        if ( eCount > fp->mda->maxroot)
            break;             
        eOffs += 32;
        if ( eOffs >= 512) { 
            eOffs = 0;
            if ( !ReadDIR( fp, eCount)) {
                FError = FE_FIND_ERROR;
                goto ListExit;
            }
        }
    }
    r = x;
ListExit:
    free( fp->buffer);
    free( fp);
    return r;
}

void _ISR _T1Interrupt( void){
    static int i= 0; 
    if (AD1CON2bits.BUFS){
    ABuffer[CurBuf][i++]= ADC1BUF0 >> 2;
    ABuffer[CurBuf][i++]= ADC1BUF1 >> 2;
    }
    else {
       ABuffer[CurBuf][i++]= ADC1BUF8 >> 2;
       ABuffer[CurBuf][i++]= ADC1BUF9 >> 2; 
    }
    if ( Live) {
        OC1R= ABuffer[CurBuf][i-1];
        OC2R= ABuffer[CurBuf][i-2];
    }
    if ( i==512) { i=0; CurBuf++; if (CurBuf==12) CurBuf= 0;}
    _T1IF= 0;
}

void _ISR _T2Interrupt( void){
    printf("timer done\n");
    _T2IE= 0; _T2IF= 0; T2CONbits.TON= 0; 
    _CN0IE= 1; _CN2IE= 1; _CN3IE= 1;_CNIP= 5; _CNIE= 1;
}

void _ISR  _T3Interrupt( void) {
    static int sk = 1;
    if ( --sk == 0) {
        sk = Skip;
        OC1R = 30+(*BPtr ^ Fix); 
        if ( Stereo==2) OC2R =30 + (*(BPtr + Size) ^ Fix);
        else OC2R = OC1R;
        BPtr += Bytes;
        BCount -= Bytes;
        if ( BCount <= 0) {
            CurBuf = 1- CurBuf;
            BPtr = &ABuffer[ CurBuf][Size-1];
            BCount = B_SIZE;
            AEmptyFlag = 1;
        }
        while ( Pause);
    } 
    _T3IF = 0;
}

void _ISR _CNInterrupt( void) {
    if ( _RA4== 0) {
        if ( Record) { 
            if ( !Live) {
                printf("playing live\n");
                T3CON = 0x8000;      
                OC2RS= OC1RS= PR3 = 363;
                OC1R= OC2R = PR3/2;
                Live= 1;
            }
            else { printf("stopping live\n"); T3CON= 0; OC1R= OC2R= 0; Live= 0; }
        }
        else if ( !Play) { printf("starting playback\n"); Play= 1; PlayNext= 0; }
        else { printf("pausing\n");Pause= !Pause;}
    }
    
    if ( _RA1== 0){
        if ( Record) {
            printf("stopping record\n");
            Began= Record= Live= 0;
            T3CON= 0; T1CONbits.TON= 0; AD1CON1bits.ADON= 0;
        }
        else {
            printf("starting recording\n");
            Record= 1;
            Play= Pause= 0;
        }
    }
    
    if ( _RA0== 0) { printf("playing next");PlayNext= 1;}
    
    _CN0IE= 0; _CN2IE= 0; _CN3IE= 0; _CNIE= 0; _CNIF = 0; 
    _T2IF= 0; _T2IP= 5; _T2IE= 1; PR2= 0xffff; T2CON= 0x8030;
}

int main(void) {
    SYSTEM_Initialize();
    printf("started\n");
    InitMedia();
    mount();
    
    int j;
    Max= listTYPE( &lister, 10, "WAV");
    Play= Pause= PlayNext= Record= Began= Live= 0;
    _CN0IE= 1;
    _CN2IE= 1;
    _CN3IE= 1;
    _CNIF = 0;
    _CNIP= 5;
    _CNIE= 1;
    
    while( 1) {
        if ( Play){
            char temp2[13];
            temp2[8]='.';
            temp2[9]='W';
            temp2[10]= 'A';
            temp2[11]= 'V';   
            for (j=0;j<Max;j++){
                memcpy( &temp2,&lister[j*8],8);
                PlayWAV( temp2);
                if ( !Play) break;
            }
            Play= 0;
        }
        if ( Record && !Began){
            int i,j=0,k;
            char temp[8];
            for (i=0;i<Max;i++) {
                memcpy(temp, (lister)+(i*8),8);
                k= atoi(temp);
                if (k>j) j=k;
            }
            sprintf( temp,"%d.wav",++j);
            Began= 1;
            RecWAV( temp);
        }
    }
    
    return -1;
}