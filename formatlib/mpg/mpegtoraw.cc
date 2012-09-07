/* MPEG/WAVE Sound library

   (C) 1997 by Jung woo-jae */

// Mpegtoraw.cc
// Server which get mpeg format and put raw format.

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream.h>

#include "mpegsound.h"
#include "mpegsound_locals.h"

#define MY_PI 3.14159265358979323846

Mpegtoraw::Mpegtoraw(Soundinputstream *loader)
{
  	__errorcode = SOUND_ERROR_OK;
  	frameoffsets = NULL;

  	forcetomonoflag = false;
  	downfrequency = 0;

  	this->loader = loader;
}

Mpegtoraw::~Mpegtoraw()
{
  	if( frameoffsets )
		delete [] frameoffsets;
}

#ifndef WORDS_BIGENDIAN
#define _KEY 0
#else
#define _KEY 3
#endif

int Mpegtoraw::getbits(int bits)
{
  	union
  	{
    	char store[4];
    	int current;
  	}u;

  	int bi;

  	if( !bits )
		return 0;

  	u.current = 0;
  	bi = ( bitindex & 7 );
  	u.store[_KEY] = buffer[bitindex >> 3] << bi;
  	bi = 8-bi;
  	bitindex += bi;

  	while(bits)
  	{
    	if(!bi)
    	{
      	u.store[_KEY] = buffer[bitindex >> 3];
      	bitindex += 8;
      	bi = 8;
    	}

    	if( bits >= bi)
    	{
      	u.current <<= bi;
      	bits -= bi;
      	bi = 0;
    	}
    	else
    	{
      	u.current <<= bits;
      	bi -= bits;
      	bits = 0;
    	}
  	}
  	bitindex -= bi;

  	return ( u.current >> 8 );
}

void Mpegtoraw::setforcetomono(bool flag)
{
  	forcetomonoflag = flag;
}

void Mpegtoraw::setdownfrequency(int value)
{
  	downfrequency = 0;
  	if( value )
		downfrequency = 1;
}

bool Mpegtoraw::getforcetomono(void)
{
  	return forcetomonoflag;
}

int Mpegtoraw::getdownfrequency(void)
{
  	return downfrequency;
}

int  Mpegtoraw::getpcmperframe(void)
{
  	int s;

  	s = 32;
  	if(layer == 3) {
    	s *= 18;
    	if( version == 0)
			s *= 2;
  	} else {
    	s *= SCALEBLOCK;
    	if( layer == 2 )
			s *= 3;
  	}

  	return s;
}

inline void stripfilename(char *dtr,char *str,int max)
{
  	char *ss;
  	int p = 0, s = 0;

  	for( ; str[p]; p++ )
    	if(str[p] == '/')
    	{
      	p++;
      	s = p;
    	}

  	ss = str + s;
  	for( p = 0; p<max && ss[p]; p++ )
		dtr[p] = ss[p];
  	dtr[p] = 0;
}

// Convert mpeg to raw
// Mpeg headder class
void Mpegtoraw::initialize(char *)
{
  	static bool initialized = false;

  	register int i;
  	register REAL *s1,*s2;
  	REAL *s3,*s4;

  	scalefactor = SCALE;
  	calcbufferoffset = 15;
  	currentcalcbuffer = 0;

  	s1 = calcbufferL[0];
	s2 = calcbufferR[0];
  	s3 = calcbufferL[1];
	s4 = calcbufferR[1];
  	for( i = CALCBUFFERSIZE-1; i >= 0; i-- )
   calcbufferL[0][i] = calcbufferL[1][i] = 
   calcbufferR[0][i] = calcbufferR[1][i] = 0.0;

  	if(!initialized)
  	{
    	for( i = 0; i < 16; i++)
			hcos_64[i] = 1.0/(2.0*cos(MY_PI*double(i*2+1)/64.0));
    	for( i = 0; i < 8; i++ )
			hcos_32[i] = 1.0/(2.0*cos(MY_PI*double(i*2+1)/32.0));
    	for( i = 0; i < 4; i++ )
			hcos_16[i] = 1.0/(2.0*cos(MY_PI*double(i*2+1)/16.0));
    	for( i = 0; i < 2; i++ )
			hcos_8 [i] = 1.0/(2.0*cos(MY_PI*double(i*2+1)/ 8.0));
    	hcos_4 = 1.0/(2.0*cos(MY_PI*1.0/4.0));
    	initialized = true;
  	}

  	layer3initialize();

  	currentframe = decodeframe = 0;

  	if(loadheader(currentframe)) {

    	totalframe = ( loader->getsize() + framesize-1 ) / framesize;
    	loader->setposition(0);

  	} else totalframe = 0;


  	if( frameoffsets )
		delete [] frameoffsets;

  	if( totalframe > 0 ) {

    	frameoffsets = new int[totalframe];
    	for( i = totalframe-1; i >= 0; i-- )
      	frameoffsets[i] = 0;

  	} else frameoffsets = NULL;

}

