
/*                                              */
/*      MIDI Library    version 1.00á           */
/*                                              */
/*      (C) Ying Yang Corp.  1995.03.           */
/*                      Code : S.G. Chang       */

#include <io.h>
#include <dos.h>
#include <fcntl.h>

#define byte    unsigned char           // 1byte
#define word    unsigned int            // 2byte
#define dword   unsigned long int       // 4byte
#define nop()   asm     nop             // NOP

void    NewRate(unsigned int milli);
void    interrupt NewTimer(void);
void    DisableTimer(void);
void    EnableTimer(void);
int     TrackDeltaTime(int idx);
dword   ReadVarLen (byte *buffer);
dword   Xchange(byte *buffer,byte size);
void    ReadMIDIMessage(int idx);
int     LoadMIDIFile(char *midf,char *buffer);
void    ResetMIDIstruct(void);
int     InitMIDI(void);
void    InitMPUport(word port);
void    PlayMIDI(char *midf,char *buff,byte rep);
void    QuitMIDI(void);
int     MIDIPlayStatus(void);
word    MIDIport(void);

void    SetVolume(int,int);
void    SetAllVolume(int);

extern  int     SendMPU(byte);          // asm code
extern  int     CtrlMPU(byte);          // asm code
extern  int     ReadMPU(byte);          // asm code
extern  int     MPUport;                // asm data

typedef struct  TRACK
        {
        dword   length;                 //track of size
        byte    *source;                //start of pointer
        byte    *seek;                  //seek of pointer
        word    delta;                  //delay time
        byte    temp;                   //temp of status
        byte    status;                 //if end of track
        };

typedef struct  CHANNEL
        {
        byte    volume;                 //channel of valume
        byte    note;                   //channel of note
        };

typedef struct  SMF
        {
        word    size;                   // file size
        byte    type;                   // SMF type
        byte    tracks;                 // track value
        byte    volume;                 // no use
        byte    countdown;              // countdown for play
        byte    cd_backup;              // countdown backup
        byte    MesgSize;               // record ReadVerLen() read of length
        word    BasicTempo;             // Basic Tempo
        byte    TimerFlag;              // for timer seting
        word    ms;                     // count to run old timer
        word    count;                  // countdown for old timer
        word    ticks;                  // Clock Ticks Per Second
        word    division;               // Ticks Per Quater Note
        dword   tempo;                  // Tempo Time
        byte    *pointer;               // main buffer pointer
        struct  TRACK   track[64];      // 64 Track of struct
        struct  CHANNEL channel[16];    // 16 Channel of struct
        };
struct  SMF     midi;

void    interrupt (*OldTimer)();


#define timer   0x08
#define oldrate 65494

#define cli()   asm     cli
#define sti()   asm     sti

void    NewRate(unsigned int milli)
{
asm     mov     al,36h
asm     out     43h,al
asm     mov     ax,milli
asm     out     40h,al
asm     mov     al,ah
asm     out     40h,al
}

void    interrupt NewTimer()
{
        int     j;

        if(!midi.TimerFlag)
        {
                cli();
                midi.TimerFlag=1;
                sti();
                if(midi.countdown)
                {
                        if(CheckStop())
                        {
                                for(j=0;j<midi.tracks;j++)
                                {
                                        ReadMIDIMessage(j);
                                }
                        }
                        else
                        {
                                ResetMIDIstruct();
                                if(midi.countdown!=0xff)
                                        midi.countdown--;
                        }
                }
                midi.count++;
                if(midi.count>=midi.ms)
                {
                        midi.count=0;
                        OldTimer();
                }
                cli();
                midi.TimerFlag=0;
                sti();
        }

asm     push    ax
asm     mov     al,20h
asm     out     20h,al
asm     pop     ax
}

void    DisableTimer()
{
        cli();
        OldTimer=getvect(timer);
        setvect(timer,NewTimer);
        sti();
}

void    EnableTimer()
{
        cli();
        NewRate(oldrate);               //back to nomal speed
        setvect(timer,OldTimer);
        sti();
}


