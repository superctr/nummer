#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "portaudio.h"
#include <string.h>

#define SAMPLE_RATE 32000
/* 0.2 seconds */
#define SHORT_DELAY (SAMPLE_RATE*0.1)
#define LONG_DELAY (SAMPLE_RATE*0.6)
#define VERY_LONG_DELAY (SAMPLE_RATE*3)

/*
         Nummer
         a k a
    *****************
    * BRIGITA MK.II *
    *****************

  © 2015 Ian Karlsson (superctr)
  License: GPL

 Usage:
   Groups only
   nummer "<groups>"

   Groups and callup
   nummer -c "<callup>" <callup time in seconds> "<groups>"

   If needed, set output device like this
   nummer -o
    to get a list, then
   nummer -o "device" <rest of options>
    to use this device

 Todo:
   de-hardcode all the sample info...
*/

int verbose_mode = 0;

typedef struct {
    int16_t * start_sample;
    int16_t * end_sample;
} Sample;
typedef struct {
    uint32_t start_sample;
    uint32_t end_sample;
} SampleOffset;

SampleOffset sample_offset_table [] = {
    {320,20918},        //0x00 0
    {20918,41196},      //0x01
    {41196,61409},      //0x02
    {61409,81631},      //0x03
    {81631,101420},     //0x04
    {101420,118894},    //0x05 1
    {118894,135641},    //0x06
    {135641,152362},    //0x07
    {152362,169107},    //0x08
    {169107,183571},    //0x09
    {183571,201874},    //0x0a 2
    {201874,216376},    //0x0b
    {216376,230874},    //0x0c
    {230874,245374},    //0x0d
    {245374,258392},    //0x0e
    {258392,277122},    //0x0f 3
    {277122,295834},    //0x10
    {295834,314494},    //0x11
    {314494,333170},    //0x12
    {333170,352022},    //0x13
    {352022,371995},    //0x14 4
    {371995,393706},    //0x15
    {393706,415384},    //0x16
    {415384,437082},    //0x17
    {437082,459159},    //0x18
    {459159,480336},    //0x19 5
    {480336,500403},    //0x1a
    {500403,520440},    //0x1b
    {520440,540474},    //0x1c
    {540474,561620},    //0x1d
    {561620,582907},    //0x1e 6
    {582907,604273},    //0x1f
    {604273,625568},    //0x20
    {625568,646874},    //0x21
    {646874,667508},    //0x22
    {667508,689948},    //0x23 7
    {689948,707567},    //0x24
    {707567,725130},    //0x25
    {725130,742640},    //0x26
    {742640,760143},    //0x27
    {760143,778432},    //0x28 8
    {778432,796946},    //0x29
    {796946,815636},    //0x2a
    {815636,834141},    //0x2b
    {834141,852428},    //0x2c
    {852428,875401},    //0x2d 9
    {875401,895855},    //0x2e
    {895855,916278},    //0x2f
    {916278,936745},    //0x30
    {936745,955840},    //0x31
    {955840,982087},    //0x32 9
    {982087,1001152},   //0x33
    {1001152,1025365},  //0x34 5
    {1025365,1049296},  //0x35
    {1049296,1073221},  //0x36
    {1073221,1097240},  //0x37
    {1097240,1121511},  //0x38
    {1121511,1144391},  //0x39 attn
    {1144391,1164260},  //0x3a /
    {1164260,1179498},  //0x3b end
    {1179498,1195675},  //0x3c out
    {1195675,1214280},  //0x3d msg
    {1214280,1231702},  //0x3e rpt
    {1231702,1250142},  //0x3f rbt
    {1250142,1290829},  //0x40 eot
    {1290829,1381564},  //0x41 xp
};

Sample sample_table[127];

typedef struct {
    char letter;
    int sample_no[5];
    char* disp;
} NumberChar;

