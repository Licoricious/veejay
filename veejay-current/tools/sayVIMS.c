/* sendVIMS - very simple client for VeeJay
 * 	     (C) 2002-2004 Niels Elburg <elburg@hio.hen.nl> 
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <veejay/vims.h>
#include <libvjnet/vj-client.h>
#include <libvjmsg/vj-common.h>
static int   interactive = 0;
static int   port_num = 3490;
static char  *filename = NULL;
static char  *group_name = NULL;
static char  *host_name = NULL;
static vj_client *sayvims = NULL;
static int colors = 0;
static int fd_in = 0; // stdin
static int single_msg = 0;
static int dump = 0;

/* count played frames (delay) */
static void vj_flush(int frames) { 
	int n = 0;
	char status[100];
	bzero(status,100);

	while(frames>0) {
		if( vj_client_poll(sayvims, V_STATUS ))
		{
			char sta_len[6];
			bzero(sta_len, 6 );
			int nb = vj_client_read( sayvims, V_STATUS, sta_len, 5 );
			if(sta_len[0] == 'V' )
			{
				int bytes = 0;
				sscanf( sta_len + 1, "%03d", &bytes );
				if(bytes > 0 )
				{
					bzero(status, 100);
					int n = vj_client_read(sayvims,V_STATUS,status,bytes);
					if( n )
					{
						if(dump) fprintf(stdout , "%s\n", status );
						frames -- ;
					}
					if(n == -1)
					{
						exit(0);
					}
				}
			}
		}
	}
}

static void Usage(char *progname)
{
	veejay_msg(VEEJAY_MSG_INFO, "Usage: %s [options] [messages]",progname);
	veejay_msg(VEEJAY_MSG_INFO, "where options are:");
	veejay_msg(VEEJAY_MSG_INFO, " -p\t\tVeejay port (3490)"); 
	veejay_msg(VEEJAY_MSG_INFO, " -g\t\tVeejay groupname (224.0.0.31)");
	veejay_msg(VEEJAY_MSG_INFO, " -h\t\tVeejay hostname (localhost)");
	veejay_msg(VEEJAY_MSG_INFO, " -m\t\tSend single message");
	veejay_msg(VEEJAY_MSG_INFO, " -c\t\tColored output");
	veejay_msg(VEEJAY_MSG_INFO, " -d\t\tDump status to stdout");
	veejay_msg(VEEJAY_MSG_INFO, "Messages to send to veejay must be wrapped in quotes");
	veejay_msg(VEEJAY_MSG_INFO, "You can send multiple messages by seperating them with a whitespace");  
	veejay_msg(VEEJAY_MSG_INFO, "Example: %s \"600:;\"",progname);
	veejay_msg(VEEJAY_MSG_INFO, "         (quit veejay)");
	veejay_msg(VEEJAY_MSG_INFO, "Example: echo \"%03d:;\" | %s ", VIMS_QUIT, progname);

	exit(-1);
}

static int set_option(const char *name, char *value)
{
	int err = 0;
	if(strcmp(name, "h") == 0 )
	{
		host_name = strdup(optarg);
		if(group_name) err ++;
	}
	else if(strcmp(name, "g") == 0)
	{
		if(host_name) err ++; 
		group_name = strdup( optarg );	
	}
	else if (strcmp(name, "p") == 0)
	{
		port_num = atoi(optarg);
	}
	else if (strcmp(name, "i") == 0)
	{
		interactive = 1;
	}
	else if (strcmp(name, "c") == 0)
	{	
		colors = 1;
	}
	else if (strcmp(name, "m") == 0 )
	{
		single_msg = 1;
	}
	else if(strcmp(name, "d") == 0)
	{
		dump = 1;
	}
	else err++;

	return err;
}

static char buf[65535];

