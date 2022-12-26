#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if _MSC_VER < 1400
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
#else
#include <stdint.h>
#endif

#define THRESHOLD   128         // Amount of minimum samples required for a repeating loop
#define GAP_THRESHOLD  200      // max gap between samples before splitting
#define DETECTION_MODE 1        // 0= Check for exact matches, 1= Use a tolerance margin
#define DETECTION_TOLERANCE 3   // Tolerance margin
#define STEP 1                 // Skip Step. larger values sacrifice optimization for speed
#define DEBUG 0                 // Verbose
#define SPECIAL_MODE 0

    int g_v = DEBUG;
    int g_detection_tolerance = DETECTION_TOLERANCE;
    int g_detection_mode = DETECTION_MODE;
    int g_threshold = THRESHOLD;
    int g_gap = GAP_THRESHOLD;
    int g_step = STEP;
    int g_specialmode = 0;
    char* file1;
    char* file2;
    uint8_t* fileptr;
    int g_counter;
    int g_samplecounter;

// Increments destination pointer
void my_memcpy(uint8_t** dest, void* src, int size)
{
    memcpy(*dest,src,size);
    *dest += size;
}

void add_datablockcmd(uint8_t** dest, uint8_t dtype, uint32_t size)
{
    **dest = 0x67;*dest+=1;
    **dest = 0x66;*dest+=1;
    **dest = dtype;*dest+=1;
    my_memcpy(dest,&size,4);
}

// cmd=90 or 91
void add_setupcmd(uint8_t** dest,  uint8_t cmd, uint8_t sid, uint8_t d1,uint8_t d2,uint8_t d3)
{
    **dest = cmd;*dest+=1;
    **dest = sid;*dest+=1;
    **dest = d1;*dest+=1;
    **dest = d2;*dest+=1;
    **dest = d3;*dest+=1;
}

void add_freqcmd(uint8_t** dest, uint8_t sid, uint32_t freq)
{
    **dest = 0x92;*dest+=1;
    **dest = sid;*dest+=1;
    my_memcpy(dest,&freq,4);
}

void add_startcmd(uint8_t** dest, uint8_t sid, uint32_t spos, uint8_t flag, uint32_t len)
{
    **dest = 0x93;*dest+=1;
    **dest = sid;*dest+=1;
    my_memcpy(dest,&spos,4);
    **dest = flag;*dest+=1;
    my_memcpy(dest,&len,4);
}

void add_delay(uint8_t** dest, int delay)
{
    int i;

    int commandcount = delay/65535;
    uint16_t finalcommand = delay%65535;

    if(commandcount)
    {
        for(i=0;i<commandcount;i++)
        {
            **dest = 0x61;*dest+=1;
            **dest = 0xff;*dest+=1;
            **dest = 0xff;*dest+=1;
        }
    }

    if(finalcommand == 735)
    {
        **dest = 0x62;*dest+=1;
    }
    else if(finalcommand == 882)
    {
        **dest = 0x63;*dest+=1;
    }
    else if(finalcommand > 0 && finalcommand < 17)
    {
        **dest = 0x6f+finalcommand;*dest+=1;
    }
    else if(finalcommand > 0)
    {
        **dest = 0x61;*dest+=1;
        my_memcpy(dest,&finalcommand,2);
    }

}

int get_match_len (uint16_t* a, uint16_t* b, int maxlen, uint32_t* c, uint8_t* d)
{
    int i;
    for(i=0;i<maxlen;i++)
    {
        if(g_detection_mode == 0 && (a[i] != b[i]))
                break;
        else if(g_detection_mode == 1 && (a[i] > (b[i]+g_detection_tolerance) || a[i] < (b[i]-g_detection_tolerance)) )
                break;
        if(c[i] > 200)
            break;
        if(d[i] > 0)
            break;
    }
    return i;
}