void Mpegtoraw::setframe(int framenumber)
{
  int pos = 0;

  if(frameoffsets == NULL)
		return;

  if(framenumber == 0){
		pos = frameoffsets[0];

  } else {

    if( framenumber >= totalframe )
		framenumber = totalframe-1;

    pos = frameoffsets[framenumber];

    if(pos == 0) {

      int i;

      for( i = framenumber-1; i > 0; i-- )
			if( frameoffsets[i] !=  0 )
				break;

      loader->setposition(frameoffsets[i]);

      while( i < framenumber ) {

			loadheader(i);
			i++;

      }
		frameoffsets[i] = loader->getposition();
      pos = frameoffsets[framenumber];
    }
  }

  loader->setposition(pos);
  decodeframe = currentframe = framenumber;
}

bool Mpegtoraw::loadheader(int frame)
{
    register int c;
    bool flag;

    flag = false;
    do
    {
   		if( (c = loader->getbytedirect()) < 0 ) 
            break;

        if( c == 0xff )
        {
            while( ! flag )
            {
   		if( (c = loader->getbytedirect()) < 0 ) 
                {
                    flag = true;
                    break;
                }
                if( (c & 0xf0) == 0xf0 )
                {
                    flag = true;
                    break;
                }
                else if( c != 0xff )
                {
                    break;
                }
            }
        }
    } while( ! flag );

    if( c < 0 )
        return false;

    if(frameoffsets)
    {
        if (frame >= totalframe)
        {
           return false;
        }
        frameoffsets[frame] = loader->getposition() - 2;
    }

    // Analyzing

    c &= 0xf;
    protection = c & 1;
    layer = 4 - ((c >> 1) & 3);
    version = (_mpegversion) ((c >> 3) ^ 1);

  	c = loader->getbytedirect() >> 1;  
    padding = (c & 1);
    c >>= 1;
    frequency = (_frequency) (c&3);
    if (frequency == 3){
		cerr << "Bad frequency" << endl;
        return false;
	}
    c >>= 2;
    bitrateindex = (int) c;
    if( bitrateindex == 15 ){
		cerr << "Bad bitrate" << endl;
        return false;
	}

    c = ((unsigned int)loader->getbytedirect()) >> 4;
    extendedmode = c & 3;
    mode = (_mode) (c >> 2);


    // Making information

    inputstereo = (mode == single) ? 0 : 1;
	outputstereo = forcetomonoflag ? 0 : inputstereo ; 


    channelbitrate=bitrateindex;
    if(inputstereo)
    {
        if(channelbitrate==4)
            channelbitrate=1;
        else
            channelbitrate-=4;
    }

    if(channelbitrate==1 || channelbitrate==2)
        tableindex=0;
    else
        tableindex=1;

  if(layer==1)
      subbandnumber=MAXSUBBAND;
  else
  {
    if(!tableindex)
      if(frequency==frequency32000)subbandnumber=12; else subbandnumber=8;
    else if(frequency==frequency48000||
        (channelbitrate>=3 && channelbitrate<=5))
      subbandnumber=27;
    else subbandnumber=30;
  }

  if(mode==single)stereobound=0;
  else if(mode==joint)stereobound=(extendedmode+1)<<2;
  else stereobound=subbandnumber;

  if(stereobound>subbandnumber)stereobound=subbandnumber;

  // framesize & slots
  if(layer==1)
  {
    framesize=(12000*bitrate[version][0][bitrateindex])/
              frequencies[version][frequency];
    if(frequency==frequency44100 && padding)framesize++;
    framesize<<=2;
  }
  else
  {
    framesize=(144000*bitrate[version][layer-1][bitrateindex])/
      (frequencies[version][frequency]<<version);
    if(padding)framesize++;
    if(layer==3)
    {
      if(version)
    layer3slots=framesize-((mode==single)?9:17)
                         -(protection?0:2)
                         -4;
      else
    layer3slots=framesize-((mode==single)?17:32)
                         -(protection?0:2)
                         -4;
    }
  }
if (getenv("AFLIB_DEBUG"))
{
	if(frameoffsets)
  fprintf(stderr, "MPEG %d audio layer %d (%d kbps), at %d Hz %s [%d] frame %d pos %d\n", version+1, layer,  bitrate[version][layer-1][bitrateindex], frequencies[version][frequency], (mode == single) ? "mono" : "stereo", framesize,frame,frameoffsets[frame]);
}

  /* Fill the buffer with new data */
  if(!fillbuffer(framesize-4)){
if (getenv("AFLIB_DEBUG"))
{
	cerr << "couldn't fill buffer" << endl;
}
    return false;
	}
	rawdataoffset = 0;

  if(!protection)
  {
    getbyte();                      // CRC, Not check!!
    getbyte();
  }
  return true;

/*
  register int c;
  bool flag;

  sync();

  flag = false;
  do {

    if( (c = loader->getbytedirect()) < 0 ) 
		return seterrorcode(SOUND_ERROR_FINISH);

    if( c == 0xff ){ 

      while( !flag ) {

			if( (c = loader->getbytedirect()) < 0 )
				return seterrorcode(SOUND_ERROR_FINISH);

			if( c & 0xe0 ) {
	  			flag = true;
	  			break;
			} else if( c !=  0xff ) break;
      }
	}

  } while( !flag );



  	protection = c & 1;

	c >>= 1;
	if( c & 3 ) layer = 1;
	else if ( c & 2 ) layer = 2;
	else if ( c & 1 ) layer = 3; 

	c >>= 2;
	if( c & 3 ) version = _mpegversion(0); 
	else if ( c & 2 ) version = _mpegversion(1);
	else if ( c & 0 ) version = _mpegversion(2);  


  	c = ( (loader->getbytedirect()) ) >> 1;
	padding = ( c & 1 );             
	c >>=  1;
  	frequency = ( _frequency )( c & 2 ); 
	c >>=  2;
  	bitrateindex = (int) c;
  	if( bitrateindex  ==  15 )
		return seterrorcode(SOUND_ERROR_BAD);

  	c = ((unsigned int)(loader->getbytedirect())) >> 4;
  	extendedmode = c & 3;
  	mode = ( _mode )( (c >> 2) & 3 );

  	inputstereo = mode == single ? 0 : 1;
	outputstereo = forcetomonoflag ? 0 : inputstereo ; 

    if(layer =  = 2)
    if((bitrateindex> = 1 && bitrateindex< = 3) || (bitrateindex =  = 5)) {
      if(inputstereo)return seterrorcode(SOUND_ERROR_BAD); }
    else if(bitrateindex =  = 11 && mode =  = single)
    return seterrorcode(SOUND_ERROR_BAD); 

  	channelbitrate = bitrateindex;

  	if(inputstereo){
    	if( channelbitrate == 4 )
			channelbitrate = 1;
    	else 
			channelbitrate -= 4;
	}

	tableindex = ( channelbitrate == 1 || channelbitrate == 2 ) ?  0 : 1; 

  	if( layer == 1 ){

		subbandnumber = MAXSUBBAND;

  	} else {

    	if( !tableindex )
			subbandnumber = frequency == frequency32000 ? 12 : 8;
    	 else if( frequency == frequency48000 ||
	    	(channelbitrate >= 3 && channelbitrate <= 5 ) )
      		subbandnumber = 27;
    	 else subbandnumber = 30;

  	}

  	if( mode == single ) 
		stereobound = 0;
  	else if( mode == joint )
		stereobound = ( extendedmode+1 ) << 2;
  	else 
		stereobound = subbandnumber;

  	if( stereobound > subbandnumber )
		stereobound = subbandnumber;

  	if( layer == 1 ) {

    	framesize = (( 12000*bitrate[version][0][bitrateindex] )/
              frequencies[version][frequency]  + padding ) * 4;

  	} else {

    	framesize = ((( 144000*bitrate[version][layer-1][bitrateindex] )/
      	 ( frequencies[version][frequency] << version )) + padding ); 

    	if(layer == 3)
    	{
      	if(version)
				layer3slots = framesize 
								-( mode == single ? 9 : 17 )
	                     -( protection ? 0 : 2 )
	                     -4;
      	else
				layer3slots = framesize
								-( mode == single ? 17 : 32 )
	                     -( protection ? 0 : 2 )
	                     -4;
    	}
  	}

	framesize -= 4;

  	if( !fillbuffer( framesize ) )
		seterrorcode(SOUND_ERROR_FILEREADFAIL);

  	if( !protection ) {
    	getbyte();                    
    	getbyte();
  	}

  	if( loader->eof() )
		return seterrorcode(SOUND_ERROR_FINISH);

  	return true;
*/
}


// Convert mpeg to raw
long Mpegtoraw::run(short int* buffer, int frames)
{
	int no_samples = 0;

	rawdata = buffer;

  	if( frames < 0 )
		lastfrequency = 0;

  	for( ; frames; frames-- )
  	{
    	if( totalframe > 0 ) {
      	if( decodeframe < totalframe )
				frameoffsets[decodeframe] = loader->getposition();
    	}

    	if(loader->eof()) {
      	seterrorcode(SOUND_ERROR_FINISH);
      	break;
    	}

    	if(loadheader(decodeframe) == false){
if (getenv("AFLIB_DEBUG"))
{
			cerr << "Failed loading header" << endl ;
}
			break;
		}

    	if( frequency != lastfrequency ) {

      	if( lastfrequency > 0 )
				seterrorcode(SOUND_ERROR_BAD);
      	lastfrequency = frequency;

   	}

   	if( frames < 0 ) 
      	frames =- frames;

   	decodeframe++;

   	if( layer == 3 )
			extractlayer3();
   	else if( layer == 2 )
			extractlayer2();
		else if( layer == 1 )
			extractlayer1();

		no_samples += rawdataoffset;	
  		currentframe++;
  	}
	return (no_samples);

}