dword   ReadVarLen (byte *buffer)
{
        register dword value;
        register byte c;

        midi.MesgSize=1;
        if ((value = *(buffer)) & 0x80)
        {
                value &= 0x7f;
                do
                {
                        buffer++;
                        midi.MesgSize++;
                        value = (value << 7) + ((c = *(buffer)) & 0x7f);
                } while (c & 0x80);
        }
        return  value;
}

dword   Xchange(byte *buffer,byte size)
{
        register dword   Max=0;
        register word    i;
        register byte    a;

        for(i=0;i<size;i++)
        {
                Max<<=8;
                Max|=*(buffer+i);
        };
        return  Max;
}


void    ReadMIDIMessage(int idx)
{
        word    i,z;
        byte    len[]={  0, 0, 0, 0, 0, 0, 0, 0,
                         2, 2, 2, 2, 1, 1, 2, 0 };
        byte    k,stay,t,c;

        if(midi.track[idx].delta)
        {
                midi.track[idx].delta--;
                if(midi.track[idx].delta)        return;
        }

        if(midi.track[idx].status)     return;

        if(*(midi.track[idx].seek)&0x80)        //Save Message ID
        {
                stay=midi.track[idx].temp;
                midi.track[idx].temp=*(midi.track[idx].seek);
                midi.track[idx].seek++;
        }                                       //else Running status

        k=midi.track[idx].temp&0xf0;
        c=midi.track[idx].temp&0x0f;            //ch.
        switch(k)
        {
                case 0xf0:   //System Exlusive Message

                        if(midi.track[idx].temp==0xff)
                        {
                                z=*(midi.track[idx].seek);
                                midi.track[idx].seek++;

                                t=ReadVarLen(midi.track[idx].seek);
                                midi.track[idx].seek+=midi.MesgSize;
                                switch(z)
                                {
                                        case 0x2f:
                                                midi.track[idx].status=1;
                                                break;
                                        case 0x51:
                                                midi.tempo=Xchange(midi.track[idx].seek,3);
                                                midi.ticks=midi.tempo/midi.division;
                                                midi.ms=(1193180l/midi.ticks)/18;
                                                NewRate(midi.ticks);
                                              /*
                                                midi.tempo=Xchange(midi.track[idx].seek,3);
                                                midi.BasicTempo=(unsigned)(60000000l/midi.tempo);
                                                midi.ticks=(unsigned)((float)midi.BasicTempo/60.*midi.division);
                                                midi.ms=((unsigned)1193180l/midi.ticks)/18;
                                                NewRate((unsigned)1193180l/midi.ticks/18);
                                                */
                                                break;
                                }
                        }
                        else
                        {
                                t=ReadVarLen(midi.track[idx].seek);
                                midi.track[idx].seek+=midi.MesgSize;
                                SendMPU(midi.track[idx].temp);
                                for(i=0;i<t;i++)
                                        SendMPU( *(midi.track[idx].seek+i) );
                        }
                        midi.track[idx].seek+=t;
                        midi.track[idx].temp=stay;
                        break;
                default:
                        t=len[(k>>4)];

                        SendMPU(midi.track[idx].temp);
                        for(i=0;i<t;i++)
                                SendMPU( *(midi.track[idx].seek+i) );

                        switch(k)
                        {
                                case 0xb0:
                                        if(*(midi.track[idx].seek)==0x07)
                                                midi.channel[c].volume=*(midi.track[idx].seek+1);
                                        break;
                                case 0xc0:
                                        midi.channel[c].note=*(midi.track[idx].seek);
                                        break;
                        }
                        midi.track[idx].seek+=t;
                        break;
        }
        midi.track[idx].delta=ReadVarLen(midi.track[idx].seek);
        midi.track[idx].seek+=midi.MesgSize;
}


int     CheckStop()
{
        int     i;
        for(i=0;i<midi.tracks;i++)
                if(!midi.track[i].status)       return 1;
        return  0;
}