int get_imatch_len (uint8_t* a, uint8_t b, int maxlen)
{
    int i;
    for(i=0;i<maxlen;i++)
    {
        if(a[i] != b)
            break;
    }
    return i;
}

// ========================================================================= //

typedef enum {

    YM2612,
    PWM,
    PWM_1,
    HUC6280,
    HUC6280_1,
    HUC6280_2,
    HUC6280_3,
    HUC6280_4,
    HUC6280_5,
    DAC_COUNT,

} _dac_id;

char* dac_names[DAC_COUNT] =
{
    "YM2612",
    "PWM Ch 0",
    "PWM Ch 1",
    "HuC6280 Ch 0",
    "HuC6280 Ch 1",
    "HuC6280 Ch 2",
    "HuC6280 Ch 3",
    "HuC6280 Ch 4",
    "HuC6280 Ch 5"
};

typedef enum {

    DAC_YM2612,
    DAC_PWM,
    DAC_HUC6280,

} _dactype;

typedef struct {
    int SourceStart;    // write no.
    int SourceEnd;      // write no.
    int RepeatStart;    // write no.
    int RepeatEnd;      // write no.
    int RepeatStartB;   // repeat start backup.
    int NumSamples;
    int Frequency;
} _dacevent;

// dstart, dend, rstart, rend, sdel, slen
_dacevent AddStream(int dstart, int dend, int rstart, int rend, uint32_t* sampledelay )
{
    int delaysum=0, i=0, freq=0;
    double factor=0;

    for(i=dstart+1;i<dend;i++)
        delaysum += sampledelay[i];
    factor=(double)(dend-dstart)/delaysum;
    freq=factor*44100;
    if(g_v > 1)
        printf("ds=%d, de=%d, rs=%d, re=%d, freq=%d\n",dstart,dend,rstart,rend,freq );

    {
        _dacevent res = {
            dstart,       // vgm dac sample position start
            dend,         // vgm dac sample position end
            rstart,       // repeat (datablock) start
            rend,         // repeat (datablock) end
            rstart,       // backup
            dend-dstart,  // length
            freq          // freq
        };

        return res;
    }
}


typedef struct {

    int enabled;

    _dactype type;
    int channel;    // for HuC6280

    uint16_t *samples;
    uint32_t *sampledelay;
    uint8_t *sampleusage;
    //uint8_t **vgmptrs;
    int samplecount;
    _dacevent *events;
    int eventcount;
    int streamid;   // used for datablocks
    int chipid;     // used for display only
    int samplelen;
    // all temp variables
    int lastsample;
    int totalused;
    int streamdone;
    int eventpos;
    int writecounter;
    int writecounter2;  // used to cualculate offset for loop

} _dacobject;

// initializes an empty _dacobject
_dacobject dac_init(_dactype type, int channel)
{
    _dacobject d = {
        0,
        type,
        channel,
        NULL,
        NULL,
        NULL,
        0,
        NULL,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    };

    return d;
}

// Setups a _dacobject
void dac_enable(_dacobject *o, int samples)
{
    if(o->enabled)
        return;
    o->enabled = 1;
    o->samples = (uint16_t*) malloc (samples*2);
    o->sampledelay = (uint32_t*) malloc (samples*4);
    o->sampleusage = (uint8_t*) malloc (samples);
    o->events = (void*) malloc (sizeof(_dacobject)*32768);
    memset(o->events, 0, sizeof(_dacobject)*32768);

    printf("%s enabled\n",dac_names[o->chipid]);
}

// DAC write (when reading a VGM)
void dac_write(_dacobject *o, int sample, uint16_t data, int numsamples)
{
    if(!o->enabled)
        dac_enable(o, numsamples);

    o->samples[o->samplecount] = data;
    o->sampledelay[o->samplecount] = sample - o->lastsample;
    o->lastsample = sample;
    o->sampleusage[o->samplecount] = 0;
    o->samplecount++;
}

