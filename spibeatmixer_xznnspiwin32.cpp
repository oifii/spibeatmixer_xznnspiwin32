/*
 * Copyright (c) 2010-2016 Stephane Poirier
 *
 * stephane.poirier@oifii.org
 *
 * Stephane Poirier
 * 3532 rue Ste-Famille, #3
 * Montreal, QC, H2X 2L1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "spibeatmixer_xznnspiwin32.h"
#include "FreeImage.h"
#include <shellapi.h> //for CommandLineToArgW()
#include <mmsystem.h> //for timeSetEvent()
#include <stdio.h> //for swprintf()
#include <assert.h>
#include "spiwavsetlib.h"

// Global Variables:

#define MAX_LOADSTRING 100
FIBITMAP* global_dib;
HFONT global_hFont;
HWND global_hwnd=NULL;
MMRESULT global_timer=0;
#define MAX_GLOBALTEXT	4096
WCHAR global_text[MAX_GLOBALTEXT+1];
int global_x=100;
int global_y=200;
int global_xwidth=400;
int global_yheight=400;
BYTE global_alpha=200;
int global_fontheight=24;
int global_fontwidth=-1; //will be computed within WM_PAINT handler
BYTE global_fontcolor_r=255;
BYTE global_fontcolor_g=255;
BYTE global_fontcolor_b=255;
int global_staticalignment = 0; //0 for left, 1 for center and 2 for right
int global_staticheight=-1; //will be computed within WM_SIZE handler
int global_staticwidth=-1; //will be computed within WM_SIZE handler 
//spi, begin
int global_imageheight=-1; //will be computed within WM_SIZE handler
int global_imagewidth=-1; //will be computed within WM_SIZE handler 
//spi, end
int global_titlebardisplay=1; //0 for off, 1 for on
int global_acceleratoractive=1; //0 for off, 1 for on
int global_menubardisplay=0; //0 for off, 1 for on
FILE* global_pfile=NULL;
#define IDC_MAIN_EDIT	100
#define IDC_MAIN_STATIC	101

HINSTANCE hInst;								// current instance
//TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
//TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
TCHAR szTitle[1024]={L"spibeatmixer_xznnspiwin32title"};					// The title bar text
TCHAR szWindowClass[1024]={L"spibeatmixer_xznnspiwin32class"};			// the main window class name

//new parameters
string global_begin="begin.ahk";
string global_end="end.ahk";

//#define StatusAddText StatusAddTextW

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

// Convert a wide Unicode string to an UTF8 string
std::string utf8_encode(const std::wstring &wstr)
{
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo( size_needed, 0 );
    WideCharToMultiByte                  (CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Convert an UTF8 string to a wide Unicode String
std::wstring utf8_decode(const std::string &str)
{
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar                  (CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Select sample format
#if 1
#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"
#elif 1
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#elif 0
#define PA_SAMPLE_TYPE  paInt8
typedef char SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#else
#define PA_SAMPLE_TYPE  paUInt8
typedef unsigned char SAMPLE;
#define SAMPLE_SILENCE  (128)
#define PRINTF_S_FORMAT "%d"
#endif

InstrumentSet* pInstrumentSet=NULL;
InstrumentSet* pInstrumentSet2=NULL;

void CALLBACK StartGlobalProcess(UINT uTimerID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	//WavSetLib_Initialize(global_hwnd, IDC_MAIN_STATIC, global_staticwidth, global_staticheight, global_fontwidth, global_fontheight);
	global_pfile = fopen("output.txt","w");
	WavSetLib_Initialize(global_hwnd, IDC_MAIN_STATIC, global_staticwidth, global_staticheight, global_fontwidth, global_fontheight, global_staticalignment, global_pfile);

	CHAR pCHAR[1024];
	WCHAR pWCHAR[1024];

	PaStreamParameters outputParameters;
    PaStream* stream;
    PaError err;

	////////////////////////
	// initialize port audio 
	////////////////////////
    err = Pa_Initialize();
    if( err != paNoError )
	{
		swprintf(pWCHAR, L"Error: Initialization failed.\n");StatusAddText(pWCHAR);
		Pa_Terminate();
		swprintf(pWCHAR, L"An error occured while using the portaudio stream\n" );StatusAddText(pWCHAR);
		swprintf(pWCHAR, L"Error number: %d\n", err );StatusAddText(pWCHAR);
		swprintf(pWCHAR, L"Error message: %s\n", Pa_GetErrorText( err ) );StatusAddText(pWCHAR);
		//return -1;
		return;
	}
	swprintf(pWCHAR, L"PortAudio initilized OK.\n");StatusAddText(pWCHAR);

	outputParameters.device = Pa_GetDefaultOutputDevice(); // default output device 
	if (outputParameters.device == paNoDevice) 
	{
		swprintf(pWCHAR, L"Error: No default output device.\n");StatusAddText(pWCHAR);
		Pa_Terminate();
		swprintf(pWCHAR, L"An error occured while using the portaudio stream\n" );StatusAddText(pWCHAR);
		swprintf(pWCHAR, L"Error number: %d\n", err );StatusAddText(pWCHAR);
		swprintf(pWCHAR, L"Error message: %s\n", Pa_GetErrorText( err ) );StatusAddText(pWCHAR);
		//return -1;
		return;
	}
	outputParameters.channelCount = 2;//pWavSet->numChannels;
	outputParameters.sampleFormat =  PA_SAMPLE_TYPE;
	outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = NULL;


	//////////////////////////
	//initialize random number
	//////////////////////////
	srand((unsigned)time(0));


	/*
	//////////////////////////////
	//play 60sec sinusoidal sample
	//////////////////////////////
	WavSet* pTempWavSet = new WavSet;
	pTempWavSet->CreateSin(60, 44100, 2, 440.0, 0.5f);
	pTempWavSet->Play(&outputParameters);
	if(pTempWavSet) { delete pTempWavSet; pTempWavSet = NULL; }
	*/
	
	
	/////////////////////////////
	//loop n sinusoidal samples
	/////////////////////////////
	WavSet* pTempWavSet = new WavSet;
	pTempWavSet->CreateSin(0.3333, 44100, 2, 440.0, 0.5f);
	WavSet* pSilentWavSet = new WavSet;
	pSilentWavSet->CreateSilence(5);
	pSilentWavSet->LoopSample(pTempWavSet, 5, -1.0, 0.0); //from second 0, loop sample during 5 seconds
	pSilentWavSet->Play(&outputParameters);
	if(pTempWavSet) { delete pTempWavSet; pTempWavSet = NULL; }
	if(pSilentWavSet) { delete pSilentWavSet; pSilentWavSet = NULL; }
	

	

	//////////////////////////////////////////////////////////////////////////
	//populate InstrumentSet according to input folder (folder of sound files)
	//////////////////////////////////////////////////////////////////////////
	//string wavfolder = "D:\\oifii-org\\httpdocs\\ha-org\\had\\dj-oifii\\raveaudio_wav\\dj-oifii_minimal-deep-electro-house-techno";
	string wavfolder = "D:\\oifii-org\\httpdocs\\ha-org\\had\\dj-oifii\\xznnspi\\samples_fbm";
	//string wavfolder = "D:\\oifii-org\\httpdocs\\ha-org\\had\\dj-oifii\\raveaudio_wav\\dj-oifii_ibiza";

	//WavSet* pFBM3Bars = new WavSet;
	//pFBM3Bars->ReadWavFile("D:\\oifii-org\\httpdocs\\ha-org\\had\\dj-oifii\\xznnspi\\samples_fbm\\fbm_3bars.wav");
	WavSet* pFBM4Bars = new WavSet;
	pFBM4Bars->ReadWavFile("D:\\oifii-org\\httpdocs\\ha-org\\had\\dj-oifii\\xznnspi\\samples_fbm\\fbm_4bars.wav"); //done
	WavSet* pFBM3ClavesBars = new WavSet;
	pFBM3ClavesBars->ReadWavFile("D:\\oifii-org\\httpdocs\\ha-org\\had\\dj-oifii\\xznnspi\\samples_fbm\\fbm-claves_3bars.wav");
	WavSet* pFBMHH2Bars = new WavSet;
	pFBMHH2Bars->ReadWavFile("D:\\oifii-org\\httpdocs\\ha-org\\had\\dj-oifii\\xznnspi\\samples_fbm\\fbm-hh_2bars.wav"); //done
	WavSet* pFBMLow1Beat = new WavSet;
	pFBMLow1Beat->ReadWavFile("D:\\oifii-org\\httpdocs\\ha-org\\had\\dj-oifii\\xznnspi\\samples_fbm\\fbm-low_1beat.wav"); //done
	WavSet* pFBMLow2Bars = new WavSet;
	pFBMLow2Bars->ReadWavFile("D:\\oifii-org\\httpdocs\\ha-org\\had\\dj-oifii\\xznnspi\\samples_fbm\\fbm-low_2bars.wav"); //done
	WavSet* pFBMOff1Bar = new WavSet;
	pFBMOff1Bar->ReadWavFile("D:\\oifii-org\\httpdocs\\ha-org\\had\\dj-oifii\\xznnspi\\samples_fbm\\fbm-off_1bar.wav"); //done
	WavSet* pFBMSnare1Bar = new WavSet;
	pFBMSnare1Bar->ReadWavFile("D:\\oifii-org\\httpdocs\\ha-org\\had\\dj-oifii\\xznnspi\\samples_fbm\\fbm-snare_1bar.wav"); //done

	WavSet* pPlayaWide = new WavSet;
	pPlayaWide->ReadWavFile("D:\\oifii-org\\httpdocs\\ha-org\\had\\dj-oifii\\xznnspi\\samples_playa\\playa_wide(distortionless).wav"); //done

	WavSet* pBeanBrotherChat = new WavSet;
	pBeanBrotherChat->ReadWavFile("D:\\oifii-org\\httpdocs\\ha-org\\had\\dj-oifii\\xznnspi\\samples_speech\\beanbrotherchat.wav"); //done

	/*
	InstrumentSet* pInstrumentSet=new InstrumentSet;
	InstrumentSet* pInstrumentSet2=new InstrumentSet;
	*/
	pInstrumentSet=new InstrumentSet;
	//pInstrumentSet2=new InstrumentSet;
	//InstrumentSet* pInstrumentSet3=new InstrumentSet;
	//if(pInstrumentSet!=NULL && pInstrumentSet2!=NULL && pInstrumentSet3!=NULL)
	//if(pInstrumentSet!=NULL && pInstrumentSet2!=NULL)
	if(pInstrumentSet!=NULL)
	{
		pInstrumentSet->Populate(wavfolder.c_str());
		//pInstrumentSet2->Populate(wavfolder2.c_str());
		//pInstrumentSet3->Populate(wavfolder3.c_str());
	}
	else
	{
		assert(false);
		swprintf(pWCHAR, L"exiting, instrumentset could not be allocated\n");StatusAddText(pWCHAR);
		Pa_Terminate();
		//return -1;
		return;
	}
		

	/*
	///////////////////////////////////
	//play all notes of all instruments
	///////////////////////////////////
	//int numberofinstrumentsinplayback=3;
	int numberofinstrumentsinplayback=1; //one instrument at a time
	float fSecondsPlay = 600.0f;
	int iCONCATENATEATTACKSflag = 0; //0 false, 1 true
	pInstrumentSet->Play(&outputParameters, fSecondsPlay, numberofinstrumentsinplayback, iCONCATENATEATTACKSflag); //each instrument will play its loaded samples sequentially
	
	int dummy=0;
	*/


	//////////////////////////////////////////////////////////////////////////////////////////////
	//pick random instrument, pick random wavset, loop wavset for loopduration_s seconds and play,
	//then repeat all of the above 100 times
	//////////////////////////////////////////////////////////////////////////////////////////////
	if(0)
	{
		Instrument* pAnInstrument = NULL;
		WavSet* pAWavSet = NULL;
		for(int i=0; i<100; i++) //repeat 100 times
		{
			if(pInstrumentSet && pInstrumentSet->HasOneInstrument()) 
			{
				//vector<Instrument*>::iterator it;
				//for(it=pInstrumentSet->instrumentvector.begin();it<pInstrumentSet->instrumentvector.end();it++)
				//{
				
					cout << endl;
					//pAnInstrument = *it;
					pAnInstrument = pInstrumentSet->GetInstrumentRandomly();
					assert(pAnInstrument);
					sprintf(pCHAR, "instrument name: %s\n", pAnInstrument->GetInstrumentName());StatusAddTextA(pCHAR);
					pAWavSet = pAnInstrument->GetWavSetRandomly();
					sprintf(pCHAR, "sound filename: %s\n", pAWavSet->GetName());StatusAddTextA(pCHAR);
					assert(pAWavSet);
					//pAWavSet->Play(&outputParameters);

					/*
					float loopduration_s = 2 * pAWavSet->GetWavSetLength() + 0.050f; //0.050 sec to ensure loopduration_s is larger than sample
					//WavSet* pSilentWavSet = new WavSet;
					pSilentWavSet = new WavSet;
					pSilentWavSet->CreateSilence(loopduration_s); 
					pSilentWavSet->LoopSample(pAWavSet, loopduration_s, -1.0, 0.0); //from second 0, loop sample during loopduration_s seconds
					//pSilentWavSet->Play(&outputParameters);
					*/
					WavSet* pSilentWavSet = new WavSet;
					while(pSilentWavSet->GetWavSetLength()<10.0)
					{
						if(pSilentWavSet->GetWavSetLength()==0.0)
						{
							//copy
							pSilentWavSet->Copy(pAWavSet);
						}
						else
						{
							//concatenate
							pSilentWavSet->Concatenate(pAWavSet);
						}
					}
				
					////////////
					//play
					////////////
					pSilentWavSet->Play(&outputParameters);
				
					//pSilentWavSet->Erase();
					//pSilentWavSet->Play(&outputParameters);
					if(pSilentWavSet)
					{
						delete pSilentWavSet;
						pSilentWavSet = NULL;
					}
				//}
			}
		}
	}


	//////////////////////////////////////////////////////////////////////////////////////////////
	//pick random instrument, pick random wavset, loop wavset for loopduration_s seconds and play,
	//then repeat all of the above 100 times
	//////////////////////////////////////////////////////////////////////////////////////////////
	if(1)
	{
		WavSet* pFinalSilentWavSet1 = new WavSet;
		pFinalSilentWavSet1->CreateSilence(0.001f); //1ms initial length so we can concatenate later

		swprintf(pWCHAR, L"\n");StatusAddText(pWCHAR);
		swprintf(pWCHAR, L"\n");StatusAddText(pWCHAR);
		swprintf(pWCHAR, L"*****************************************************\n");StatusAddText(pWCHAR);
		swprintf(pWCHAR, L"creating track 1, with a random alternance of samples\n");StatusAddText(pWCHAR);
		swprintf(pWCHAR, L"*****************************************************\n");StatusAddText(pWCHAR);
		swprintf(pWCHAR, L"\n");StatusAddText(pWCHAR);

		float barduration_s = pFBMOff1Bar->GetWavSetLength();
		Instrument* pAnInstrument = NULL;
		WavSet* pAWavSet = NULL;
		for(int i=0; i<100; i++) //repeat 100 times
		{
			if(pInstrumentSet && pInstrumentSet->HasOneInstrument()) 
			{
				//vector<Instrument*>::iterator it;
				//for(it=pInstrumentSet->instrumentvector.begin();it<pInstrumentSet->instrumentvector.end();it++)
				//{
				
					cout << endl;
					//pAnInstrument = *it;
					pAnInstrument = pInstrumentSet->GetInstrumentRandomly();
					assert(pAnInstrument);
					sprintf(pCHAR, "instrument name: %s\n", pAnInstrument->GetInstrumentName());StatusAddTextA(pCHAR);
					pAWavSet = pAnInstrument->GetWavSetRandomly();
					sprintf(pCHAR, "sound filename: %s\n", pAWavSet->GetName());StatusAddTextA(pCHAR);
					assert(pAWavSet);
					//pAWavSet->Play(&outputParameters);

					/*
					float loopduration_s = 2 * pAWavSet->GetWavSetLength() + 0.050f; //0.050 sec to ensure loopduration_s is larger than sample
					//WavSet* pSilentWavSet = new WavSet;
					pSilentWavSet = new WavSet;
					pSilentWavSet->CreateSilence(loopduration_s); 
					pSilentWavSet->LoopSample(pAWavSet, loopduration_s, -1.0, 0.0); //from second 0, loop sample during loopduration_s seconds
					//pSilentWavSet->Play(&outputParameters);
					*/
					int random_integer;
					int lowest=1, highest=10;
					int range=(highest-lowest)+1;
					random_integer = lowest+int(range*rand()/(RAND_MAX + 1.0));
					float random_float = (float) random_integer;
					//float loopduration_s = 10.0f;
					//float loopduration_s = random_float*barduration_s;
					float loopduration_s = random_float;
					swprintf(pWCHAR, L"loop duration in sec: %f\n", loopduration_s);StatusAddText(pWCHAR);
					WavSet* pSilentWavSet = new WavSet;
					while(pSilentWavSet->GetWavSetLength()<loopduration_s)
					{
						if(pSilentWavSet->GetWavSetLength()==0.0)
						{
							//copy
							pSilentWavSet->Copy(pAWavSet);
						}
						else
						{
							//concatenate
							pSilentWavSet->Concatenate(pAWavSet);
						}
					}
				
					//////
					//play
					//////
					//pSilentWavSet->Play(&outputParameters);

					//////////////////////////////
					//concatenate into final ouput
					//////////////////////////////
					pFinalSilentWavSet1->Concatenate(pSilentWavSet);

					//pSilentWavSet->Erase();
					//pSilentWavSet->Play(&outputParameters);
					if(pSilentWavSet) { delete pSilentWavSet; pSilentWavSet = NULL; }
				//}
			}
		}

		pFinalSilentWavSet1->WriteWavFile("testoutput-random.wav");
		//pFinalSilentWavSet1->Play(&outputParameters);

		float trackduration_s = pFinalSilentWavSet1->GetWavSetLength();
		swprintf(pWCHAR, L"\ntrack 1 duration (in sec): %f\n", trackduration_s);StatusAddText(pWCHAR);

		WavSet* pFinalSilentWavSet12 = new WavSet;
		if(0)
		{
			//float barduration_s = pFBMOff1Bar->GetWavSetLength();
			swprintf(pWCHAR, L"\n");StatusAddText(pWCHAR);
			swprintf(pWCHAR, L"\n");StatusAddText(pWCHAR);
			swprintf(pWCHAR, L"*********************************************************");StatusAddText(pWCHAR);
			swprintf(pWCHAR, L"creating track 2, with an alternance of silence and playa");StatusAddText(pWCHAR); //playa is the name of the ambient sample
			swprintf(pWCHAR, L"*********************************************************");StatusAddText(pWCHAR);
			swprintf(pWCHAR, L"\n");StatusAddText(pWCHAR);
			WavSet* pFinalSilentWavSet2 = new WavSet;
			pFinalSilentWavSet2->CreateSilence(0.001f); //1ms initial length so we can concatenate later
			bool playaflag=0;
			while(pFinalSilentWavSet2->GetWavSetLength()<trackduration_s)
			{
				int random_integer;
				int lowest=3, highest=10;
				int range=(highest-lowest)+1;
				random_integer = lowest+int(range*rand()/(RAND_MAX + 1.0));
				float random_float = (float)random_integer;

				float segmentduration_s = random_float*barduration_s;

				if(playaflag==0) playaflag=1;
				  else playaflag=0;

				WavSet* pSilentWavSet2 = new WavSet;
				if(playaflag==0)
				{
					pSilentWavSet2->CreateSilence(segmentduration_s);
					swprintf(pWCHAR, L"silence duration in sec: %f\n", segmentduration_s);StatusAddText(pWCHAR);
				}
				else
				{
					pSilentWavSet2->Copy(pPlayaWide, segmentduration_s, 0.0f);
					swprintf(pWCHAR, L"playa duration in sec: %f\n", segmentduration_s);StatusAddText(pWCHAR);
				}
				//concatenate
				pFinalSilentWavSet2->Concatenate(pSilentWavSet2);
				if(pSilentWavSet2) { delete pSilentWavSet2; pSilentWavSet2 = NULL; }
			}

			swprintf(pWCHAR, L"\ntrack 2 duration (in sec): %f\n", pFinalSilentWavSet2->GetWavSetLength());StatusAddText(pWCHAR);

			pFinalSilentWavSet12->Mix(0.8, pFinalSilentWavSet1, 0.5, pFinalSilentWavSet2);
			if(pFinalSilentWavSet1) { delete pFinalSilentWavSet1; pFinalSilentWavSet1=NULL; }
			if(pFinalSilentWavSet2) { delete pFinalSilentWavSet2; pFinalSilentWavSet2=NULL; }
			//pFinalSilentWavSet12->WriteWavFile("testoutput-random.wav");
			//pFinalSilentWavSet->Play(&outputParameters);
		}

		if(0)
		{
			swprintf(pWCHAR, L"\n");StatusAddText(pWCHAR);
			swprintf(pWCHAR, L"\n");StatusAddText(pWCHAR);
			swprintf(pWCHAR, L"**********************************************************\n");StatusAddText(pWCHAR);
			swprintf(pWCHAR, L"creating track 3, with an alternance of silence and speech\n");StatusAddText(pWCHAR); //speech from bean brother chat
			swprintf(pWCHAR, L"**********************************************************\n");StatusAddText(pWCHAR);
			swprintf(pWCHAR, L"\n");StatusAddText(pWCHAR);
			WavSet* pFinalSilentWavSet3 = new WavSet;
			pFinalSilentWavSet3->CreateSilence(0.001f); //1ms initial length so we can concatenate later
			bool speechflag=0;
			while(pFinalSilentWavSet3->GetWavSetLength()<trackduration_s)
			{
				int random_integer;
				int lowest=10, highest=30;
				int range=(highest-lowest)+1;
				random_integer = lowest+int(range*rand()/(RAND_MAX + 1.0));
				float random_float = (float)random_integer;

				float segmentduration_s = random_float*barduration_s;

				if(speechflag==0) speechflag=1;
				  else speechflag=0;

				WavSet* pSilentWavSet3 = new WavSet;
				if(speechflag==0)
				{
					pSilentWavSet3->CreateSilence(segmentduration_s);
					swprintf(pWCHAR, L"silence duration in sec: %f\n", segmentduration_s);StatusAddText(pWCHAR);
				}
				else
				{
					pSilentWavSet3->Copy(pBeanBrotherChat, segmentduration_s, 0.0f);
					swprintf(pWCHAR, L"speech duration in sec: %f\n", segmentduration_s);StatusAddText(pWCHAR);
				}
				//concatenate
				pFinalSilentWavSet3->Concatenate(pSilentWavSet3);
				if(pSilentWavSet3) { delete pSilentWavSet3; pSilentWavSet3 = NULL; }
			}

			swprintf(pWCHAR, L"\ntrack 3 duration (in sec): %f\n", pFinalSilentWavSet3->GetWavSetLength());StatusAddText(pWCHAR);

			WavSet* pFinalSilentWavSet123 = new WavSet;
			pFinalSilentWavSet123->Mix(0.8, pFinalSilentWavSet12, 0.5, pFinalSilentWavSet3);
			if(pFinalSilentWavSet12) { delete pFinalSilentWavSet12; pFinalSilentWavSet12=NULL; }
			if(pFinalSilentWavSet3) { delete pFinalSilentWavSet3; pFinalSilentWavSet3=NULL; }
			pFinalSilentWavSet123->WriteWavFile("testoutput-random.wav");
			//pFinalSilentWavSet->Play(&outputParameters);

			if(pFinalSilentWavSet123) { delete pFinalSilentWavSet123; pFinalSilentWavSet123=NULL; }
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//for each wavset, loop wavset for loopduration_s seconds (i.e. 16 bars) into a segment called pSilentWavSetX
	//then concatenate these pSilentWavSetX and save output wav to file
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if(0)
	{
		float extralength_s = 0.001f; //1 ms extra
		//////////////////
		//loop a few times
		//////////////////
		float loopduration_s = 4 * pFBM4Bars->GetWavSetLength() + extralength_s; //extralength_s sec to ensure loopduration_s is larger than sample
		WavSet* pSilentWavSet = new WavSet;
		pSilentWavSet->CreateSilence(loopduration_s); 
		pSilentWavSet->LoopSample(pFBM4Bars, loopduration_s, -1.0, 0.0); //from second 0, loop sample during loopduration_s seconds
		//pSilentWavSet->Play(&outputParameters);
		//////////////////
		//loop a few times
		//////////////////
		loopduration_s = 8 * pFBMHH2Bars->GetWavSetLength() + extralength_s; //extralength_s sec to ensure loopduration_s is larger than sample
		WavSet* pSilentWavSet1 = new WavSet;
		pSilentWavSet1->CreateSilence(loopduration_s); 
		pSilentWavSet1->LoopSample(pFBMHH2Bars, loopduration_s, -1.0, 0.0); //from second 0, loop sample during loopduration_s seconds
		//pSilentWavSet1->Play(&outputParameters);
		//////////////////
		//loop a few times
		//////////////////
		loopduration_s = 16 * pFBMSnare1Bar->GetWavSetLength() + extralength_s; //extralength_s sec to ensure loopduration_s is larger than sample
		WavSet* pSilentWavSet2 = new WavSet;
		pSilentWavSet2->CreateSilence(loopduration_s); 
		pSilentWavSet2->LoopSample(pFBMSnare1Bar, loopduration_s, -1.0, 0.0); //from second 0, loop sample during loopduration_s seconds
		//pSilentWavSet2->Play(&outputParameters);
		//////////////////
		//loop a few times
		//////////////////
		loopduration_s = 8 * pFBMLow2Bars->GetWavSetLength() + extralength_s; //extralength_s sec to ensure loopduration_s is larger than sample
		WavSet* pSilentWavSet3 = new WavSet;
		pSilentWavSet3->CreateSilence(loopduration_s); 
		pSilentWavSet3->LoopSample(pFBMLow2Bars, loopduration_s, -1.0, 0.0); //from second 0, loop sample during loopduration_s seconds
		//pSilentWavSet3->Play(&outputParameters);
		//////////////////
		//loop a few times
		//////////////////
		loopduration_s = 16 * pFBMOff1Bar->GetWavSetLength() + extralength_s; //extralength_s sec to ensure loopduration_s is larger than sample
		WavSet* pSilentWavSet4 = new WavSet;
		pSilentWavSet4->CreateSilence(loopduration_s); 
		pSilentWavSet4->LoopSample(pFBMOff1Bar, loopduration_s, -1.0, 0.0); //from second 0, loop sample during loopduration_s seconds
		//pSilentWavSet4->Play(&outputParameters);
		//////////////////
		//loop a few times
		//////////////////
		loopduration_s = 16 * 4 * pFBMLow1Beat->GetWavSetLength() + extralength_s; //extralength_s sec to ensure loopduration_s is larger than sample
		WavSet* pSilentWavSet5 = new WavSet;
		pSilentWavSet5->CreateSilence(loopduration_s); 
		pSilentWavSet5->LoopSample(pFBMLow1Beat, loopduration_s, -1.0, 0.0); //from second 0, loop sample during loopduration_s seconds
		//pSilentWavSet5->Play(&outputParameters);


		///////////////////////////////////////////////////////
		//concatenate all beat segments, to make the beat track
		///////////////////////////////////////////////////////
		pSilentWavSet->Concatenate(pSilentWavSet1);
		pSilentWavSet->Concatenate(pSilentWavSet2);
		pSilentWavSet->Concatenate(pSilentWavSet3);
		pSilentWavSet->Concatenate(pSilentWavSet4);
		pSilentWavSet->Concatenate(pSilentWavSet5);
		//pSilentWavSet->Play(&outputParameters);
		pSilentWavSet->WriteWavFile("testoutput.wav");

		if(pSilentWavSet) { delete pSilentWavSet; pSilentWavSet=NULL; }
		if(pSilentWavSet1) { delete pSilentWavSet1; pSilentWavSet1=NULL; }
		if(pSilentWavSet2) { delete pSilentWavSet2; pSilentWavSet2=NULL; }
		if(pSilentWavSet3) { delete pSilentWavSet3; pSilentWavSet3=NULL; }
		if(pSilentWavSet4) { delete pSilentWavSet4; pSilentWavSet4=NULL; }
		if(pSilentWavSet5) { delete pSilentWavSet5; pSilentWavSet5=NULL; }
	}


	//if(pFBM3Bars) delete pFBM3Bars;
	if(pFBM4Bars) { delete pFBM4Bars; pFBM4Bars=NULL; }
	if(pFBM3ClavesBars) { delete pFBM3ClavesBars; pFBM3ClavesBars=NULL; }
	if(pFBMHH2Bars) { delete pFBMHH2Bars; pFBMHH2Bars=NULL; }
	if(pFBMLow1Beat) { delete pFBMLow1Beat; pFBMLow1Beat=NULL; }
	if(pFBMLow2Bars) { delete pFBMLow2Bars; pFBMLow2Bars=NULL; }
	if(pFBMOff1Bar) { delete pFBMOff1Bar; pFBMOff1Bar=NULL; }
	if(pFBMSnare1Bar) { delete pFBMSnare1Bar; pFBMSnare1Bar=NULL; }

	if(pPlayaWide) { delete pPlayaWide; pPlayaWide=NULL; }
	if(pBeanBrotherChat) { delete pBeanBrotherChat; pBeanBrotherChat=NULL; }


	PostMessage(global_hwnd, WM_DESTROY, 0, 0);
}




PCHAR*
    CommandLineToArgvA(
        PCHAR CmdLine,
        int* _argc
        )
    {
        PCHAR* argv;
        PCHAR  _argv;
        ULONG   len;
        ULONG   argc;
        CHAR   a;
        ULONG   i, j;

        BOOLEAN  in_QM;
        BOOLEAN  in_TEXT;
        BOOLEAN  in_SPACE;

        len = strlen(CmdLine);
        i = ((len+2)/2)*sizeof(PVOID) + sizeof(PVOID);

        argv = (PCHAR*)GlobalAlloc(GMEM_FIXED,
            i + (len+2)*sizeof(CHAR));

        _argv = (PCHAR)(((PUCHAR)argv)+i);

        argc = 0;
        argv[argc] = _argv;
        in_QM = FALSE;
        in_TEXT = FALSE;
        in_SPACE = TRUE;
        i = 0;
        j = 0;

        while( a = CmdLine[i] ) {
            if(in_QM) {
                if(a == '\"') {
                    in_QM = FALSE;
                } else {
                    _argv[j] = a;
                    j++;
                }
            } else {
                switch(a) {
                case '\"':
                    in_QM = TRUE;
                    in_TEXT = TRUE;
                    if(in_SPACE) {
                        argv[argc] = _argv+j;
                        argc++;
                    }
                    in_SPACE = FALSE;
                    break;
                case ' ':
                case '\t':
                case '\n':
                case '\r':
                    if(in_TEXT) {
                        _argv[j] = '\0';
                        j++;
                    }
                    in_TEXT = FALSE;
                    in_SPACE = TRUE;
                    break;
                default:
                    in_TEXT = TRUE;
                    if(in_SPACE) {
                        argv[argc] = _argv+j;
                        argc++;
                    }
                    _argv[j] = a;
                    j++;
                    in_SPACE = FALSE;
                    break;
                }
            }
            i++;
        }
        _argv[j] = '\0';
        argv[argc] = NULL;

        (*_argc) = argc;
        return argv;
    }

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	//LPWSTR *szArgList;
	LPSTR *szArgList;
	int nArgs;
	int i;
	//szArgList = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	szArgList = CommandLineToArgvA(GetCommandLineA(), &nArgs);
	if( NULL == szArgList )
	{
		//wprintf(L"CommandLineToArgvW failed\n");
		return FALSE;
	}
	LPWSTR *szArgListW;
	int nArgsW;
	szArgListW = CommandLineToArgvW(GetCommandLineW(), &nArgsW);
	if( NULL == szArgListW )
	{
		//wprintf(L"CommandLineToArgvW failed\n");
		return FALSE;
	}
	if(nArgs>1)
	{
		global_x = atoi(szArgList[1]);
	}
	if(nArgs>2)
	{
		global_y = atoi(szArgList[2]);
	}
	if(nArgs>3)
	{
		global_xwidth = atoi(szArgList[3]);
	}
	if(nArgs>4)
	{
		global_yheight = atoi(szArgList[4]);
	}
	if(nArgs>5)
	{
		global_alpha = atoi(szArgList[5]);
	}
	if(nArgs>6)
	{
		global_titlebardisplay = atoi(szArgList[6]);
	}
	if(nArgs>7)
	{
		global_menubardisplay = atoi(szArgList[7]);
	}
	if(nArgs>8)
	{
		global_acceleratoractive = atoi(szArgList[8]);
	}
	if(nArgs>9)
	{
		global_fontheight = atoi(szArgList[9]);
	}
	if(nArgs>10)
	{
		global_fontcolor_r = atoi(szArgList[10]);
	}
	if(nArgs>11)
	{
		global_fontcolor_g = atoi(szArgList[11]);
	}
	if(nArgs>12)
	{
		global_fontcolor_b = atoi(szArgList[12]);
	}
	if(nArgs>13)
	{
		global_staticalignment = atoi(szArgList[13]);
	}
	//new parameters
	if(nArgs>14)
	{
		wcscpy(szWindowClass, szArgListW[14]); 
	}
	if(nArgs>15)
	{
		wcscpy(szTitle, szArgListW[15]); 
	}
	if(nArgs>16)
	{
		global_begin = szArgList[16]; 
	}
	if(nArgs>17)
	{
		global_end = szArgList[17]; 
	}

	LocalFree(szArgList);
	LocalFree(szArgListW);

	ShellExecuteA(NULL, "open", global_begin.c_str(), "", NULL, nCmdShow);

	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	//LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	//LoadString(hInstance, IDC_SPIWAVWIN32, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	if(global_acceleratoractive)
	{
		hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SPIWAVWIN32));
	}
	else
	{
		hAccelTable = NULL;
	}
	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	//wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SPIWAVWIN32));
	wcex.hIcon			= (HICON)LoadImage(NULL, L"background_32x32x16.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);

	if(global_menubardisplay)
	{
		wcex.lpszMenuName = MAKEINTRESOURCE(IDC_SPIWAVWIN32); //original with menu
	}
	else
	{
		wcex.lpszMenuName = NULL; //no menu
	}
	wcex.lpszClassName	= szWindowClass;
	//wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
	wcex.hIconSm		= (HICON)LoadImage(NULL, L"background_16x16x16.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;

	hInst = hInstance; // Store instance handle in our global variable

	global_dib = FreeImage_Load(FIF_JPEG, "background.jpg", JPEG_DEFAULT);

	//global_hFont=CreateFontW(32,0,0,0,FW_BOLD,0,0,0,0,0,0,2,0,L"SYSTEM_FIXED_FONT");
	global_hFont=CreateFontW(global_fontheight,0,0,0,FW_NORMAL,0,0,0,0,0,0,2,0,L"SYSTEM_FIXED_FONT");

	if(global_titlebardisplay)
	{
		hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, //original with WS_CAPTION etc.
			global_x, global_y, global_xwidth, global_yheight, NULL, NULL, hInstance, NULL);
	}
	else
	{
		hWnd = CreateWindow(szWindowClass, szTitle, WS_POPUP | WS_VISIBLE, //no WS_CAPTION etc.
			global_x, global_y, global_xwidth, global_yheight, NULL, NULL, hInstance, NULL);
	}
	if (!hWnd)
	{
		return FALSE;
	}
	global_hwnd = hWnd;

	SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
	SetLayeredWindowAttributes(hWnd, 0, global_alpha, LWA_ALPHA);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	return TRUE;
}