int     LoadMIDIFile(char *midf,char *buffer)
{
        int     i,fh;
        byte    *ptr;

        ptr=(byte *)&midi.size;
        for(i=0;i<sizeof(midi);i++)
        {
                *(ptr+i)=0;
        }

        midi.pointer=buffer;

        if((fh=_open(midf,O_RDONLY))==-1)   return 1;
        midi.size=filelength(fh);
        _read(fh,midi.pointer,midi.size);
        _close(fh);

        i=midi.size;
        ptr=midi.pointer;

        midi.type=Xchange(midi.pointer+8,2);
        midi.tracks=Xchange(midi.pointer+10,2);   //Max Tracks
        midi.division=Xchange(midi.pointer+12,2);

        if(midi.division&0x8000)        return 1;       // nonsupport SMPTE
//////////////////////////////////////////
        ptr+=4;         //MThd  4Byte
        ptr+=Xchange(ptr,4);
        ptr+=4;         //length 4Byte
        for(i=0;i<midi.tracks;i++)
        {
                ptr+=4;         //MTrk 4Byte
                midi.track[i].source=ptr;
                midi.track[i].length=Xchange(ptr,4);
                ptr+=4+midi.track[i].length;
        }
//////////////////////////////////////////

        for(i=0;i<midi.tracks;i++)
                midi.track[i].seek=midi.track[i].source+4;      //jump length

        for(i=0;i<midi.tracks;i++)
        {
                midi.track[i].delta=ReadVarLen(midi.track[i].seek);
                midi.track[i].seek+=midi.MesgSize;
                midi.track[i].temp=*(midi.track[i].seek);
        }

        return  0;
}

void    ResetMIDIstruct()
{
        int     i;
        for(i=0;i<midi.tracks;i++)
        {
                midi.track[i].seek=midi.track[i].source+4;      //jump length
                midi.track[i].temp=0;
                midi.track[i].status=0;
                midi.track[i].delta=ReadVarLen(midi.track[i].seek);
                midi.track[i].seek+=midi.MesgSize;
        }
}

int     InitMIDI()
{
        int     i,k;

        if(CtrlMPU(0xff))       return 1;        //System Reset
        CtrlMPU(0x3f);          //Set MPU-401 to UART mode
        CtrlMPU(0x88);          //Disable MPU-401 THRU

        midi.countdown=0;
        midi.division=120;
        midi.tempo=500000;              //base speed
        midi.ticks=midi.tempo/midi.division;
        midi.ms=(1193180l/midi.ticks)/18;

        DisableTimer();
        NewRate(midi.ticks);
        return 0;
}

void    PlayMIDI(char *midf,char *buff,byte rep)
{
        int     i;
        if(midi.countdown)
        {
                SetAllVolume(0);
                for(i=0;i<16;i++)
                {
                        SendMPU(0x90+i);        //note off
                        SendMPU(0x00);
                        SendMPU(0x00);
                }
        }
        while(midi.TimerFlag);
        midi.countdown=0;
        i=LoadMIDIFile(midf,buff);
        if(i)   return;
        midi.countdown=rep;
        midi.cd_backup=rep;
}


void    QuitMIDI()
{
        int     i;
        while(midi.TimerFlag);
        midi.countdown=0;
        EnableTimer();
        SetAllVolume(0);
        for(i=0;i<16;i++)
        {
                SendMPU(0x90+i);
                SendMPU(0x00);
                SendMPU(0x00);
        }
        CtrlMPU(0xff);          //System Reset
}

void    InitMPUport(word port)
{
        MPUport=port;
}

int     MIDIPlayStatus()
{
        if(midi.countdown)      return 1;
        else                    return 0;
}

void    SetAllVolume(int vol)
{
        int     i;

        for(i=0;i<16;i++)
        {
                SetVolume(i,vol);
        }
}

void    SetVolume(int ch,int vol)
{
        int     j;

        j=midi.channel[ch].volume/10;
        j*=vol;
        SendMPU(0xb0+ch);       //Control Change
        SendMPU(0x7b);          //Main Volume
        SendMPU(j);             //Value
}

word    MIDIport()
{
        word    i,p;
        p=0x330;
        for(i=0;i<8;i+=2,p+=2)
        {
                InitMPUport(p);
                if(!CtrlMPU(0xff))      return p;
        }
        return -1;
}

void    SetContDown(int st)
{
        midi.countdown=st;
        if(!midi.countdown)
        {
                SetAllVolume(0);
                for(st=0;st<16;st++)
                {
                        SendMPU(0x90+st);
                        SendMPU(0x00);
                        SendMPU(0x00);
                }
        }
}