NumberChar char_table [] = {
    {'^',{0,1,2,3,4},"Attention "}, //at
    {'*',{57,57,57,57,57},"Attention "}, //at
    {'+',{59,59,59,59,59},"End "}, //end
    {',',{60,60,60,60,60},"Out "}, //out
    {'-',{62,62,62,62,62},"Repeat "}, //repeat
    {'?',{63,63,63,63,63},"Rebeat "}, //rebeat
    {'/',{58,58,58,58,58},"/"}, //oblique

    {'0',{0,1,2,3,4},"0"},
    {'1',{5,6,7,8,9},"1"},
    {'2',{10,11,12,13,14},"2"},
    {'3',{15,16,17,18,19},"3"},
    {'4',{20,21,22,23,24},"4"},
    {'5',{52,53,54,55,56},"5"},
    {'6',{30,31,32,33,34},"6"},
    {'7',{35,36,37,38,39},"7"},
    {'8',{40,41,42,43,44},"8"},
    {'9',{51,51,51,51,51},"9"},

    {'=',{61,61,61,61,61},"Message"}, //message
    {'@',{65,65,65,65,65},"(Windows XP) "}, //XP
    {'~',{64,64,64,64,64},"End of transmission "}, //eot
};

enum PlayerState_State {
    STATE_PLAYING,
    STATE_WAITING,
    STATE_DONE,
};

enum CommandType {
    CMD_STOP = 0,
    CMD_PLAY = 1,
    CMD_WAIT = 2,
    CMD_SET_COUNTER = 3,
};

typedef struct {
    enum CommandType type;
    int value;
    int delay;
    int flag;
    char* string;
} Command;

typedef struct {
    enum PlayerState_State state;
    int pc;
    Command* command;
    int16_t* sample_pos;
    int16_t* sample_end;
    int waitcounter;
} PlayerState;

PaDeviceIndex GetAudioDeviceFromString(char* name, int isinput)
{
    PaDeviceIndex devid = 0;
    const PaDeviceInfo *dev;
    for(devid=0;devid<Pa_GetDeviceCount();devid++)
    {
        dev = Pa_GetDeviceInfo(devid);
        if(((dev->maxInputChannels > 0 && isinput==1) || (dev->maxOutputChannels > 0 && isinput==0))
            && !strcmp(name,dev->name))
        {
            return devid;
        }
    }
    return -1;
}

void PrintAudioDevices(int isinput)
{
    PaDeviceIndex devid = 0;
    const PaDeviceInfo *dev;
    for(devid=0;devid<Pa_GetDeviceCount();devid++)
    {
        dev = Pa_GetDeviceInfo(devid);
        if((dev->maxInputChannels > 0 && isinput==1) || (dev->maxOutputChannels > 0 && isinput==0))
            printf("\"%s\"\n",dev->name);
    }
    return;
}

static int audio_callback(const void* input, void* output, unsigned long length,
                         const PaStreamCallbackTimeInfo* patime, PaStreamCallbackFlags flags,
                         void* userdata)
{
    PlayerState* ps = userdata;
    float* out = output;
    float s;
    int i;
    for(i=0;i<length;i++)
    {
        switch(ps->state)
        {
        case STATE_PLAYING:
            s = ((float)*(ps->sample_pos)/65535)*0.75;
            *out = s;
            out++;
            ps->sample_pos++;
            if(ps->sample_pos == ps->sample_end)
            {
                ps->state = STATE_WAITING;
            }
            break;
        case STATE_WAITING:
            *out = 0;
            out++;
            ps->waitcounter--;
            if(ps->waitcounter<=0)
            {
                if(verbose_mode>1)
                    printf("Command = %d, %d\n",ps->command->type,ps->command->value);
                switch(ps->command->type)
                {
                case CMD_STOP:
                    ps->state = STATE_DONE;
                    break;
                case CMD_WAIT:
                    ps->state = STATE_WAITING;
                    ps->waitcounter = ps->command->delay;
                    if(verbose_mode)
                        printf("Delay %d samples\n",ps->waitcounter);
                    else
                        printf(" ");
                    break;
                case CMD_PLAY:
                    if(verbose_mode)
                        printf("Sample %d : %s\n",ps->command->value,ps->command->string );
                    else
                        printf("%s",ps->command->string );
                    ps->sample_pos = sample_table[ps->command->value].start_sample;
                    ps->sample_end = sample_table[ps->command->value].end_sample;
                    ps->waitcounter = SHORT_DELAY;
                    ps->state = STATE_PLAYING;
                    break;
                default:
                    break;
                }
                ps->command++;
            }
            break;
        case STATE_DONE:
        default:
            break;

        }
    }

    return 0;
}