// kills  a _dacobject
void dac_disable(_dacobject *o)
{
    if(!o->enabled)
        return;
    o->enabled = 0;
    free(o->samples);
    free(o->sampledelay);
    free(o->sampleusage);
    free(o->events);
}


// ---------------------------------------------------------------------------- //
// Read DAC logs, check for repetitions
void dac_optimize(_dacobject *o) // startpos = 0
{
    int i,j,l,m, res=0,  lastused=0, counter=0;

    printf("%s: Optimizing...\n",dac_names[o->chipid]);

    if(!o->enabled)
        return;

    o->samples[o->samplecount] = 0;
    o->sampledelay[o->samplecount] = g_threshold+1;

    for(i=0+g_threshold;i<o->samplecount;i+=g_step)
    {
        for(j=0;j<i;j++)
        {
            res = get_match_len(&o->samples[i], &o->samples[j], i-j, &o->sampledelay[i], &o->sampleusage[j]);

            if(res > g_threshold)
            {
                // Check for any one-off samples
                if(i>(lastused+g_threshold))
                {
                    for(l=lastused+1;l<=i;l++)
                    {
                        // Try to split sample blocks where a large delay occurs. This should help prevent
                        // wrong pitches.
                        if(o->sampledelay[l] > g_gap || l==i)
                        {
                            if(l-lastused > g_threshold)
                            {
                                if(g_v>1)
                                    printf("Single ");

                                o->events[o->eventcount] = AddStream(lastused,l,lastused,l,o->sampledelay);
                                o->eventcount++;
                            }
                            lastused=l;
                        }
                    }

                }

                m=i;
                for(l=0;l<res;l++)
                {
                    // Try to split sample blocks where a large delay occurs. This should help prevent
                    // wrong pitches.
                    if(o->sampledelay[i+l+1] > g_gap || (l+1)==res)
                    {
                        if(l > g_threshold)
                        {
                            if(g_v>1)
                                printf("Match! ");

                            o->events[o->eventcount] = AddStream(i,i+l,j,j+l,o->sampledelay);
                            memset(o->sampleusage+i, 0x01, l);
                            o->eventcount++;
                        }
                        i+=l;
                    }
                }
                i=m+res;
                lastused=i;
                break;
            }
        }
        counter++;
        if(counter%64 == 0)
            fprintf(stderr,"\r%s: %10d (%d events)              ",dac_names[o->chipid], i,o->eventcount);
    }

    if(i>(lastused+g_threshold))
    {
        for(l=lastused+1;l<=i;l++)
        {
            // Try to split sample blocks where a large delay occurs. This should help prevent
            // wrong pitches.
            if(o->sampledelay[l] > g_gap || l==i)
            {
                if(l-lastused > g_threshold)
                {
                    if(g_v>1)
                        printf("Final  ");

                    o->events[o->eventcount] = AddStream(lastused,l,lastused,l,o->sampledelay);
                    o->eventcount++;
                }
                lastused=l;
            }
        }
    }

    printf("\r%s: %d events detected\t\t\n",dac_names[o->chipid],o->eventcount);
}