static void human_friendly_msg( int net_id,char * buffer )
{

	char vims_msg[150];
	if(buffer==NULL)
		sprintf(vims_msg, "%03d:;", net_id );
	else
		sprintf( vims_msg, "%03d:%s;", net_id,buffer+1);

	vj_client_send(sayvims,V_CMD, vims_msg);
}
static int human_friendly_vims(char *buffer)
{
   if(buffer[0] == 'h' || buffer[0] == '?')
   {
	veejay_msg(VEEJAY_MSG_INFO, "vi\t\tOpen video4linux device");
	veejay_msg(VEEJAY_MSG_INFO, "li\t\tOpen vloopback device");
 	veejay_msg(VEEJAY_MSG_INFO, "fi\t\tOpen Y4M stream for input");
	veejay_msg(VEEJAY_MSG_INFO, "fo\t\tOpen Y4M stream for output");
	veejay_msg(VEEJAY_MSG_INFO, "lo\t\tOpen vloopback device for output");
	veejay_msg(VEEJAY_MSG_INFO, "mr\t\tOpen multicast receiver [address port]");
	veejay_msg(VEEJAY_MSG_INFO, "pr\t\tOpen unicast receiver [address port]");
	veejay_msg(VEEJAY_MSG_INFO, "av\t\tOpen file as stream using FFmpeg [filename]");
	veejay_msg(VEEJAY_MSG_INFO, "cl\t\tLoad samplelist from file");
	veejay_msg(VEEJAY_MSG_INFO, "cn\t\tNew sample from frames n1 to n2");
	veejay_msg(VEEJAY_MSG_INFO, "cd\t\tDelete sample n1");
	veejay_msg(VEEJAY_MSG_INFO, "sd\t\tDelete Stream n1");
	veejay_msg(VEEJAY_MSG_INFO, "cs\t\tSave samplelist to file");
	veejay_msg(VEEJAY_MSG_INFO, "es\t\tSave editlist to file");
	veejay_msg(VEEJAY_MSG_INFO, "ec\t\tCut frames n1 - n2 to buffer");
	veejay_msg(VEEJAY_MSG_INFO, "ed\t\tDel franes n1 - n2");
	veejay_msg(VEEJAY_MSG_INFO, "ep\t\tPaste from buffer at frame n1");
	veejay_msg(VEEJAY_MSG_INFO, "ex\t\tCopy frames n1 - n2 to buffer");
	veejay_msg(VEEJAY_MSG_INFO, "er\t\tCrop frames n1 - n2");
        veejay_msg(VEEJAY_MSG_INFO, "al\t\tAction file Load");
        veejay_msg(VEEJAY_MSG_INFO, "as\t\tAction file save");
	veejay_msg(VEEJAY_MSG_INFO, "sa\t\tToggle 4:2:0 -> 4:4:4 sampling mode");
	return 1;
   }

   if(strncmp( buffer, "vi",2 ) == 0 ) { human_friendly_msg( VIMS_STREAM_NEW_V4L, buffer+2); return 1; }
   if(strncmp( buffer, "fi",2 ) == 0 ) { human_friendly_msg( VIMS_STREAM_NEW_Y4M, buffer+2); return 1; }
   if(strncmp( buffer, "fo",2 ) == 0 ) { human_friendly_msg( VIMS_OUTPUT_Y4M_START,buffer+2); return 1; }
   if(strncmp( buffer, "cl",2 ) == 0 ) { human_friendly_msg( VIMS_SAMPLE_LOAD_SAMPLELIST, buffer+2); return 1;}
   if(strncmp( buffer, "cn",2 ) == 0 ) { human_friendly_msg( VIMS_SAMPLE_NEW, buffer+2); return 1;}
   if(strncmp( buffer, "cd",2 ) == 0 ) { human_friendly_msg( VIMS_SAMPLE_DEL, buffer+2); return 1;}
   if(strncmp( buffer, "sd",2 ) == 0 ) { human_friendly_msg( VIMS_STREAM_DELETE,buffer+2); return 1;}
   if(strncmp( buffer, "cs",2 ) == 0 ) { human_friendly_msg( VIMS_SAMPLE_SAVE_SAMPLELIST,buffer+2); return 1;}
   if(strncmp( buffer, "es",2 ) == 0 ) { human_friendly_msg( VIMS_EDITLIST_SAVE,buffer+2); return 1;}
   if(strncmp( buffer, "ec",2 ) == 0 ) { human_friendly_msg( VIMS_EDITLIST_CUT,buffer+2); return 1;}
   if(strncmp( buffer, "ed",2 ) == 0 ) { human_friendly_msg( VIMS_EDITLIST_DEL,buffer+2); return 1;}
   if(strncmp( buffer, "ep",2 ) == 0 ) { human_friendly_msg( VIMS_EDITLIST_PASTE_AT,buffer+2); return 1; }
   if(strncmp( buffer, "ex",2 ) == 0 ) { human_friendly_msg( VIMS_EDITLIST_COPY, buffer+2); return 1; }
   if(strncmp( buffer, "er",2 ) == 0 ) { human_friendly_msg( VIMS_EDITLIST_CROP, buffer+2); return 1; }   
   if(strncmp( buffer, "al",2 ) == 0 ) { human_friendly_msg( VIMS_BUNDLE_FILE,buffer+2); return 1; }
   if(strncmp( buffer, "as",2 ) == 0 ) { human_friendly_msg( VIMS_BUNDLE_SAVE,buffer+2); return 1; }
   if(strncmp( buffer, "de",2 ) == 0 ) { human_friendly_msg( VIMS_DEBUG_LEVEL, NULL); return 1;}
   if(strncmp( buffer, "be",2 ) == 0 ) { human_friendly_msg( VIMS_BEZERK,NULL); return 1;}
   if(strncmp( buffer, "sa",2 ) == 0 ) { human_friendly_msg( VIMS_SAMPLE_MODE,NULL); return 1;}
	if(strncmp(buffer, "mr",2 ) == 0 ) { human_friendly_msg( VIMS_STREAM_NEW_MCAST, buffer+2); return 1; }
	if(strncmp(buffer, "pr",2 ) == 0 ) { human_friendly_msg( VIMS_STREAM_NEW_UNICAST, buffer+2);
return 1;}
	if(strncmp(buffer, "av",2 ) == 0 ) { human_friendly_msg( VIMS_STREAM_NEW_AVFORMAT, buffer+2); return 1; }
   return 0;
}