int get_char_id(char id)
{
    int a;
    for(a=1;a<sizeof(char_table)/sizeof(NumberChar);a++)
    {
        if(char_table[a].letter == id)
            return a;
    }
    return -1;
}

/*
 * textp = pointer to char
 * cmd = pointer to commands
 * repeat = 1 if to repeat
 * repeattime = time to repeat
 */
Command* create_commands(char* textp, Command* cmd, int repeat, float repeattime)
{
    int counter=0;
    int len=0;
    char* text;
rpt:
    text = textp;
    while(*text != 0)
    {
        if(*text == ' ')
        {
            cmd->type = CMD_WAIT;
            cmd->delay = LONG_DELAY;
            cmd++;
            counter=0;
            len += LONG_DELAY;
        }
        else if(*text == '_')
        {
            cmd->type = CMD_WAIT;
            cmd->delay = VERY_LONG_DELAY;
            cmd++;
            counter=0;
            len += VERY_LONG_DELAY;
        }
        else if(get_char_id(*text) != -1)
        {
            cmd->type = CMD_PLAY;
            cmd->delay = SHORT_DELAY;
            if(get_char_id(*(text+1)) == -1)
            {
                counter=4;
            }

            cmd->value = char_table[get_char_id(*text)].sample_no[counter];
            cmd->string = char_table[get_char_id(*text)].disp;

            len += SHORT_DELAY + (sample_offset_table[cmd->value].end_sample-sample_offset_table[cmd->value].start_sample);
            counter++;
            if(counter>3)
                counter=3;
            cmd++;
        }
        else
            counter=0;
        text++;
    }

    cmd->type = CMD_WAIT;
    cmd->delay = LONG_DELAY;
    cmd++;

    if(repeat)
    {
        /* insert a space first */
        counter=0;
        repeat++;
        if(len < (repeattime*SAMPLE_RATE))
        goto rpt;
    }

    if(verbose_mode>1 && repeat)
        printf("Number of repeats = %d\n",repeat);

    cmd->type = CMD_STOP;
    return cmd;
}