//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	HGDIOBJ hOldBrush;
	HGDIOBJ hOldPen;
	int iOldMixMode;
	COLORREF crOldBkColor;
	COLORREF crOldTextColor;
	int iOldBkMode;
	HFONT hOldFont, hFont;
	TEXTMETRIC myTEXTMETRIC;

	switch (message)
	{
	case WM_CREATE:
		{
			//HWND hStatic = CreateWindowEx(WS_EX_TRANSPARENT, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,  
			HWND hStatic = CreateWindowEx(WS_EX_TRANSPARENT, L"STATIC", L"", WS_CHILD | WS_VISIBLE | global_staticalignment, 
				0, 100, 100, 100, hWnd, (HMENU)IDC_MAIN_STATIC, GetModuleHandle(NULL), NULL);
			if(hStatic == NULL)
				MessageBox(hWnd, L"Could not create static text.", L"Error", MB_OK | MB_ICONERROR);
			SendMessage(hStatic, WM_SETFONT, (WPARAM)global_hFont, MAKELPARAM(FALSE, 0));



			global_timer=timeSetEvent(2000,25,(LPTIMECALLBACK)&StartGlobalProcess,0,TIME_ONESHOT);
		}
		break;
	case WM_SIZE:
		{
			RECT rcClient;

			GetClientRect(hWnd, &rcClient);
			/*
			HWND hEdit = GetDlgItem(hWnd, IDC_MAIN_EDIT);
			SetWindowPos(hEdit, NULL, 0, 0, rcClient.right/2, rcClient.bottom/2, SWP_NOZORDER);
			*/
			HWND hStatic = GetDlgItem(hWnd, IDC_MAIN_STATIC);
			global_staticwidth = rcClient.right - 0;
			//global_staticheight = rcClient.bottom-(rcClient.bottom/2);
			global_staticheight = rcClient.bottom-0;
			//spi, begin
			global_imagewidth = rcClient.right - 0;
			global_imageheight = rcClient.bottom - 0; 
			WavSetLib_Initialize(global_hwnd, IDC_MAIN_STATIC, global_staticwidth, global_staticheight, global_fontwidth, global_fontheight, global_staticalignment, global_pfile);
			//spi, end
			//SetWindowPos(hStatic, NULL, 0, rcClient.bottom/2, global_staticwidth, global_staticheight, SWP_NOZORDER);
			SetWindowPos(hStatic, NULL, 0, 0, global_staticwidth, global_staticheight, SWP_NOZORDER);
		}
		break;
	case WM_CTLCOLOREDIT:
		{
			SetBkMode((HDC)wParam, TRANSPARENT);
			SetTextColor((HDC)wParam, RGB(0xFF, 0xFF, 0xFF));
			return (INT_PTR)::GetStockObject(NULL_PEN);
		}
		break;
	case WM_CTLCOLORSTATIC:
		{
			SetBkMode((HDC)wParam, TRANSPARENT);
			//SetTextColor((HDC)wParam, RGB(0xFF, 0xFF, 0xFF));
			SetTextColor((HDC)wParam, RGB(global_fontcolor_r, global_fontcolor_g, global_fontcolor_b));
			return (INT_PTR)::GetStockObject(NULL_PEN);
		}
		break;
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		SetStretchBltMode(hdc, COLORONCOLOR);
		//spi, begin
		/*
		StretchDIBits(hdc, 0, 0, global_xwidth, global_yheight,
						0, 0, FreeImage_GetWidth(global_dib), FreeImage_GetHeight(global_dib),
						FreeImage_GetBits(global_dib), FreeImage_GetInfo(global_dib), DIB_RGB_COLORS, SRCCOPY);
		*/
		StretchDIBits(hdc, 0, 0, global_imagewidth, global_imageheight,
						0, 0, FreeImage_GetWidth(global_dib), FreeImage_GetHeight(global_dib),
						FreeImage_GetBits(global_dib), FreeImage_GetInfo(global_dib), DIB_RGB_COLORS, SRCCOPY);
		//spi, end
		hOldBrush = SelectObject(hdc, (HBRUSH)GetStockObject(GRAY_BRUSH));
		hOldPen = SelectObject(hdc, (HPEN)GetStockObject(WHITE_PEN));
		//iOldMixMode = SetROP2(hdc, R2_MASKPEN);
		iOldMixMode = SetROP2(hdc, R2_MERGEPEN);
		//Rectangle(hdc, 100, 100, 200, 200);

		crOldBkColor = SetBkColor(hdc, RGB(0xFF, 0x00, 0x00));
		crOldTextColor = SetTextColor(hdc, RGB(0xFF, 0xFF, 0xFF));
		iOldBkMode = SetBkMode(hdc, TRANSPARENT);
		//hFont=CreateFontW(70,0,0,0,FW_BOLD,0,0,0,0,0,0,2,0,L"SYSTEM_FIXED_FONT");
		//hOldFont=(HFONT)SelectObject(hdc,global_hFont);
		hOldFont=(HFONT)SelectObject(hdc,global_hFont);
		GetTextMetrics(hdc, &myTEXTMETRIC);
		global_fontwidth = myTEXTMETRIC.tmAveCharWidth;
		//TextOutW(hdc, 100, 100, L"test string", 11);

		SelectObject(hdc, hOldBrush);
		SelectObject(hdc, hOldPen);
		SetROP2(hdc, iOldMixMode);
		SetBkColor(hdc, crOldBkColor);
		SetTextColor(hdc, crOldTextColor);
		SetBkMode(hdc, iOldBkMode);
		SelectObject(hdc,hOldFont);
		//DeleteObject(hFont);
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		//terminate portaudio
		Pa_Terminate();
		//delete all memory allocations
		if(pInstrumentSet!=NULL) delete pInstrumentSet;
		if(pInstrumentSet2!=NULL) delete pInstrumentSet2;
		//close file
		if(global_pfile) fclose(global_pfile);
		//terminate wavset library
		WavSetLib_Terminate();
		//terminate win32 app.
		if (global_timer) timeKillEvent(global_timer);
		FreeImage_Unload(global_dib);
		DeleteObject(global_hFont);
		ShellExecuteA(NULL, "open", global_end.c_str(), "", NULL, 0);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