vj_client	*sayvims_connect(void)
{
	vj_client *client = vj_client_alloc( 0,0,0 );
	if(host_name == NULL)
		host_name = strdup( "localhost" );

	if(!vj_client_connect( client, host_name,group_name, port_num ))
	{
		return NULL;
	}
	return client;
}

int main(int argc, char *argv[])
{
       	int i;
	int n = 0;
	int x = 0;
	int k =0;
	char msg[20];
	char option[2];
	char ibuf[1024];
	int err = 0;
	FILE *infile;
	// parse commandline parameters
	while( ( n = getopt(argc,argv, "h:g:p:micd")) != EOF)
	{
		sprintf(option,"%c",n);
		err += set_option( option,optarg);
	}

	veejay_set_colors(colors);

	if( optind > argc )
		err ++;
	if ( err )
		Usage(argv[0]);

	bzero( buf, 65535 );

	// make connection with veejay
	sayvims = sayvims_connect();
	if(!sayvims)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot connect to %s", host_name);
		exit(1);
	}
	veejay_msg(VEEJAY_MSG_INFO,
		"Connected to %s", host_name );			
	if(interactive)
	{
		fcntl( 0, F_SETFL, O_NONBLOCK);
		veejay_msg(VEEJAY_MSG_INFO, "Interactive mode");
	}

	while(interactive==1)
	{
		bzero(ibuf,1024);
		if(read(0, ibuf,255)>0)
		{
			int i;
			for(i=0; i < strlen(ibuf); i++)
			if(ibuf[i] == '\n') ibuf[i] = '\0';
			if(!human_friendly_vims(ibuf))
				vj_client_send(sayvims,V_CMD, ibuf);

		}
		vj_flush(1);
	}
	if ( interactive )
	{
		vj_client_close( sayvims );
		vj_client_free(sayvims );
		return 0;
	}
	if(single_msg || (optind == 1 && err == 0 && argc > 1 )) 
	{
		char **msg = argv + optind;
		int  nmsg  = argc - optind;
		i=0;
		while( i < nmsg )
		{
			if(msg[i][0] == '+')
			{
				int delay = 1;
				char *tmp = msg[i];
				if(sscanf(tmp + 1, "%d",&delay) == 1 )
				{
					vj_flush(delay);
				}
				else
				{
					veejay_msg(VEEJAY_MSG_ERROR, "Fatal: error parsing %s", tmp );
					exit(-1);
				}
			}
			else
			{
				vj_client_send( sayvims,V_CMD, msg[i] );
			}
			i++;
		}
	}
	else
	{
		/* read from stdin*/
		int not_done = 1;
		infile = fdopen( fd_in, "r" );
		if(!infile)
		{
			return 0;
		}
		while( fgets(buf, 4096, infile) )
		{
			if( buf[0] == '+' )
			{
				int wait_ = 1;
		
				if(!sscanf( buf+1, "%d", &wait_ ) )
				{
					return 0;
				}
				vj_flush( wait_ );
			}
			else
			{
				vj_client_send( sayvims, V_CMD, buf );
			}
		}
	}
	veejay_msg(VEEJAY_MSG_INFO, "closing ...");	
	vj_client_close(sayvims);
	vj_client_free(sayvims);
        return 0;
} 