int main(int argc, char *argv[])
{
    PaError pa = Pa_Initialize();
    if(pa!=paNoError)
    {
        printf("PortAudio initialization failed (%s)",Pa_GetErrorText(pa));
        return -1;
    }
    int res,a;
    PaDeviceIndex pa_output_device = Pa_GetDefaultOutputDevice();
    FILE* samplefile;
    /* Load sample file */
    samplefile = fopen("sample.bin","rb");
    if(!samplefile)
    {
        printf("Could not open sample file!\n");
        exit(EXIT_FAILURE);
    }
    fseek(samplefile,0,SEEK_END);
    int filesize = ftell(samplefile);
    rewind(samplefile);
    int16_t* sampledata = (int16_t*)malloc(filesize);
    res = fread(sampledata,1,filesize,samplefile);
    if(res != filesize)
    {
        printf("Reading error\n");
        exit(EXIT_FAILURE);
    }
    fclose(samplefile);

    /* Add sample offset */
    for(a=0;a<sizeof(sample_offset_table)/sizeof(SampleOffset);a++)
    {
        sample_table[a].end_sample = sampledata + sample_offset_table[a].end_sample;
        sample_table[a].start_sample = sampledata + sample_offset_table[a].start_sample ;
    }

    Command* commands = (Command*)malloc(sizeof(Command)*128000);
    Command* cmds = commands;

    if(argc<2)
    {
        printf("Number station voice machine\n\n");
        printf("usage:\n");
        printf("play groups only - %s \"<groups>\"\n",argv[0]);
        printf("play with call up - %s -c \"<callup>\" <length> \"<groups>\"\n\t(length in seconds)\n",argv[0]);
        printf("example:\n");
        printf("%s \"01234 56789\"\n",argv[0]);
        printf("%s -c 250 120 \"01234 56789\"\n",argv[0]);
        printf("\t(plays '250' for 120 seconds)\n");
        printf("%s -c 250 120 -c 02 60 \"01234 56789\"\n",argv[0]);
        printf("\t(plays '250' for 200 seconds, then '02' for 60 seconds, then groups)\n");
        exit(EXIT_FAILURE);
    }
    int argctr=1, badusage=0;

    while(argctr<argc)
    {
        if(verbose_mode>1)
            printf("%s\n",argv[argctr]);
        /* Check options */
        /* Audio output */
        if((!strcmp(argv[argctr],"-o") || !strcmp(argv[argctr],"--output")) && argctr == argc-1)
        {
            printf("No output specified, showing list of devices:\n\n");

            PrintAudioDevices(0);
            printf("\nTo use a device: %s --output \"device name\" ...\n",argv[0]);
            exit(EXIT_FAILURE);
        }
        else if((!strcmp(argv[argctr],"-o") || !strcmp(argv[argctr],"--output")) && (argctr+1) < argc)
        {
            PaDeviceIndex gotdevid = GetAudioDeviceFromString(argv[argctr+1],0);
            if(gotdevid == -1)
            {
                printf("No output device was found with the name \"%s\"\n",argv[argctr+1]);
                printf("To get a list of devices: %s --output\n",argv[0]);
                exit(EXIT_FAILURE);
            }
            else
            {
                pa_output_device = gotdevid;
            }
            argctr++;
        }
        /* verbose mode */
        else if(!strcmp(argv[argctr],"-vv"))
            verbose_mode=2;
        else if(!strcmp(argv[argctr],"-v"))
            verbose_mode=1;
        /* call up */
        else if((argctr+2) < argc && !strcmp(argv[argctr],"-c"))
        {
            argctr++;
            cmds = create_commands(argv[argctr],cmds,1,atof(argv[argctr+1]));
            argctr++;
        }
        /* group/message */
        else
        {
            cmds = create_commands(argv[argctr],cmds,0,0);
        }
        argctr++;
    }

    if(badusage)
        exit(EXIT_FAILURE);
    /*
    if(argctr < argc)
        create_commands(argv[argctr],cmds,0,0);
    else
        create_commands(test_count,cmds,0,0);
    */

    PlayerState state = {STATE_WAITING,0,commands,sampledata,sampledata,1};
    /****/

    PaStreamParameters output = {
        pa_output_device,
        1,
        paFloat32,
        0.1,
        NULL
    };

    PaStream *stream;
    PaError err = Pa_OpenStream( &stream, NULL, &output, SAMPLE_RATE, paFramesPerBufferUnspecified,
                                       paNoFlag, audio_callback, &state );

    if(err != paNoError)
    {
        printf("Could not open stream (%s)",Pa_GetErrorText(err));
        return -1;
    }

    printf("Playing audio\n");
    err=Pa_StartStream(stream);
    if(err!=paNoError)
    {
        printf("Could not start stream (%s)",Pa_GetErrorText(err));
        return -1;
    }
    while(state.state != STATE_DONE)
    {
        Pa_Sleep(10);
    }
    err=Pa_StopStream(stream);
    /****/

    free(sampledata);
    free(commands);
    return 0;
}