// ---------------------------------------------------------------------------- //
// Calculate block usage, relocations
void dac_calculate_usage(_dacobject *o)
{
    int i,j,k,totalused=0,totalunused=0,res;

    printf("%s: Calculating Usage...\n",dac_names[o->chipid]);

    //uint8_t* blockusagemask;
    //blockusagemask = (uint8_t*) malloc(o->samplecount);

    memset(o->sampleusage,0,o->samplecount);

    for(i=0;i<o->eventcount;i++)
    {
        for(j=o->events[i].RepeatStart;j<=o->events[i].RepeatEnd;j++)
        {
            o->sampleusage[j-1] |= 0x01;
        }
    }

    if(g_v>0)
    {
        printf("\n\nStart      End        Length     Status\n");
        printf(    "========== ========== ========== ==========\n");
    }

    for(i=0;i<o->samplecount;i++)
    {
        res=get_imatch_len(o->sampleusage+i, 0x01, o->samplecount-i);
        if(res > 0)
        {
            if(g_v > 0)
                printf ("%10d %10d %10d USED\n", i, i+res, res);

            k=0;
            for(j=0;j<o->eventcount;j++)
            {
                if(o->events[j].RepeatStartB >= i && o->events[j].RepeatStartB < i+res )
                {
                    o->events[j].RepeatStart = o->events[j].RepeatStart - i + totalused;
                    o->events[j].RepeatEnd = o->events[j].RepeatEnd - i + totalused;
                    k++;
                }
            }
            memmove(&o->samples[totalused], &o->samples[i], res*2);

            if(g_v>1)
            {
                printf("(%d events relocated, %d bytes moved) \n",k,res);
                printf("%08x moved to %08x\n", i, totalused);
            }

            i+=res;
            totalused += res;
        }

        res=get_imatch_len(o->sampleusage+i, 0x00, o->samplecount-i);
        if(res > 0)
        {
            if(g_v > 0)
                printf ("%10d %10d %10d UNUSED\n", i, i+res, res);

            i+=res;
            totalunused += res;

        }
    }
    printf("\n\nTotal Used: %d, Total Unused: %d\n", totalused,totalunused);
    o->totalused = totalused;

    if(g_v>1)
    {
        for(j=0;j<o->eventcount;j++)
        {
            printf("block %03d: %08x - %08x, old = %08x\n",j, o->events[j].RepeatStart, o->events[j].RepeatEnd, o->events[j].RepeatStartB );
        }
    }
}


// ---------------------------------------------------------------------------- //
// DAC datablock setup (when writing a VGM)
void dac_setup(_dacobject *o) // writedb = 1, dbid = g_counter
{
    int i;

    if(!o->enabled)
        return;

    printf("%s: Writing Databank (ID = %d)...\n",dac_names[o->chipid], g_counter);

    switch(o->type)
    {
    case DAC_YM2612:
    case DAC_HUC6280:


        add_datablockcmd(&fileptr, g_counter, o->totalused);

        //my_memcpy(&fileptr,o->samples,o->totalused);
        for(i=0;i<o->totalused;i++)
        {
            *fileptr++ = o->samples[i] & 0xff;
        }

        if(o->type == DAC_YM2612)
            add_setupcmd(&fileptr, 0x90,g_counter,0x02,0x00,0x2a);
        else if(o->type == DAC_HUC6280)
            add_setupcmd(&fileptr, 0x90,g_counter,0x1b,o->channel, 0x06);

        add_setupcmd(&fileptr, 0x91,g_counter,g_counter,0x01,0x00);

        o->streamid = g_counter;
        o->samplelen = 1;
        g_counter++;
        break;

    case DAC_PWM:


        add_datablockcmd(&fileptr, g_counter, o->totalused*2);
        my_memcpy(&fileptr,o->samples,o->totalused*2);

        add_setupcmd(&fileptr, 0x90,g_counter,0x11,0x00,o->channel + 2);
        add_setupcmd(&fileptr, 0x91,g_counter,g_counter,0x01,0x00);

        o->samplelen = 2;
        o->streamid = g_counter;
        g_counter++;

        break;

    default:
        printf("not yet supported\n");
        exit(EXIT_FAILURE);
        break;

    }

}

// ---------------------------------------------------------------------------- //
void update_delays()
{
    if(g_counter)
    {
        //printf("Delay %d samples\n",samplecounter);
        add_delay(&fileptr, g_counter);
        g_samplecounter += g_counter;
        g_counter=0;
    }
}


// ---------------------------------------------------------------------------- //
// rewrite commands during loops
void dac_setloop(_dacobject *o)
{
    if(!o->enabled)
        return;

    if(o->writecounter > o->events[o->eventpos].SourceStart && o->writecounter < o->events[o->eventpos].SourceEnd && !o->streamdone)
    {
        update_delays();
        *fileptr++ = 0x94;
        *fileptr++ = o->streamid;
        add_freqcmd(&fileptr,o->streamid,o->events[o->eventpos].Frequency);
        add_startcmd(&fileptr,o->streamid,(o->events[o->eventpos].RepeatStart+o->writecounter2)*o->samplelen,1,o->events[o->eventpos].NumSamples-o->writecounter2);
        //printf("Rewrote loop command (counter: %d).\nIf problems occur, try shifting the loop point %d samples to the left.\n",o->writecounter2,o->writecounter2+10);
    }
}

// ---------------------------------------------------------------------------- //
// DAC update (when writing a VGM)
// returns 1 if command was not processed (that is, optimization is disabled)
int dac_update(_dacobject *o)
{
    int writeok = 1;

    if(!o->enabled)
        return 1;

    // at the end of a stream
    if(o->writecounter == o->events[o->eventpos].SourceEnd && !o->streamdone)
    {
        writeok=0;

        update_delays();
        *fileptr++ = 0x94;
        *fileptr++ = o->streamid;
        o->eventpos++;

        if(o->eventpos == o->eventcount)
            o->streamdone=1;

    }
    // starting a stream
    if(o->writecounter == o->events[o->eventpos].SourceStart && !o->streamdone)
    {
        writeok=0;

        update_delays();
        add_freqcmd(&fileptr,o->streamid,o->events[o->eventpos].Frequency);
        add_startcmd(&fileptr,o->streamid,o->events[o->eventpos].RepeatStart*o->samplelen,1,o->events[o->eventpos].NumSamples);
        o->writecounter2=0;
    }
    // in the middle of a stream
    if(o->writecounter > o->events[o->eventpos].SourceStart && o->writecounter < o->events[o->eventpos].SourceEnd && !o->streamdone)
    {
        writeok=0;
    }

    o->writecounter++;
    o->writecounter2++;

    return writeok;
}

// ---------------------------------------------------------------------------- //
// ---------------------------------------------------------------------------- //

int OptimizeVGM()
{
    FILE *sourcefile, *destfile;
    unsigned long sourcefile_size;
    uint8_t *source, *dest;
    uint32_t numsamples, samplecounter;
    uint8_t *looppos, *srcptr, *endptr;
    uint32_t id3offset;
    int cpsize;
    int writeok;

    int res, endofdata=0, skip=0, i=0;
    uint8_t d, cmd, data;

    int hu_latch=0;                     // channel select latch for HuC6280
    int hu_flags[6]={0,0,0,0,0,0};     // flags registers for HuC6280

    _dacobject dac[2][DAC_COUNT];

    for(i=0;i<1;i++)
    {
        dac[i][YM2612] = dac_init(DAC_YM2612,0);
        dac[i][PWM] = dac_init(DAC_PWM,0);
        dac[i][PWM_1] = dac_init(DAC_PWM,1);
        dac[i][HUC6280] = dac_init(DAC_HUC6280,0);
        dac[i][HUC6280_1] = dac_init(DAC_HUC6280,1);
        dac[i][HUC6280_2] = dac_init(DAC_HUC6280,2);
        dac[i][HUC6280_3] = dac_init(DAC_HUC6280,3);
        dac[i][HUC6280_4] = dac_init(DAC_HUC6280,4);
        dac[i][HUC6280_5] = dac_init(DAC_HUC6280,5);
    }
    for(i=0;i<DAC_COUNT;i++)
        dac[0][i].chipid = i;


// ---------------------------------------------------------------------------- //
// Open input file

    sourcefile = fopen(file1,"rb");
    if(!sourcefile)
    {
        printf("Could not open %s\n",file1);
        exit(EXIT_FAILURE);
    }
    fseek(sourcefile,0,SEEK_END);

    sourcefile_size = ftell(sourcefile);

    rewind(sourcefile);

    source = (uint8_t*)malloc(sourcefile_size);
    dest = (uint8_t*)malloc(sourcefile_size*2);

    res = fread(source,1,sourcefile_size,sourcefile);
    if(res != sourcefile_size)
    {
        printf("Reading error\n");
        exit(EXIT_FAILURE);
    }
    fclose(sourcefile);

    numsamples = (*(uint32_t*)(source+0x18));
    looppos = source + (*(uint32_t*)(source+0x1c)) + 0x1c;
    srcptr = source + (*(uint32_t*)(source+0x34)) + 0x34;
    endptr = source+sourcefile_size-5;


    printf("Number of VGM samples = %d\n",numsamples);

    samplecounter=0;

// ---------------------------------------------------------------------------- //
// Log DAC writes

    while((srcptr < endptr) && !endofdata)
    {
        d = *(srcptr);
        cmd = *(srcptr+1);
        data = *(srcptr+2);

        //printf("%p %02x\n", srcptr - source, d);

        if(d==0x66)
            endofdata = 1;
        // YM2612 DAC
        else if(d==0x52 && cmd==0x2a)
        {
            //printf("dac %p  %10d = %02x, %d\n", srcptr - source, dac[0][YM2612].samplecount, *(srcptr+2), samplecounter);
            dac_write(&dac[0][YM2612], samplecounter, data, numsamples);
            srcptr+=2;
        }

        // HuC6280 latch
        else if(d==0xb9 && cmd==0x00)
        {
            data &= 0x07;
            if(data < 7)
                hu_latch=data;
            srcptr+=2;
        }
        // HuC6280 flags
        else if(d==0xb9 && cmd==0x04)
        {
            hu_flags[hu_latch]=data;
            srcptr+=2;
        }
        // HuC6280 DAC
        else if(d==0xb9 && cmd==0x06 && hu_flags[hu_latch]&0x40 )
        {
            dac_write(&dac[0][HUC6280+hu_latch], samplecounter, data, numsamples);
            srcptr+=2;
        }

        // PWM
        else if(d==0xb2 )
        {
            if(((cmd&0xf0) == 0x20) || ((cmd&0xf0) == 0x40))
                dac_write(&dac[0][PWM], samplecounter, (cmd&0x0f)<<8|data, numsamples);
            if(((cmd&0xf0) == 0x30) || ((cmd&0xf0) == 0x40))
                dac_write(&dac[0][PWM_1], samplecounter, (cmd&0x0f)<<8|data, numsamples);
            srcptr+=2;
        }

        else if (d==0x67)
        {
            skip = *(uint32_t*)(srcptr+3) & 0x0FFFFFFF;
            srcptr += skip+6;
        }
        else if ((d > 0x6f) && (d < 0x80))
        {
            samplecounter += (d & 0x0f)+1;
        }
        else if (d==0x61)
        {
            samplecounter += *(uint16_t*)(srcptr+1) & 0xFFFF;
            srcptr += 2;
        }
        else if (d==0x62)
            samplecounter += 735;
        else if (d==0x63)
            samplecounter += 882;
        else if(d == 0x4f || d == 0x50)
            srcptr++;
        else if((d>0x50 && d<0x62) || (d>0x9f && d<0xc0))
            srcptr+=2;
        else if(d>0xbf && d<0xe0)
            srcptr+=3;
        srcptr++;
    }

// ---------------------------------------------------------------------------- //
// Optimize DAC writes

    srcptr = source + (*(uint32_t*)(source+0x34)) + 0x34;
    endptr = source + (*(uint32_t*)(source+0x4)) + 0x4;
    fileptr = dest;

    cpsize=srcptr-source;
    my_memcpy(&fileptr,source,cpsize);

    g_counter=0;

    for(i=0;i<DAC_COUNT;i++)
    {
        if(dac[0][i].enabled)
        {
            if(dac[0][i].samplecount < g_threshold)
            {
                printf("%s: Amount of samples (%d) below threshold. Optimization not possible.\n",dac_names[i],dac[0][i].samplecount);
                dac_disable(&dac[0][i]);
            }
            else
            {
                printf("%s: %d samples read\n",dac_names[i],dac[0][i].samplecount);
                dac_optimize(&dac[0][i]);
                dac_calculate_usage(&dac[0][i]);
                dac_setup(&dac[0][i]);
            }
        }
    }

// ---------------------------------------------------------------------------- //
// Write output file
    endofdata = 0;
    g_counter=0;
    hu_latch=0;
    memset(hu_flags,0,6);
    g_samplecounter=0;

    while((srcptr < endptr) && !endofdata)
    {
        uint8_t d, cmd, data;

        //printf("srcptr = %p\n",srcptr-source);
        writeok=0;
        skip = 0;

        if(srcptr == looppos)
        {
            *(uint32_t*)(dest+0x1c) = (fileptr-dest)-0x1c;
            *(uint32_t*)(dest+0x20) = samplecounter-g_samplecounter;
            
            for(i=0;i<DAC_COUNT;i++)
                dac_setloop(&dac[0][i]);
        }

        d = *(srcptr);
        cmd = *(srcptr+1);
        data = *(srcptr+2);

        switch(d)
        {
        case 0x61:
            g_counter += *(uint16_t*)(srcptr+1) & 0xFFFF;
            skip += 2;
            break;
        case 0x62:
            g_counter += 735;
            break;
        case 0x63:
            g_counter += 882;
            break;
        case 0x66:
            writeok=1;
            endofdata = 1;
            break;
        default:
            if ((d > 0x6f) && (d < 0x80))
            {
                g_counter += (d & 0x0f)+1;
            }
            else
            {
                writeok=1;

                //if(d==0xb9 && cmd==0x06 && hu_latch == 0x01 )
                //{
                //    printf("data %02hhx writen to ch 1 at %d\n",data, g_samplecounter+g_counter );
                //}

                // Datablock (eventually we'll remove old YM2612 datablocks from optvgm)
                if (d==0x67)
                {
                    skip = *(uint32_t*)(srcptr+3) & 0x0FFFFFFF;
                    skip += +6;
                }
                // YM2612 DAC update
                else if(d==0x52 && cmd==0x2a)
                {
                    writeok = dac_update(&dac[0][YM2612]);
                    skip+=2;
                }

                // HuC6280 latch
                else if(d==0xb9 && cmd==0x00)
                {
                    data &= 0x07;
                    if(data < 7)
                        hu_latch=data;
                    skip+=2;
                }
                // HuC6280 flags
                else if(d==0xb9 && cmd==0x04)
                {
                    hu_flags[hu_latch]=data;
                    skip+=2;
                }
                // HuC6280 DAC
                else if(d==0xb9 && cmd==0x06 && (hu_flags[hu_latch]&0x40)==0x40 )
                {
                    writeok = dac_update(&dac[0][HUC6280+hu_latch]);
                    skip+=2;
                }

                else if(d==0xb2 )
                {

                    if((cmd&0xf0) == 0x40)
                    {
                        if(writeok)
                        {
                            update_delays();
                            *fileptr++ = 0xb2;
                            *fileptr++ = 0x20 | (cmd&0x0f);
                            *fileptr++ = data;
                        }

                        writeok = dac_update(&dac[0][PWM_1]);
                        if(writeok)
                        {
                            update_delays();
                            *fileptr++ = 0xb2;
                            *fileptr++ = 0x30 | (cmd&0x0f);
                            *fileptr++ = data;
                        }

                        writeok=0;
                    }
                    else if((cmd&0xf0) == 0x20)
                        writeok = dac_update(&dac[0][PWM]);
                    else if((cmd&0xf0) == 0x30)
                        writeok = dac_update(&dac[0][PWM_1]);

                    skip+=2;
                }

                else if(d == 0x4f || d == 0x50)
                    skip++;
                else if((d>0x50 && d<0x62) || (d>0x9f && d<0xc0))
                    skip+=2;
                else if(d>0xbf && d<0xe0)
                    skip+=3;
            }
            break;
        }

        skip++;

        if(writeok)
        {
            if(g_counter)
            {
                //printf("Delay %d samples\n",samplecounter);
                g_samplecounter += g_counter;
                add_delay(&fileptr, g_counter);
                g_counter=0;
            }
            my_memcpy(&fileptr,srcptr,skip);
        }
        srcptr += skip;
    }

    id3offset = *(uint32_t*)(source+0x14);
    if(id3offset>0)
        *(uint32_t*)(dest+0x14) = (fileptr-dest)-0x14;

    my_memcpy(&fileptr,srcptr, sourcefile_size-(srcptr-source));
    *(uint32_t*)(dest+0x04) = (fileptr-dest)-0x04;

    destfile = fopen(file2,"wb");
    if(!destfile)
    {
        printf("Could not open %s\n",file2);
        exit(EXIT_FAILURE);
    }
    for(i=0;i<(fileptr-dest);i++)
    {
        putc(*(dest+i),destfile);
    }
    fclose(destfile);
    printf("Wrote %d bytes to %s\n",i,file2);

    free(dest);
    free(source);

    return 0;
}


int main(int argc, char* argv [])
{
    char tempfname[128];
    int i=1;

    printf("\nVGM DAC Optimizer by ctr (ver 0.10 built %s %s)\n", __DATE__,__TIME__);
    printf(  "========================\n");

    while(i<argc)
    {
        if(!strcmp(argv[i],"-v")) // verbose
            g_v = 1;
        else if(!strcmp(argv[i],"-vv")) // debug verbose
            g_v = 2;
        else if(!strcmp(argv[i],"-t") && (i+1)<argc) // threshold
        {
            g_threshold = atoi(argv[i+1]);
            i++;
        }
        else if(!strcmp(argv[i],"-s") && (i+1)<argc) // step
        {
            g_step = atoi(argv[i+1]);
            i++;
        }
        else if(!strcmp(argv[i],"-m") && (i+1)<argc) // margin
        {
            g_detection_tolerance = atoi(argv[i+1]);
            i++;
        }
        else if(!strcmp(argv[i],"-g") && (i+1)<argc) // gap
        {
            g_gap = atoi(argv[i+1]);
            i++;
        }
        //else if(!strcmp(argv[i],"-pwm")) // PWM special mode (got rid of it since it didn't work)
        //    g_specialmode = 1;
        else
            break;
        i++;
    }

    if(g_detection_tolerance > 0)
        g_detection_mode = 0;

    if(i+1>argc)
    {
        printf("Usage:\n\t%s [-v] [-vv] [-t %d] [-m %d] [-s %d] [-g %d] <file> [destination]\n\n",argv[0],g_threshold,g_detection_tolerance,g_step,g_gap);
        printf("Options:"
               "\t-v\tverbose\n"
               "\t-vv\teven more verbose (suggest redirecting output to a file)\n"
               "\t-t\tset sample count threshold\n"
               "\t-m\tset sample tolerance margin\n"
               "\t-s\tset step size\n"
               "\t-g\tset maximum sample gap\n"
               "\nKnown limitations:\n"
               "\tNo multichip support yet.\n"
               "\tDetection algorithm a bit greedy, only one pass done at optimization\n"
               );

        exit(EXIT_FAILURE);
    }
    else
        file1 = argv[i];

    if(i+2>argc)
    {
        memset(tempfname,0,sizeof(tempfname));
        memcpy(tempfname,file1,strcspn(file1,"."));
        strcat(tempfname,"_optimized.vgm");
        file2 = tempfname;
    }
    else
        file2 = argv[i+1];

    return OptimizeVGM();

}

