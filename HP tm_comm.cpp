/*********************************************************************
*  Author:    Sara Levy
*  Date:      4 March 1996
*  Modififed:
*  Revision:  TeleTool 2.0
*
*  FILE NAME: tm_comm.cpp
*
*
*  DESCRIPTION:
*	TM_COMM.CPP contains all the protocol, packet and message  
*	functions, along with some misc. communication functions.
*	
*
*	Basic control flow for outgoing data is as follows:
*		Restore or ...	- initiated by user via GUI
*						  calls SendMessage with appropriate message
*		SendMessage		- Increment CurrentSpacket
*						  If CurrenSpacket status is empty, set
*						  Packet's message pointer to this message
*						  and call CreatePacket.
*		CreatePacket	- Set Address, Length, State, CRC and 
*						  Status= AWAITING_SEND.  Packet will wait
*						  until Interrupt Handler receives an OK
*						  for TeleTool to talk. Main will contin-
*						  ously check for this and when it is 
*						  notified that TeleTool can talk, it
*						  will call SendPacket.
*		SendPacket		- Transfers contents of packet to a flat
*						  data structure, called Sbuffer.  
*						  If needed, wake up tUHFman and
*						  prime transmit register with the
*						  first byte to be sent.  Set packet
*						  status to AWAITING_ACK.  Interrupt
*						  handler will then take over and send the
*						  contents of Sbuffer.
*                                             
*
*	Basic control flow for incoming data is as follows:
*		IncomingData	- Waits for all packets to be received
*		InterpretBuffer - Confirm that packet is meant for
*						  TeleTool.  Increment CurrentRpacket and
*						  confirm it is empty.  Copy contents
*						  of incoming Rbuffer to Rbuffer2 for
*						  interpretation.  Call InterpretPacket.
*		InterpretPacket - If ACK packet, decrement Rpacket and
*						  reset both matching Spacket and Rpacket.
*						  If data, fill Rpacket data structure
*						  with data and check CRC.  If good CRC,
*						  call InterpretMessage, send ACK Message
*						  and reset packet.  If bad CRC just reset
*						  the packet.
*		InterpretMessage- Update screen and GUI data structures
*						  with new data.
*		SendMessage		- Send ACK Message back to tUHFman
*		ResetPacket		- Set packet status to EMPTY
*
**********************************************************************/

/*-SYSTEM INCLUDES---------------------------------------------------*/
#include <iostream.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <dos.h>
#include <stdlib.h> 
#include <graph.h>
#include <math.h>
#include <conio.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <io.h>

/*-MODULE INCLUDES---------------------------------------------------*/
#include "disp.h"
#include "comm.h"
#include "config.h"
#include "protocol.h"
#include "packet.h"
#include "message.h"
#include "screen.h"
#include "crc.h"

/*-EXTERNAL FUNCTIONS------------------------------------------------*/
extern void InstallInterruptHandler(void);
extern void GetFilename (char *);
extern int  CheckFilename (char *,unsigned int);
extern void disp_str (char *, int, int, int);   
extern void (_cdecl _far _interrupt * oldClockVector)(); 
extern void pos_cursor (int, int);
extern void disp_msg (char *);
extern void PrintTMconfig (char*);
extern void SaveTMconfig (char*);
extern void LoadTMsettings( char* );
extern double CharToDouble( char* );


/*-EXTERNAL VARIABLES------------------------------------------------*/
extern  void (_cdecl _far _interrupt * oldUARTvector)();
extern  void (_cdecl _far _interrupt * oldClockVector)(); 
extern int KEY_STATUS;
extern unsigned char Rbuffer[255];
extern unsigned char Sbuffer[10][100];
extern unsigned int  volatile rcvCount;
extern unsigned int sndCount[10];
extern int volatile CURRENT_PACKET;
extern int volatile PACKET_RECD;
extern unsigned char volatile TTlastpnum; 
extern unsigned char volatile TTcurrpnum;
extern unsigned char volatile TMlastpnum;
extern unsigned char volatile TMcurrpnum; 
extern Screen TMscreen1;
extern Screen TMscreen2;
extern Screen TMscreen3;
extern Screen TMscreen4;
extern Screen TMscreen5;
extern Screen TMscreen6;
extern Screen TMscreen7;  
extern Screen   *CurrentScreenPtr;
extern int volatile PACKET_STATUS[10];
int MAX_PACKETS;
extern int volatile ERROR_FLAG;
extern int INIT_IN_PROGRESS;
extern int volatile SELFTEST_STATUS;

/*-DEFINES-----------------------------------------------------------*/
#define NOACKMSG					0x00
#define ACKMSG						0x01
#define INVOKE						0
#define RESPONSE					1
#define COM_PORT					0
#define IR_PORT						1
#define NUM_COM_PACKETS				16
#define NUM_IR_PACKETS				1
#define CONN_ESTAB_ON				0x40      // bit 6
#define CONN_ESTAB_OFF				0x0
#define TIMEOUT_TICKS				6
#define	X							0
#define Y							1
#define SUCCESS						0
#define FAILURE						1
#define MAX_LINE_SIZE				512
#define MAX_SEGMENT_SIZE			255

// Message Function Numbers:
#define		GetAttrInvoke			0
#define		GetAttrResponse			1
#define		SetAttrInvoke			2
#define		SetAttrResponse			3
#define		EventInvoke				4
#define		SelfTestInvoke			5
#define		SelfTestResponse		6
#define		SetUpMeasureInvoke		7
#define		RemoveMeasureInvoke		8
#define		RebootInvoke			9
#define		UploadInvoke			10
#define		DownloadInvoke			11
#define		DownloadResponse		12
#define		DownloadSegmentInvoke	13
#define		DownloadSegmentResponse	14


/*-TYPEDEFS----------------------------------------------------------*/

/*-GLOBAL FUNCTIONS--------------------------------------------------*/
void comm_inits(void);
void _far _interrupt pcTickHandler(void);
void CreateMessages(void);
double InterpretFrequency(char, char, char);
float create_xy(unsigned char, unsigned char);
char undo_xy(unsigned char, unsigned char);
char GetByte(int, double);
int GetFreqOptionNum(double);
void ClearCopy_Rbuffer();
int QueryUser(void);
void QueryAnother(void);
void Display_RFRange(int);
void TransferSpO2(void);
void sleep( clock_t );
void GetAttr17(void);

void Create_CopyMFTT_Msg1(void);
void Create_CopyMFTT_Msg2(void);
void Create_CopyMFTT_Msg3(void);
void Create_CopyMFTT_Msg4(void);
void Create_CopyMFTT_Msg5(void);
void Create_CopyMFTT_Msg6(void);
void Create_CopyMFTT_Msg7(void);
void Create_CopyMFTT_Msg8(void);
void Create_CopyMFTT_Msg9(void);
void Create_CopyMFTT_Msg10(void);

void Create_Junk_Msg(void);

void Create_CopyTTMF_Msg1(void);
void Create_CopyTTMF_Msg2(void);
void Create_CopyTTMF_Msg3(void);
void Create_CopyTTMF_Msg4(void);
void Create_CopyTTMF_Msg5(void);

void Create_SelfTest_Msg(void);

unsigned int asciihex(char, char);
int IncomingData(void);
void InterpretBuffer(void);
void restore(void);
void CreateNumBytesTable(void); 
char GetValue(int, int, int);
void TransferPCB(void);
void SelfTest(void);

/*-GLOBAL VARIABLES--------------------------------------------------*/
int QA_STATUS;
int NEW_CONNECTION;
int NUM_PACKETS;
int PORT = IR_PORT;
int FREQ_VERIFIED = NO;
Protocol TMprotocol;
int NumBytesTable[40];
unsigned char Rbuffer2[255]; 
Screen CurrentScreen;
long gmentTotal; 
FILE *upgfile; 
long segmentTotal;
static int segmentCount=0; 


Message CopyTTMF_Msg5;   
Message CopyTTMF_Msg4;   
Message CopyTTMF_Msg3;   
Message CopyTTMF_Msg2;   
Message CopyTTMF_Msg1;   

Message CopyMFTT_Msg10;
Message CopyMFTT_Msg9;
Message CopyMFTT_Msg8;
Message CopyMFTT_Msg7;
Message CopyMFTT_Msg6;
Message CopyMFTT_Msg5;
Message CopyMFTT_Msg4;
Message CopyMFTT_Msg3;
Message CopyMFTT_Msg2;
Message CopyMFTT_Msg1;

Message SelfTest_Msg;
Message DownloadInit_Msg;
Message Download_Msg;
Message Junk_Msg;

/*-STATIC FUNCTIONS--------------------------------------------------*/

/*-STATIC VARIABLES--------------------------------------------------*/
int volatile Tcounter;   // global time counter


//\/\/\/\/\/\/\/\/\/\ PROTOCOL CLASS FUNCTIONS /\/\/\/\/\/\/\/\/\/\/\/
/*********************************************************************
*
*  Protocol Constructor 
*
**********************************************************************/
Protocol::Protocol()
{
    CurrentRpacket = 0;
    CurrentSpacket = 0; 
    SetFirstRpacketPtr();
    SetFirstSpacketPtr();
    EndMark = 0xF0;
}



//\/\/\/\/\/\/\/\/\/ PACKET CLASS FUNCTIONS /\/\/\/\/\/\/\/\/\/\/\/\/
/*********************************************************************
*  ROUTINE: Packet Constructor
*
*  DESCRIPTION:
**********************************************************************/
Packet::Packet()
{
    Address = NULL;
    Length = NULL;
    State = NULL; 
    Msg = (Message *)NULL;
    CRC = NULL;
    Status = EMPTY;
	SetPnum( 1 ); 
}
 

/*********************************************************************
*  ROUTINE: CreateSbuffer
*
*  DESCRIPTION: Copy contents of current Spacket into flat Sbuffer data 
*				structure.  
**********************************************************************/  
void
Packet::CreateSbuffer( int Sbuffer_entry )
{
	unsigned int i=0, j, NumArgs, function; 
	
	Sbuffer[Sbuffer_entry][i++] = GetAddress();                   
	Sbuffer[Sbuffer_entry][i++] = GetLength();             
	Sbuffer[Sbuffer_entry][i++] = GetState(); 

	// Message 1
	Sbuffer[Sbuffer_entry][i++] = Msg->GetLength(); 
	Sbuffer[Sbuffer_entry][i++] = Msg->GetFunction();
	
	function = Msg->GetFunction();
	switch (function)
	{
		case GetAttrInvoke:
			Sbuffer[Sbuffer_entry][i++] = Msg->GetInvokeID(); 
			Sbuffer[Sbuffer_entry][i++] = Msg->GetAttribID();
			break;
			
		case SetAttrInvoke:
			Sbuffer[Sbuffer_entry][i++] = Msg->GetInvokeID(); 
			Sbuffer[Sbuffer_entry][i++] = Msg->GetAttribID();
			NumArgs =  Msg->GetNumArgs(); 
			for (j=0; j<NumArgs; j++)
				Sbuffer[Sbuffer_entry][i++] = Msg->GetAttrDesc(j); 
			break;
			
		case SelfTestInvoke:
			break;
			
			
		default:
			break;
	}	
		             
	// Messages 2+
	while (Msg->GetNextMsg() != NULL)
	{
		Msg = Msg->GetNextMsg(); 
		
		Sbuffer[Sbuffer_entry][i++] = Msg->GetLength(); 
		Sbuffer[Sbuffer_entry][i++] = Msg->GetFunction();   
		
		function = Msg->GetFunction();
		switch (function)
		{
			case GetAttrInvoke:
				Sbuffer[Sbuffer_entry][i++] = Msg->GetInvokeID(); 
				Sbuffer[Sbuffer_entry][i++] = Msg->GetAttribID();
				break;
			
			case SetAttrInvoke:
				Sbuffer[Sbuffer_entry][i++] = Msg->GetInvokeID(); 
				Sbuffer[Sbuffer_entry][i++] = Msg->GetAttribID();
				NumArgs =  Msg->GetNumArgs(); 
				for (j=0; j<NumArgs; j++)
					Sbuffer[Sbuffer_entry][i++] = Msg->GetAttrDesc(j); 
				break;
			
			case SelfTestInvoke:
				break;
			
			default:
				break;
		}	
	}
			             
	Sbuffer[Sbuffer_entry][i++] = GetCRC1();                   
	Sbuffer[Sbuffer_entry][i++] = GetCRC2(); 
	                  
	SetLength( i );	// i is an unsigned char                  
}


/*********************************************************************
*  ROUTINE: ResetPacket
*
*  DESCRIPTION:
**********************************************************************/  
void
Packet::ResetPacket()
{                   
	Address = NULL;
	Length = NULL;

	// Reset State (keep Packet Numbers):
	SetConnEst( CONN_ESTAB_OFF );
	SetAckStatus( 0x00 );

	CRC = NULL;
	Status = EMPTY; 
	
	Msg->ResetMessage();
}


/*********************************************************************
*  ROUTINE: CreatePacket
*
*  DESCRIPTION:  Called from restore
**********************************************************************/
void
Packet::CreatePacket(unsigned char ackstatus, int Sbuffer_entry )
{
    Message *FirstMsg = this->Msg;
 
	// Set destination and source
    SetAddress( TM_TT );	// 0010 0001

	// Set Packet Length
	int plength = ( Msg->GetLengthI() + 5 );
	while( Msg->GetNextMsg() != NULL )
	{
   		Msg = Msg->GetNextMsg();
	   	plength += Msg->GetLengthI();
    }
    SetLength( plength );
    
	this->Msg = FirstMsg;
    
	// Set State components:                 
	SetConnEst( CONN_ESTAB_OFF );
	SetAckStatus( ackstatus );
		 
	// Compute and set CRC - moved to interrupt handler
     
   	// Prepare packet for interrupt handler
   	CreateSbuffer( Sbuffer_entry );
   	sndCount[Sbuffer_entry] = GetLengthI() - 2;
	PACKET_STATUS[Sbuffer_entry] = AWAITING_SEND;
}


/*********************************************************************
*  ROUTINE: CreateCEpacket
*                                    
*  DESCRIPTION:  
**********************************************************************/
void           
Packet::CreateCEpacket( int Sbuffer_entry )
{
    Message *FirstMsg = this->Msg;

	// reset packet numbers
	TTlastpnum = 0; 
	TTcurrpnum = 0;
	TMlastpnum = 0;
	TMcurrpnum = 0;
	             
	// reset Tcounter	             
	Tcounter = 0; 

   // Set destination and source
    SetAddress( TM_TT );	// 0010 0001

	// Set packet length
	int plength = ( Msg->GetLengthI() + 5 );
	while( Msg->GetNextMsg() != NULL )
	{
   		Msg = Msg->GetNextMsg();
	   	plength += Msg->GetLengthI();
    }
    SetLength( plength );
    
	this->Msg = FirstMsg;
    
	// Set state 
	SetPnum( 0 );
    SetConnEst( CONN_ESTAB_ON );
 	SetAckStatus( 0x00 );
		 
	// Compute and set CRC - moved to interrupt handler 
     
    // Prepare packet for interrupt handler
    CreateSbuffer( Sbuffer_entry );
    sndCount[Sbuffer_entry] = GetLengthI() - 2;
	PACKET_STATUS[Sbuffer_entry] = AWAITING_SEND;
}


/*********************************************************************
*  ROUTINE: InterpretPacket
*
*  DESCRIPTION:	Interpret and store Rbuffer2 to Rpacket data structure
*				Used with incoming packets
*
*  A packet has the following structure:
*	Address (Source, Destination)
*	Length
*	State (AckStatus, ConnEst, Packet#
*	Message
*	CheckCode
**********************************************************************/
void
Packet::InterpretPacket(unsigned int DestSource, 
                        unsigned int Plength, 
                        unsigned int State, 
                        unsigned int index)
{           
	unsigned int Mlength, m, n, inv, att, attval, result1, result2, result3;
	unsigned int msgnum=0, crc;
	int NumAttribs, s, sw=0;
	Message *Msg1, *Msg2;
	char *msg = new char[80];     
    unsigned int func; 
    static int first_time=YES;
    unsigned char crc1, crc2;
    
	       
	SetAddress( DestSource );
	SetLength( Plength );
	SetState( State );
	
   // less CRCs, index is post incremented
	while( index < (Plength - 2) )
	{
		// Message Length
		Mlength = Rbuffer2[index++];
			
		// To get to end of message: add current index, then
		//	subtract one for Mlength previously read
		m = Mlength + index - 1;
			
		// Message Function 
		func = Rbuffer2[index++];
	                              
		switch (func)
		{
		    case SetAttrResponse:
				inv = Rbuffer2[index++];
				att = Rbuffer2[index++];
				result1 = Rbuffer2[index++];
				result2 = Rbuffer2[index++];
				result3 = Rbuffer2[index++];
				
				if( (result1 != 0) || (result2 != 0) || (result3 != 0) )
					disp_str( "The Store command was NOT successful.", 1, 0, (int)NORM_DISP );

				break;
			
			case GetAttrResponse:
			 	while( index < m )
				{
					inv = Rbuffer2[index++];
					att = Rbuffer2[index++];
                         
					if( func == SetAttrResponse )
						NumAttribs = 3;
					else
						NumAttribs = (int)NumBytesTable[att];
							
					if (sw == 0)
					{
						// Point packet.Msg to Msg1
						Msg1 = new Message;
						this->Msg = Msg1;
						sw = 1;
					} 
					else if (sw == 1)
					{
						Msg2 = new Message;
						Msg1->SetNextMsg (Msg2);
						sw = 2;
					} 
					else 
					{
						Msg1 = new Message;
						Msg2->SetNextMsg( Msg1 ); 
						sw = 1;
					}
				
					Msg->SetInvokeID( inv );
					Msg->SetAttribID( att );
                
					Msg->SetLength( Mlength );                             
					Msg->SetFunction( func );                             
					Msg->SetNumArgs( NumAttribs );
				
					for (s=0; s<NumAttribs; s++)
					{
						attval = Rbuffer2[index++];
						Msg->SetAttrDesc( s, attval );
					}
					msgnum++;	
				}

				crc1 = Rbuffer2[index++];
				crc2 = Rbuffer2[index++];
	
				crc = crc2<<8;
				crc = crc | crc1;
				SetCRC ( crc ); 
                
				for (n=0; n<msgnum; n++)
				{
					Msg[n].InterpretMessage(); 
					delete[] Msg;
				}
			 
				delete[] Msg1;                        
				delete[] Msg2; 
	
				break;
					        	
		    default:
		    	break;
		}
	}
					
	ResetPacket();
	
	delete[] msg;
}    
    

/*********************************************************************
*  ROUTINE: ComputeCRC
*
*  DESCRIPTION: Used with outgoing packets
**********************************************************************/
unsigned short
Packet::ComputeCRC()
{
    unsigned int NumArgs, MsgNum=0, j, function; 
    unsigned short result=0; 
    Message *FirstMsg = this->Msg;
    
    calcCRC( result, (unsigned int)GetAddress() );
    calcCRC( result, (unsigned int)GetLength() );
    calcCRC( result, (unsigned int)GetState() ); 

	// Message 1
    calcCRC( result, (unsigned int)Msg->GetLength() );	
    calcCRC( result, (unsigned int)Msg->GetFunction() );		

	function = Msg->GetFunction();
	switch (function)
	{
		case GetAttrInvoke:
			calcCRC( result, Msg->GetInvokeID() ); 
			calcCRC( result, Msg->GetAttribID() );
			break;
			
		case SetAttrInvoke:
			calcCRC( result, Msg->GetInvokeID() ); 
			calcCRC( result, Msg->GetAttribID() );
			NumArgs =  Msg->GetNumArgs(); 
			for (j=0; j<NumArgs; j++)
				calcCRC( result, Msg->GetAttrDesc(j) ); 
			break;

		case SelfTestInvoke:
			break;
			
		default:
			break;
	}	
		             
		             
	// Messages 2+
	while( Msg->GetNextMsg() != NULL )
	{
		Msg = Msg->GetNextMsg();
		MsgNum++;
 	
	    calcCRC( result, (unsigned int)Msg->GetLength() );	
	    calcCRC( result, (unsigned int)Msg->GetFunction() );     

		function = Msg->GetFunction();
		switch (function)
		{
			case GetAttrInvoke:
				calcCRC( result, Msg->GetInvokeID() ); 
				calcCRC( result, Msg->GetAttribID() );
				break;
			
			case SetAttrInvoke:
				calcCRC( result, Msg->GetInvokeID() ); 
				calcCRC( result, Msg->GetAttribID() );
				NumArgs =  Msg->GetNumArgs(); 
				for (j=0; j<NumArgs; j++)
					calcCRC( result, Msg->GetAttrDesc(j) ); 
				break;
			
			case SelfTestInvoke:
				break;
			
			default:
				break;
		}
	}
	
	this->Msg = FirstMsg;

	return result;	             
}




//\/\/\/\/\/\/\/\/\/ MESSAGE CLASS FUNCTIONS /\/\/\/\/\/\/\/\/\/\/\/\/
 /*********************************************************************
*  ROUTINE: Message Constructor
*
*  DESCRIPTION:
**********************************************************************/
Message::Message()
{
	numargs = 1;

    Length = NULL;
	Function = NULL;
	Trailer = NULL;        //not used
    InvokeID = NULL;
    AttribID = NULL;                 
    
    for( unsigned int i=0; i<	MAX_ATTRIBS; i++ )
    	AttrDesc[i] = NULL; 
    	
	Next_Msg = (Message *)NULL;   
} 


/*********************************************************************
*  ROUTINE: ResetMessage
*
*  DESCRIPTION:
**********************************************************************/  
void
Message::ResetMessage()
{                   
	Length = NULL;
	Function = NULL;
	Next_Msg = NULL;   
	
    InvokeID = NULL;
    AttribID = NULL;                 
    
    for( unsigned int i=0; i<MAX_ATTRIBS; i++ )
    	AttrDesc[i] = NULL; 
		
	numargs = 0;
}


/*********************************************************************
*  ROUTINE: SendMessage
*
*  DESCRIPTION:  
**********************************************************************/
void
Message::SendMessage( unsigned char ackstatus, int Sbuffer_entry )
{                           
	// Bundle message with packet, and set Packet Status to AWAITING_SEND.             
	
    int temp = TMprotocol.GetCurrentSpacket();
	Packet *p = TMprotocol.GetSpacketPtr(temp);

	// Set message in Packet class to point to correct message 
	p->SetMsgPtr( this ); 
		
	// Fill in Packet information; set status to AWAITING_SEND
	// If no contact from transmitter in 6 talk/tick cycles,
	// re-establish connection.
	if( NEW_CONNECTION == YES)
	{
		NEW_CONNECTION = NO;
 		p->CreateCEpacket( Sbuffer_entry );
	}
	else if( Tcounter < TIMEOUT_TICKS )
		p->CreatePacket( ackstatus, Sbuffer_entry  );
   	else
   		p->CreateCEpacket( Sbuffer_entry );
        
	// Reset message in Packet class to point to correct message 
	p->SetMsgPtr( this );
}


/*************************************************************************** 
*  ROUTINE: InterpretMessage
*
*  DESCRIPTION: Read Msg.Data and prepare for screen to be updated.
*				Data contains data from all messages in current packet
*
*
*  A message has the following structure:
*	Length
*	Function
*	Arguments
*
*  Functions					Arguments
*  ---------                    ---------
*  -GetAttrInvoke(0)			InvokeID, AttributeID
*  +GetAttrResponse				InvokeID, AttributeID, AttrValue
*  -SetAttrInvoke				InvokeID, AttributeID, AttrValue			
*  +SetAttrResponse				InvokeID, AttributeID, ResultID (24 bits)			
*	EventInvoke					InvokeID, Data Samples				
*  -SelfTestInvoke						
*  +SelfTestResponse			Self Test Result		
*	SetUpMeasureInvoke			AttributeID		
*	RemoveMeasureInvoke			AttributeID		
*	RebootInvoke							
*	UploadInvoke							
*  -DownloadInvoke				Program Start Location, Max. Segments, CRC			
*  +DownloadResponse			Result (0=success, 1=failure)
*  -DownloadSegmentInvoke		Segment Number and Start Address, Contents	
*  +DownloadSegmentResponse(14)	Segment#, Result (0=success, 1=failure) 
*
*	+currently used incoming functions (TM to TT)
*	-currently used outgoing functions (TT to TM)
***************************************************************************/
void
Message::InterpretMessage()
{                          
	unsigned char newchar;
	char *result2, *result, r1, r2, r3;
	int newval;
	float newf;
	double newd;
	char *msg = new char[80]; 
	                 
    unsigned int anum = GetAttribID();  
    unsigned char *newarray = new unsigned char[15];
	unsigned int fnum = GetFunction();
    
	switch (anum)
	{
		case 1:	// SpO2 Rev Number - 168 bits
			// All bytes are displayed in x.y format,
			// where byte 1 is y and byte 2 is x	    	

			//  byte 0		- unused
	   		//  bytes 1,2   - HW Revision
   			newf = create_xy( GetAttrDesc(1), GetAttrDesc(2) ); 
			TMscreen1.ChangeSetting (6, newf ); 
				
	   		//  byte 3		- unused
    		//  bytes 4,5	- DSP Revision
   			newf = create_xy( GetAttrDesc(4), GetAttrDesc(5) ); 
			TMscreen1.ChangeSetting (7, newf ); 
				
	   		//  byte 6		- unused
    		//  bytes 7,8	- CAESAR Revision
   			newf = create_xy( GetAttrDesc(7), GetAttrDesc(8) ); 
			TMscreen1.ChangeSetting (8, newf ); 
				
       		//  byte 9		- unused
       		//  bytes 10,11	- FW Revision
   	    	newf = create_xy( GetAttrDesc(10), GetAttrDesc(11) ); 
			TMscreen1.ChangeSetting (9, newf ); 
				
    		//  byte 12		- unused
   			//  bytes 13,14	- SW Revision
   			newf = create_xy( GetAttrDesc(13), GetAttrDesc(14) ); 
			TMscreen1.ChangeSetting (10, newf ); 
				
    		//  byte 15		- unused
   			//  bytes 16,17	- Algorithm Revision 
   			newf = create_xy( GetAttrDesc(16), GetAttrDesc(17) ); 
			TMscreen1.ChangeSetting (11, newf ); 

    		//  byte 18		- unused
    		//  bytes 19,20	- unused
   			newf = create_xy( GetAttrDesc(19), GetAttrDesc(20) ); 
			TMscreen1.ChangeSetting (12, newf ); 

    		break;
    	case 2:  // Hardware/Firmware Revision - 48 bits
				
    		// bytes 0,1,2: Main PCB Info, FW Revision Number  
    		// Format A.xx.yy, where yy is the lower byte
    		// Example:  0x0A0102 will be displayed as A.01.02
    		// on the screen
	   		r1 = GetAttrDesc(2);
       		r2 = GetAttrDesc(1);
    		r3 = GetAttrDesc(0);
    		
    		result = new char[9];    
    		result[0] = (r1 & 0xF0) + 0x30;
    		result[1] = (r1 & 0x0F) + 0x37;
    		result[2] = '.';
    		result[3] = (r2 & 0xF0) + 0x30;
    		result[4] = (r2 & 0x0F) + 0x30;
    		result[5] = '.';
    		result[6] = (r3 & 0xF0) + 0x30;
    		result[7] = (r3 & 0x0F) + 0x30;
    		result[8] = '\0';
    		
			TMscreen1.ChangeSetting (3, result ); 

    		// bits 24,25,26,27 - Main PCB Info, HW Revision Number
    		// values include 0-7, where 7h=7
    		newchar = (GetAttrDesc(3) & 0x0F) + 0x30;
			TMscreen1.ChangeSetting (2, newchar ); 
			
            delete[] result;
            
    		break;
    	case 3:  // RF Option Number - 24 bits
	    	
			// bits 0-7 used,  0x01=001, 0x10=016, etc.
    		newchar = GetAttrDesc(0);                                 
    		
    		// Get corresponding frequency range
    		Display_RFRange( newchar );
				
    		break;
    	case 4:  // Miscellaneous Control - 24 bits
	    	
    		//  bit 0 - Frequency set by user/CE (0:CE/1:USER)
   			newval = GetAttrDesc(0) & 0x01;
   			if( newval == 0)
				 TMscreen2.ChangeSetting (2, 1, 1);		// NO  
   			else
				 TMscreen2.ChangeSetting (2, 0);		// YES  
				
    		//  bit 7 - Filter Frequency (0:50Hz/1:60Hz)
   			newval = GetAttrDesc(0) & 0x80; 
   			if( newval == 0)
				 TMscreen2.ChangeSetting (3, 0);		// 50 
   			else
				 TMscreen2.ChangeSetting (3, 1, 1);		// 60 

    		//  bit 9 - RF On/Off after 10 minutes (0:10 MIN/1:ON)
   			newval = GetAttrDesc(1) & 0x02; 
   			if( newval == 0)
				 TMscreen2.ChangeSetting (1, 0);		// YES  
   			else
				 TMscreen2.ChangeSetting (1, 1, 1);		// NO  
				
      		//  bit 1 - SpO2 Installed (0:NO/1:YES)
    		newval = GetAttrDesc(0) & 0x02;	 
    		if( newval == 0)
				TMscreen3.ChangeSetting (0, 1, 1);		// NO  
   			else
				TMscreen3.ChangeSetting (0, 0);			// YES  
				
	    	break;

    	case 8:  // Country Code - 24 bits          
    		// Format ###: bits 0-7 LSB,  bits 8-15 MSB
   			newval = asciihex( GetAttrDesc(1), GetAttrDesc(2) );
			TMscreen4.ChangeSetting (0, newval ); 
	    	break;
	    	
    	case 9:  // Locale Code - 24 bits
	    	
    		// Format #####: bits 0-7 LSB,  bits 8-15 MSB
   			newval = asciihex( GetAttrDesc(1), GetAttrDesc(2) );
 			TMscreen4.ChangeSetting (1, newval ); 
	   		break;

    	case 10:
    		// Main Board Part Number (96 bytes, 88 used)
    		// M####-#####, LSB first, byte 11=M
 
    		result2 = new char[12];
    		result2[0] = GetAttrDesc(0);               
    		result2[1] = GetAttrDesc(1);               
    		result2[2] = GetAttrDesc(2);               
    		result2[3] = GetAttrDesc(3);               
    		result2[4] = GetAttrDesc(4);               
    		result2[5] = GetAttrDesc(5);               
    		result2[6] = GetAttrDesc(6);               
    		result2[7] = GetAttrDesc(7);               
    		result2[8] = GetAttrDesc(8);               
    		result2[9] = GetAttrDesc(9);               
    		result2[10] = GetAttrDesc(10);               
    		result2[11] = '\0';               

 			TMscreen1.ChangeSetting (1, result2 ); 
            delete[] result2;
	    	break;
	    	
		case 11:  // Product Mode - 24 bits
			// Product Mode (uses bits 0,1,2,3)
			// TM=0h, Magic=1h (not implemented), TMJ=2h (not implemented) 
	    		
			newval =  GetAttrDesc(1) & 0x0F;	
			if( newval == 0)
 				TMscreen1.ChangeSetting (0, "tUHFman" ); 
   			else if( newval == 1 )
 				TMscreen1.ChangeSetting (0, "Magic" ); 
   			else if( newval == 2 )
 				TMscreen1.ChangeSetting (0, "tUHFman-J" ); 

    		break;

    	case 14:  // Frequency - 144 bits
   			newd = InterpretFrequency( GetAttrDesc(11), GetAttrDesc(14), GetAttrDesc(17) );
			TMscreen1.ChangeSetting (5, newd ); 
    		break;

    	case 17:  
			newval =  GetAttrDesc(1); 
			
			if( SELFTEST_STATUS == AWAITING_ATTR17 )
			{
				// tUHFman Status - 24 bits - in response to a reboot request
				// Look at bits 14 and 15 - error if either set to 1	
				if( ((newval && 0x80) == 1) || ((newval && 0x70) == 1) )
					msg = "Self Test Error.";
				else
					msg = "Self Test Completed Successfully";

				disp_str( msg, 1, 0, (int)NORM_DISP );
				SELFTEST_STATUS = COMPLETED;
				KEY_STATUS = ENABLED;
			} 
			else	// 3 Wire Lead Select - 24 bits, look at bits 11 and 12 
			{
				if( ((newval && 0x10) == 0) && ((newval && 0x08) == 0) )
					TMscreen2.ChangeSetting (0, 1, 1);		// NO, DISABLED  
 				else
 					TMscreen2.ChangeSetting (0, 0);			// YES, ENABLED 
			}
			
			break;
 				
    	default:  // Only the above function numbers are being used
    		break;
    }    
    
    delete[] newarray;
	delete[] msg;
    
	// now update the TeleTool screen				
	CurrentScreenPtr->DisplayScreen();
}



//\/\/\/\/\/\/\/\/\/\/\/\ NON CLASS FUNCTIONS /\/\/\/\/\/\/\/\/\/\/\/\/ 
 
/*********************************************************************
*  ROUTINE: comm_inits
*
*  DESCRIPTION:
**********************************************************************/
void
comm_inits()
{
    rcvCount = 0;      
    for( int i=0; i<10; i++ )
		sndCount[i] = 0;
	CreateMessages();   
	CreateNumBytesTable();
	KEY_STATUS = ENABLED;
	NEW_CONNECTION = YES;
 
	// The IR port has a packet limit of 1 packet, while
	// the COM port has a packet limit of 16.
	if( PORT == COM_PORT )
		NUM_PACKETS = NUM_COM_PACKETS;
	else 
		NUM_PACKETS = NUM_IR_PACKETS; 
}


/*********************************************************************
*  ROUTINE: pcTickHandler
*
*  DESCRIPTION: Service the real-time clock interrupt callout
**********************************************************************/
void 
_far _interrupt pcTickHandler()
{
    Tcounter++;
		
    _chain_intr(oldClockVector);
}


/*********************************************************************
*  ROUTINE: Display_RFRange
*
*  DESCRIPTION:  
**********************************************************************/
void
Display_RFRange( int rfoption )
{
	switch( rfoption )
	{
		case 0:
			TMscreen1.ChangeSetting( 4, "0    (0 - 400)" );
			TMscreen4.ChangeSetting( 2, "0" );
			break;
			
		case 1:
			TMscreen1.ChangeSetting( 4, "1    (406 - 412.5)" );
			TMscreen4.ChangeSetting( 2, "1" );
			break;
			
		case 2:
			TMscreen1.ChangeSetting( 4, "2    (412.5 - 421.5)" );
			TMscreen4.ChangeSetting( 2, "2" );
			break;
			
		case 3:
			TMscreen1.ChangeSetting( 4, "3    (421.5 - 430)" );
			TMscreen4.ChangeSetting( 2, "3" );
			break;
			
		case 4:
			TMscreen1.ChangeSetting( 4, "4    (430 - 440)" );
			TMscreen4.ChangeSetting( 2, "4" );
			break;
			
		case 5:
			TMscreen1.ChangeSetting( 4, "5    (440 - 450)" );
			TMscreen4.ChangeSetting( 2, "5" );
			break; 
			
		case 6:
			TMscreen1.ChangeSetting( 4, "6    (450 - 460)" );
			TMscreen4.ChangeSetting( 2, "6" );
			break; 
			
		case 7:
			TMscreen1.ChangeSetting( 4, "7    (460 - 470)" );
			TMscreen4.ChangeSetting( 2, "7" );
			break;
			
		case 8:
			TMscreen1.ChangeSetting( 4, "8    (470 - 480)" );
			TMscreen4.ChangeSetting( 2, "8" );
			break;
			
		case 9:
			TMscreen1.ChangeSetting( 4, "9    (480 - 490)" );
			TMscreen4.ChangeSetting( 2, "9" );
			break;
			
		case 10:
			TMscreen1.ChangeSetting( 4, "10   (490 - 500)" );
			TMscreen4.ChangeSetting( 2, "10" );
			break;
			
		case 11:
			TMscreen1.ChangeSetting( 4, "11   (500 - 512)" );
			TMscreen4.ChangeSetting( 2, "11" );
			break;
			
		case 12:
			TMscreen1.ChangeSetting( 4, "12   (512 - 524)" );
			TMscreen4.ChangeSetting( 2, "12" );
			break;
			
		case 13:
			TMscreen1.ChangeSetting( 4, "13   (524 - 536)" );
			TMscreen4.ChangeSetting( 2, "13" );
			break;
			
		case 14:
			TMscreen1.ChangeSetting( 4, "14   (536 - 548)" );
			TMscreen4.ChangeSetting( 2, "14" );
			CurrentScreen = TMscreen1;                         
			break;
			
		case 15:
			TMscreen1.ChangeSetting( 4, "15   (548 - 560)" );
			TMscreen4.ChangeSetting( 2, "15" );
			break;
			
		case 16:
			TMscreen1.ChangeSetting( 4, "16   (560 - 566)" );
			TMscreen4.ChangeSetting( 2, "16" );
			break;          
			
		default:
			TMscreen1.ChangeSetting( 4, "?" );
			TMscreen4.ChangeSetting( 2, "?" );
			break;
	}
}   


/*********************************************************************
*  ROUTINE: CreateMessages 
*
*  DESCRIPTION:  
**********************************************************************/
void
CreateMessages()
{ 
	Create_CopyMFTT_Msg1();		// COPY MF->TT
	Create_CopyMFTT_Msg2();		// COPY MF->TT
	Create_CopyMFTT_Msg3();		// COPY MF->TT
	Create_CopyMFTT_Msg4();		// COPY MF->TT
	Create_CopyMFTT_Msg5();		// COPY MF->TT
	Create_CopyMFTT_Msg6();		// COPY MF->TT
	Create_CopyMFTT_Msg7();		// COPY MF->TT
	Create_CopyMFTT_Msg8();		// COPY MF->TT
	Create_CopyMFTT_Msg9();		// COPY MF->TT
	Create_CopyMFTT_Msg10();	// COPY MF->TT
	
	Create_SelfTest_Msg();		// SELFTEST
	Create_Junk_Msg();
}


/*********************************************************************
*  ROUTINE: InterpretFrequency
*
*  DESCRIPTION: Create Message for Copying MF->TT
**********************************************************************/
double
InterpretFrequency(char a, char n, char z)
{ 
    unsigned int temp1 = z & 0x0F;
    long temp2 = a + (64 * (n + (temp1 * 256)));
    double temp3 = (double)12500 * temp2;
    double result = temp3/1000000;

	return( result );
} 


/*********************************************************************
*  ROUTINE: create_xy
*
*  DESCRIPTION:
**********************************************************************/
float
create_xy( unsigned char y, unsigned char x) 
{
	float fx = (float)x;
	float fy = (float)y;
	                               
	while (fy >= 1.0) fy /= 10;

	// If y=9, it should be displayed as 09, not 90		                               
	if( y < 10 ) 
		fy = fy * (float).1;
		
	float result = fx + fy;
	                  
	return( result );
}


/*********************************************************************
*  ROUTINE: undo_xy
*
*  DESCRIPTION:
**********************************************************************/
char
undo_xy( unsigned char one, unsigned char two) 
{
	int newone, result;
	
	switch (one)
	{
		case '1':
			newone = 10;
		case '2':
			newone = 20;
		case '3':
			newone = 30;
		case '4':
			newone = 40;
		case '5':
			newone = 50;
		case '6':
			newone = 60;
		case '7':
			newone = 70;
		case '8':
			newone = 80;
		case '9':
			newone = 90;
		default:
			newone = 0;
	}
	
	switch (one)
	{
		case '1':
			result = newone + 1;
		case '2':
			result = newone + 1;
		case '3':
			result = newone + 1;
		case '4':
			result = newone + 1;
		case '5':
			result = newone + 1;
		case '6':
			result = newone + 1;
		case '7':
			result = newone + 1;
		case '8':
			result = newone + 1;
		case '9':
			result = newone + 1;
		default:
			result = newone;
	}
	
	return( result );
}


/*********************************************************************
*  ROUTINE: Create_CopyMFTT_Msgs
*
*  DESCRIPTION: Create Messages for Copying MF->TT
**********************************************************************/
void
Create_CopyMFTT_Msg1()
{
 	CopyMFTT_Msg1.SetLength( 0x04 );
	CopyMFTT_Msg1.SetFunction( GetAttrInvoke ); 
	CopyMFTT_Msg1.SetInvokeID( 0x00 );
	CopyMFTT_Msg1.SetAttribID( 0x01 );			                      
	CopyMFTT_Msg1.SetNextMsg( NULL );                        
}

void
Create_CopyMFTT_Msg2()
{
	CopyMFTT_Msg2.SetLength( 0x04 ); 
	CopyMFTT_Msg2.SetFunction( GetAttrInvoke );	 
    CopyMFTT_Msg2.SetInvokeID( 0x00 );
    CopyMFTT_Msg2.SetAttribID( 0x02 );	                       
	CopyMFTT_Msg2.SetNextMsg( NULL );                        
}

void
Create_CopyMFTT_Msg3()
{
	CopyMFTT_Msg3.SetLength( 0x04 ); 
	CopyMFTT_Msg3.SetFunction( GetAttrInvoke );	 
    CopyMFTT_Msg3.SetInvokeID( 0x00 );
    CopyMFTT_Msg3.SetAttribID( 0x03 );	                     
	CopyMFTT_Msg3.SetNextMsg( NULL );                        
}

void
Create_CopyMFTT_Msg4()
{
	CopyMFTT_Msg4.SetLength( 0x04 ); 
	CopyMFTT_Msg4.SetFunction( GetAttrInvoke );	 
    CopyMFTT_Msg4.SetInvokeID( 0x00 );
    CopyMFTT_Msg4.SetAttribID( 0x04 );                          
	CopyMFTT_Msg4.SetNextMsg( NULL );                        
}

void
Create_CopyMFTT_Msg5()
{
	CopyMFTT_Msg5.SetLength( 0x04 ); 
	CopyMFTT_Msg5.SetFunction( GetAttrInvoke );	 
    CopyMFTT_Msg5.SetInvokeID( 0x00 );
    CopyMFTT_Msg5.SetAttribID( 0x08 );                           
	CopyMFTT_Msg5.SetNextMsg( NULL );                        
}

void
Create_CopyMFTT_Msg6()
{
	CopyMFTT_Msg6.SetLength( 0x04 ); 
	CopyMFTT_Msg6.SetFunction( GetAttrInvoke );	 
    CopyMFTT_Msg6.SetInvokeID( 0x00 );
    CopyMFTT_Msg6.SetAttribID( 0x09 );                      
	CopyMFTT_Msg6.SetNextMsg( NULL );                        
}
                                    
void
Create_CopyMFTT_Msg7()
{
	CopyMFTT_Msg7.SetLength( 0x04 ); 
	CopyMFTT_Msg7.SetFunction( GetAttrInvoke );	 
    CopyMFTT_Msg7.SetInvokeID( 0x00 );
    CopyMFTT_Msg7.SetAttribID( 0x0A );                
	CopyMFTT_Msg7.SetNextMsg( NULL );                        
}

void
Create_CopyMFTT_Msg8()
{
	CopyMFTT_Msg8.SetLength( 0x04 ); 
	CopyMFTT_Msg8.SetFunction( GetAttrInvoke );	 
    CopyMFTT_Msg8.SetInvokeID( 0x00 );
    CopyMFTT_Msg8.SetAttribID( 0x0B );         
	CopyMFTT_Msg8.SetNextMsg( NULL );                        
}

void
Create_CopyMFTT_Msg9()
{
	CopyMFTT_Msg9.SetLength( 0x04 ); 
	CopyMFTT_Msg9.SetFunction( GetAttrInvoke );	 
    CopyMFTT_Msg9.SetInvokeID( 0x00 );
    CopyMFTT_Msg9.SetAttribID( 0x0E );                         
	CopyMFTT_Msg9.SetNextMsg( NULL );                        
}


void
Create_CopyMFTT_Msg10()
{
	CopyMFTT_Msg10.SetLength( 0x04 ); 
	CopyMFTT_Msg10.SetFunction( GetAttrInvoke );	 
    CopyMFTT_Msg10.SetInvokeID( 0x00 );
    CopyMFTT_Msg10.SetAttribID( 0x11 );                         
	CopyMFTT_Msg10.SetNextMsg( NULL );                        
}                                                                      


/*********************************************************************
*  ROUTINE: Create_Junk_Msg
*
*  DESCRIPTION: Create Junk Message to use with QueryAnother()
**********************************************************************/
void
Create_Junk_Msg()
{
	Junk_Msg.SetLength( 0x04 ); 
	Junk_Msg.SetFunction( GetAttrInvoke );	 
    Junk_Msg.SetInvokeID( 0x00 );                       
    
	// OF is unused, will fall thru InterpretMessage();
    Junk_Msg.SetAttribID( 0x0F );
	Junk_Msg.SetNextMsg( NULL );                        
}


/*********************************************************************
*  ROUTINE: Create_CopyTTMF_Msgs
*
*  DESCRIPTION: Create Messages for Copying TT->MF
**********************************************************************/
void
Create_CopyTTMF_Msg1()
{
	CopyTTMF_Msg1.SetLength( 0x07 );	 
	CopyTTMF_Msg1.SetFunction( SetAttrInvoke ); 
    CopyTTMF_Msg1.SetInvokeID( 0x00 );
    CopyTTMF_Msg1.SetAttribID( 0x04 );
    
	// Miscellaneous - 24 bytes 
	//	byte  0 - bits 0, 1, 4, 7 used
	//		bit 0 - Frequency set by user/CE (field 3, screen 2)
	//		bit 1 - SpO2 HW enabled/disabled (field 1, screen 3)
	//		bit 7 - Notch filter 50/60 Hz (field 4, screen 2)
	//	byte  1 - bit 9 used 
	//		bit 9 - RF ON/OFF after 10 min. (field 2, screen 2)
	//	byte  2 - unused 
    CopyTTMF_Msg1.SetAttrDesc( 0, GetValue(2,0,0) );
    CopyTTMF_Msg1.SetAttrDesc( 1, GetValue(2,0,1) );
    CopyTTMF_Msg1.SetAttrDesc( 2, 0x00 ); 
    	
	CopyTTMF_Msg1.SetNextMsg( NULL ); 
	                       
	CopyTTMF_Msg1.SetNumArgs( 3 );                        
}   
    
void
Create_CopyTTMF_Msg2()
{
	CopyTTMF_Msg2.SetLength( 0x07 );	 
	CopyTTMF_Msg2.SetFunction( SetAttrInvoke ); 
    CopyTTMF_Msg2.SetInvokeID( 0x00 );
    CopyTTMF_Msg2.SetAttribID( 0x08 );
    
	// Country Code - 24 bytes, (field 0, screen 4)
	//	byte  0 - LSB   
	//	byte  1 - MSB 
	//	byte  2 - unused
    CopyTTMF_Msg2.SetAttrDesc( 0, GetValue(4,0,0) );	// screen, field, byte 
    CopyTTMF_Msg2.SetAttrDesc( 1, GetValue(4,0,1) ); 
    CopyTTMF_Msg2.SetAttrDesc( 2, 0x00 ); 
    	
	CopyTTMF_Msg2.SetNextMsg( NULL ); 
	                       
	CopyTTMF_Msg2.SetNumArgs( 3 );                        
}   
   
void
Create_CopyTTMF_Msg3()
{
	CopyTTMF_Msg3.SetLength( 0x07 );	 
	CopyTTMF_Msg3.SetFunction( SetAttrInvoke ); 
    CopyTTMF_Msg3.SetInvokeID( 0x00 ); 
    CopyTTMF_Msg3.SetAttribID( 0x09 );
    
	// Locale Code - 24 bytes, (field 1, screen 4)
	//	byte  0 - LSB   
	//	byte  1 - MSB 
	//	byte  2 - unused
    CopyTTMF_Msg3.SetAttrDesc( 0, GetValue(4,1,0) );	// screen, field, byte 
    CopyTTMF_Msg3.SetAttrDesc( 1, GetValue(4,1,1) ); 
    CopyTTMF_Msg3.SetAttrDesc( 2, 0x00 ); 
    	
	CopyTTMF_Msg3.SetNextMsg( NULL );   
	                     
	CopyTTMF_Msg3.SetNumArgs( 3 );                        
}   

    
void
Create_CopyTTMF_Msg4()
{
	CopyTTMF_Msg4.SetLength( 0x16 );	 
	CopyTTMF_Msg4.SetFunction( SetAttrInvoke ); 
    CopyTTMF_Msg4.SetInvokeID( 0x00 );
    CopyTTMF_Msg4.SetAttribID( 0x0E );
    
 	// Frequency - 18 bytes, (field 5, screen 1)
	//	byte  0 - unused  
	//	byte  1 - unused  
	//	byte  2 - unused  
	//	byte  3 - unused  
	//	byte  4 - unused  
	//	byte  5 - unused  
	//	byte  6 - unused  
	//	byte  7 - unused  
	//	byte  8 - unused    
	//	byte  9 - unused  
	//	byte 10 - unused  
	//	byte 11 - A  
	//	byte 12 - unused  
	//	byte 13 - unused  
	//	byte 14 - N
	//	byte 15 - unused  
	//	byte 16 - unused  
	//	byte 17 - unused  
    CopyTTMF_Msg4.SetAttrDesc(  0, 0x23 );	// screen, field, byte );
    CopyTTMF_Msg4.SetAttrDesc(  1, 0x00 ); 
    CopyTTMF_Msg4.SetAttrDesc(  2, 0x6C ); 
    CopyTTMF_Msg4.SetAttrDesc(  3, 0x22 ); 
    CopyTTMF_Msg4.SetAttrDesc(  4, 0x00 ); 
    CopyTTMF_Msg4.SetAttrDesc(  5, 0x00 ); 
    CopyTTMF_Msg4.SetAttrDesc(  6, 0x23 ); 
    CopyTTMF_Msg4.SetAttrDesc(  7, 0x00 ); 
    CopyTTMF_Msg4.SetAttrDesc(  8, 0x82 ); 
    CopyTTMF_Msg4.SetAttrDesc(  9, 0x21 ); 
    CopyTTMF_Msg4.SetAttrDesc( 10, 0x00 ); 
    CopyTTMF_Msg4.SetAttrDesc( 11, GetValue(4,19,11) ); 
    CopyTTMF_Msg4.SetAttrDesc( 12, 0x22 ); 
    CopyTTMF_Msg4.SetAttrDesc( 13, 0x00 ); 
    CopyTTMF_Msg4.SetAttrDesc( 14, GetValue(4,19,14) ); 
    CopyTTMF_Msg4.SetAttrDesc( 15, 0x23 ); 
    CopyTTMF_Msg4.SetAttrDesc( 16, 0x00 ); 
    CopyTTMF_Msg4.SetAttrDesc( 17, GetValue(4,19,17) ); 
    	
	CopyTTMF_Msg4.SetNextMsg( NULL ); 
	                       
	CopyTTMF_Msg4.SetNumArgs( 18 );                        
}                    


void
Create_CopyTTMF_Msg5()
{
	CopyTTMF_Msg5.SetLength( 0x07 );	 
	CopyTTMF_Msg5.SetFunction( SetAttrInvoke ); 
    CopyTTMF_Msg5.SetInvokeID( 0x00 );
    CopyTTMF_Msg5.SetAttribID( 0x16 );
    
	// 3-Wire Lead Select - 24 bytes 
	//	byte  0 - 00
	//	byte  1 - 
	//		00 if disabled
	//		08 if lead I
	//		10 if lead II (default)
	//		18 if lead III
	//	byte  2 - 00
	//
    //  set to default of lead II if enabled
    CopyTTMF_Msg5.SetAttrDesc( 0, 0x00 ); 
    CopyTTMF_Msg5.SetAttrDesc( 0, GetValue(2,1,1) );	   
    CopyTTMF_Msg5.SetAttrDesc( 0, 0x00 );
    	
	CopyTTMF_Msg5.SetNextMsg( NULL );
	                        
	CopyTTMF_Msg5.SetNumArgs( 3 );                        
}   
    

/*********************************************************************
*  ROUTINE: Create_SelfTest_Msg
*
*  DESCRIPTION: Create Message for Self Test
**********************************************************************/
void
Create_SelfTest_Msg()
{ 
	SelfTest_Msg.SetLength( 0x02 ); 
	
	// tUHFman does not support SelfTestInvoke, use RebootInvoke (09) instead
	SelfTest_Msg.SetFunction( RebootInvoke );      
	SelfTest_Msg.SetNextMsg( NULL );                          
}

 
/*********************************************************************
*  ROUTINE: asciihex
*
*  DESCRIPTION:
**********************************************************************/
unsigned int
asciihex( char one, char two)
{
    unsigned int result;
/*
    if (one >= '0' && one <= '9')
        one = one - '0';
    else
        one = one - '7';

    if (two >= '0' && two <= '9')
        two = two - '0';
    else
        two = two - '7';
*/
    result = (one << 4) + two;
    return result;
} 

 
/*********************************************************************
*  ROUTINE: ClearCopy_Rbuffer
*
*  DESCRIPTION: Copy and free up Rbuffer 
**********************************************************************/
void
ClearCopy_Rbuffer()
{					
	for (int k=0; k<=255; k++)
	{
		Rbuffer2[k] = Rbuffer[k];
		Rbuffer[k] = NULL;
	}
	rcvCount = 0; 
}
					

/*********************************************************************
*  ROUTINE: InterpretBuffer
*
*  DESCRIPTION: First determine if the packet is meant for TeleTool
*				If packet is meant for TeleTool, interpret data 
*				If packet is not meant for TeleTool, skip
**********************************************************************/
void
InterpretBuffer()
{
	unsigned int i=0; 
	unsigned char packetnum;
	                         
	PACKET_RECD = NO;
	unsigned int NUM_BYTES = rcvCount-1;
	
	if( ERROR_FLAG == YES )                                                                   
	{
    	for (unsigned int j=0; j<=rcvCount; j++)
        	Rbuffer[j] = '\0';     
    	rcvCount = 0;
    	ERROR_FLAG = NO;  
    }
    else
    {
		while (i <= NUM_BYTES)
		{                                                      
			unsigned int DestSource = Rbuffer[i++]; 
			unsigned int Plength = Rbuffer[i++];
			unsigned int State = Rbuffer[i++];
		
			packetnum = State & 0x0F;

	    	Packet *currp = TMprotocol.GetCurrentRpacketPtr(); 
			ClearCopy_Rbuffer(); 
			currp->InterpretPacket(DestSource, Plength, State, i);

		    i += ( (Plength*2) - 3);   // advance to next packet
		}
	}
	
	if( CURRENT_PACKET == MAX_PACKETS )
	{
		KEY_STATUS = ENABLED;
		 
		if( INIT_IN_PROGRESS == NO ) 
		{
			// inform user that action is complete
			disp_str( "Requested Action Completed.", 1, 0, (int)NORM_DISP );
			
			// if store function was performed, query user
			if( QA_STATUS == YES )
			{
				QueryAnother();
				QA_STATUS = NO;
			}
				
		} else
    		INIT_IN_PROGRESS = NO;
	}    
	
}


/*********************************************************************
*  ROUTINE: restore
*
*  DESCRIPTION: copy settings from Mainframe to TeleTool
**********************************************************************/
void
restore()
{ 
	disp_str( "Copying to TeleTool...", 1, 0, (int)NORM_DISP );
	
	KEY_STATUS = DISABLED;
	CURRENT_PACKET = 0;
	MAX_PACKETS = 9;
	
	CopyMFTT_Msg1.SendMessage( NOACKMSG, 0 );                        
	CopyMFTT_Msg2.SendMessage( NOACKMSG, 1 );
	CopyMFTT_Msg3.SendMessage( NOACKMSG, 2 );                        
	CopyMFTT_Msg4.SendMessage( NOACKMSG, 3 );                        
	CopyMFTT_Msg5.SendMessage( NOACKMSG, 4 );                        
	CopyMFTT_Msg6.SendMessage( NOACKMSG, 5 );                        
	CopyMFTT_Msg7.SendMessage( NOACKMSG, 6 );                        
	CopyMFTT_Msg8.SendMessage( NOACKMSG, 7 );                        
	CopyMFTT_Msg9.SendMessage( NOACKMSG, 8 );                        
	CopyMFTT_Msg10.SendMessage( NOACKMSG, 9 ); 
}


/*********************************************************************
*  ROUTINE: store
*
*  DESCRIPTION: copy settings from TeleTool to Transmitter
**********************************************************************/
void
store()
{
	if( FREQ_VERIFIED == YES )
    {
		disp_str( "Copying to Transmitter...", 1, 0, (int)NORM_DISP );

		QA_STATUS = YES;  
 		KEY_STATUS = DISABLED;
		CURRENT_PACKET = 0;
		MAX_PACKETS = 4;  
	
		Create_CopyTTMF_Msg1(); 
		Create_CopyTTMF_Msg2();	
		Create_CopyTTMF_Msg3();
		Create_CopyTTMF_Msg4();
		Create_CopyTTMF_Msg5();
	
		CopyTTMF_Msg1.SendMessage( NOACKMSG, 0 ); 
		CopyTTMF_Msg2.SendMessage( NOACKMSG, 1 ); 
		CopyTTMF_Msg3.SendMessage( NOACKMSG, 2 ); 
		CopyTTMF_Msg4.SendMessage( NOACKMSG, 3 ); 
		CopyTTMF_Msg5.SendMessage( NOACKMSG, 4 );
	} else
		disp_msg("Frequency/Check Code mismatch.");
}


/*********************************************************************
*  ROUTINE: GetAttr17
*
*  DESCRIPTION:  
**********************************************************************/
void
GetAttr17()
{
	CURRENT_PACKET = 0;
	MAX_PACKETS = 0;
	
	CopyMFTT_Msg10.SendMessage( NOACKMSG, 0 );
}
       

/*********************************************************************
*  ROUTINE: CreateNumBytesTable
*
*  DESCRIPTION: Create tables containing message lengths in bytes
**********************************************************************/
void
CreateNumBytesTable()
{
    NumBytesTable[0]  = 0;		// NOT USED 
    NumBytesTable[1]  = 21;		// Sp02 Rev Number 
    NumBytesTable[2]  = 6;		// HW/FW Rev Number 
    NumBytesTable[3]  = 3;		// Rf Option Number 
    NumBytesTable[4]  = 3;		// Miscellaneous Control
    NumBytesTable[5]  = 0;		// NOT USED 
    NumBytesTable[6]  = 0;		// NOT USED 
    NumBytesTable[7]  = 0;		// NOT USED 
    NumBytesTable[8]  = 3;		// Country Code 
    NumBytesTable[9]  = 3;		// Locale Code 
    NumBytesTable[10] = 12;		// Main PCB Part Number 
    NumBytesTable[11] = 3;		// Product Mode 
    NumBytesTable[12] = 0;		// NOT USED 
    NumBytesTable[13] = 0;		// NOT USED 
    NumBytesTable[14] = 18;		// RF Synthsizer Frequency 
    NumBytesTable[15] = 0;		// NOT USED 
    NumBytesTable[16] = 0;		// NOT USED 
    NumBytesTable[17] = 3;		// 3 Wire Select
    NumBytesTable[18] = 0;		// NOT USED 
    NumBytesTable[19] = 0;		// NOT USED 
    NumBytesTable[20] = 0;		// NOT USED 
    NumBytesTable[21] = 0;		// NOT USED 
    NumBytesTable[22] = 3;		// 3 Wire Select
    
    for (int i=23; i<40; i++)
    	NumBytesTable[i] = 0;		// NOT USED 
}  
  
  
/*********************************************************************
*  ROUTINE: GetValue
*
*  DESCRIPTION:  
**********************************************************************/
char
GetValue(int screen, int field, int byte)
{
	char answer;
    Field *ptr;
    int i, CurrentSetting, CurrField;
    double dvalue; 
    
	if( screen == 2 )
	{
		// There are three calls to screen2:
		//	GetValue( 2,0,0 ) - includes:
		//		Ability for user to change frequency (bit 0)
		//		SpO2 Installed (bit 1)
		//		Filter Frequency (bit 7)  
		//	GetValue( 2,0,1 ) 
		//		Auto Shutoff (bit 9)
		//	GetValue( 2,1,1 ) 
		//		3 Wire Lead Select - byte 1
			        
		if( field == 0 )
		{
   			if( byte == 0 )
   			{
				ptr = TMscreen2.GetFieldPtr();
				ptr = ptr->Next ();	 // skip Lead Selection field
				ptr = ptr->Next ();	 // skip Auto Shutoff field
				CurrentSetting = ptr->Getcurrent_setting();
				if( CurrentSetting == 0 )
					answer = 0x01;
				else
					answer = 0x00;

				ptr = ptr->Next ();	
				CurrentSetting = ptr->Getcurrent_setting();
				if( CurrentSetting == 0 )
					answer = answer | 0x00;
				else
					answer = answer | 0x80;
    			
				ptr = TMscreen3.GetFieldPtr();
				CurrentSetting = ptr->Getcurrent_setting();
				if( CurrentSetting == 0 )
					answer = answer | 0x02;
				else
					answer = answer | 0x00;
    			
    			return( answer );		    

    		} else if( byte == 1) { 
				ptr = TMscreen2.GetFieldPtr();
				ptr = ptr->Next ();				// skip first field
				CurrentSetting = ptr->Getcurrent_setting();
				if( CurrentSetting == 0 )
					answer = 0x00;
				else
					answer = 0x02;
				
    			return( answer );		    
			} else
				return( 0x00 );                  
		} else if( field == 1 ) {
			ptr = TMscreen2.GetFieldPtr();
			CurrentSetting = ptr->Getcurrent_setting();
			if( CurrentSetting == 0 )	// YES
				answer = 0x10;	// lead II - default
			else
				answer = 0x00;	// disabled
				
			return( answer );		    
		} else
			return( 0x00 );
    }        
	else if( screen == 4 )
	{
		switch (field)
		{
			case 0: 
				// Country Code
				return( 0x00 );
    			break;  
    				
			case 1:            
				// Locale Code
				return( 0x00 ); 
				break;
					
			case 19: 
				// Frequency
				ptr = TMscreen4.GetFieldPtr(); 
				CurrField = TMscreen4.GetCurrField();
				
				switch( CurrField )
				{
					case 19:
					case 20:
						CurrField = 19;
						break;
						
					case 21:
					case 22:
						CurrField = 21;
						break;
						
					case 23:
					case 24:
						CurrField = 23;
						break;
						
					case 25:
					case 26:
						CurrField = 25;
						break;
						
					case 27:
					case 28:
						CurrField = 27;
						break;
						
					case 29:
					case 30:
						CurrField = 29;
						break;
						
					case 31:
					case 32:
						CurrField = 31;
						break;
						
					case 33:
					case 34:
						CurrField = 33;
						break;
						
					default:
						CurrField = 19;
						break;
				}
				
				for (i=0; i<CurrField; i++)
					ptr = ptr->Next(); 
				dvalue = CharToDouble( ptr->GetCvalue() );

				// Main PCB Frequency
				if (byte == 11)			// A
					return( GetByte(11, dvalue) );
				else if (byte == 14)	// N
					return( GetByte(14, dvalue) );
				else if (byte == 17)	 
					return( GetByte(17, dvalue) );
				else 
					return( 0x00 );
    			break;  
    			
    		default:
    			return( 0x00 );
    			break; 
		} 
	} else
		return( 0x00 );
}


/*********************************************************************
*  ROUTINE: GetFreqOptionNum
*
*  DESCRIPTION:  
**********************************************************************/ 
int
GetFreqOptionNum(double freq)
{                    
	if( freq < 412.5 )
		return( 1 );
	else if( freq >= 412.5 && freq < 421.5 )
		return( 2 );
	else if( freq >= 421.5 && freq < 430.0 )
		return( 3 );
	else if( freq >= 430.0 && freq < 440.0 )
		return( 4 );
	else if( freq >= 440.0 && freq < 450.0 )
		return( 5 );
	else if( freq >= 450.0 && freq < 460.0 )
		return( 6 );
	else if( freq >= 460.0 && freq < 470.0 )
		return( 7 );
	else if( freq >= 470.0 && freq < 480.0 )
		return( 8 );
	else if( freq >= 480.0 && freq < 490.0 )
		return( 9 );
	else if( freq >= 490.0 && freq < 500.0 )
		return( 10 );
	else if( freq >= 500.0 && freq < 512.0 )
		return( 11 );
	else if( freq >= 512.0 && freq < 524.0 )
		return( 12 );
	else if( freq >= 524.0 && freq < 536.0 )
		return( 13 );
	else if( freq >= 536.0 && freq < 548.0 )
		return( 14 );
	else if( freq >= 548.0 && freq < 560.0 )
		return( 15 );
	else if( freq >= 560.0 && freq < 566.0 )
		return( 16 );
	else
		return( 0 );
}


/*********************************************************************
*  ROUTINE: GetByte
*
*  DESCRIPTION:  Return either byte 11, 14 or 17 of frequency  
**********************************************************************/
char
GetByte(int bytenum, double freq)
{
    double b,y,z;
    unsigned int A,N;
    int x;
    
    b=12500.;
    freq = freq * 1000000.;
    y=(int) ((freq/b)/64.);
    z=(freq/b) - 64*y;

    N= (unsigned int)y;
    N=N & 255;
    A= (unsigned int)z;

	x = (unsigned int)y;
	x = (x >> 8) & 0x0F;

	switch( bytenum )
	{
		case 11:
			return( A );  
			break;
		case 14:
 			return( N );
 			break;
		case 17:
 			return( 0x70 | x );
 			break;
		default:
			return( 0x00 );
			break;
	}
}


/*********************************************************************
*  ROUTINE: TransferPCB
*
*  DESCRIPTION: Upload PCB Firmware 
**********************************************************************/
void
TransferPCB()
{
	// 1. Reset interrupt handler and communication settings:
	_disable();
    _dos_setvect(VEC_ADDR, oldUARTvector); 
    _dos_setvect(0x1C, oldClockVector);		//Restore Vector in Int Vector Table
	_asm 
	{		
        MOV             DX,RBR_THR				;clear RBR_THR
        IN              AL,DX

		MOV             AL,20H              	;OK to clear current interrupt
		OUT             20H,AL              	;write 20H to 20H

        IN        		AL,21H					;unmask bit int PIC's IMR
        AND       		AL,PIC_ENABLE			;enable IRQ4/COM1: EF  (IRQ3/COM2: F7)
        OUT       		21H,AL

 		MOV 			AL,18H                  ;assert GPO2 - enable INTRP line (MCR) 
		MOV       		DX,MCR                  ;enable UART interrupt to drive IRQ.                                        
		OUT       		DX,AL
	}
	_enable();
	

	// 2. Call external program to update Spo2
    system( "pcb_upg" );
    
	
	// 3. Set interrupt handler and communication settings
	InstallInterruptHandler(); 
    
    // Install timer handler 
	_disable();
	_dos_setvect(0x1C,pcTickHandler);
	_enable();

	// Re-initialize some variables
    rcvCount = 0;      
    for( int i=0; i<10; i++ )
		sndCount[i] = 0;
	KEY_STATUS = ENABLED;
	NEW_CONNECTION = YES;
}

				
/*********************************************************************
*  ROUTINE: SelfTest
*
*  DESCRIPTION:  
**********************************************************************/
void SelfTest() 
{
	KEY_STATUS = DISABLED;
	SELFTEST_STATUS = IN_PROGRESS;
	CURRENT_PACKET = 0;
	MAX_PACKETS = 0;
	
	SelfTest_Msg.SendMessage( NOACKMSG, 0 );
}
 
 
/*********************************************************************
*  ROUTINE: QueryUser
*
*  DESCRIPTION:  
**********************************************************************/ 
int
QueryUser()
{ 
    int i = 0;
    char c;

    disp_str("Do you want to install another transmitter (Y/N): ", 2, 1, NORM_DISP);
    pos_cursor(2, 50);
    c = getche();       
    
	// clear display
	disp_str("", 1, 0, NORM_DISP);
	disp_str("", 2, 0, NORM_DISP);

    if( (c == 'Y') || (c == 'y') )
    	return TRUE;
    else
   		return FALSE;
}


/*********************************************************************
*  ROUTINE: QueryAnother
*
*  DESCRIPTION: The user will have the option to disconnect from 
*				the existing transmitter and hookup another transmitter.  
**********************************************************************/
void
QueryAnother()
{ 
	char c;
	
	// 1. Prompt user
	if( QueryUser() == TRUE ) 
	{   
		// 2. Prompt user to connect new transmitter, then press enter to continue
		disp_msg ("Connect new transmitter, then press the enter key...\n");
    	c = getche();
    	while( c != '\r' )  ;
		
		// 3. Call CreateCEpacket - this will fill in Sbuffer and prepare
		//    packet for sending.
		CURRENT_PACKET = 0;
		MAX_PACKETS = 0;
		NEW_CONNECTION = YES;
		Junk_Msg.SendMessage( NOACKMSG, 0 );                        
		
        // 4. Inform user
		disp_msg ("New connection has been established.\n");
	}
}


/*********************************************************************
*  ROUTINE: TransferSpO2
*
*  DESCRIPTION:
**********************************************************************/
void 
TransferSpO2()
{
	// 1. Reset interrupt handler and communication settings:
	_disable();
    _dos_setvect(VEC_ADDR, oldUARTvector); 
    _dos_setvect(0x1C, oldClockVector);		//Restore Vector in Int Vector Table
	_asm 
	{		
        MOV             DX,RBR_THR				;clear RBR_THR
        IN              AL,DX

		MOV             AL,20H              	;OK to clear current interrupt
		OUT             20H,AL              	;write 20H to 20H

        IN        		AL,21H					;unmask bit int PIC's IMR
        AND       		AL,PIC_ENABLE			;enable IRQ4/COM1: EF  (IRQ3/COM2: F7)
        OUT       		21H,AL

 		MOV 			AL,18H                  ;assert GPO2 - enable INTRP line (MCR) 
		MOV       		DX,MCR                  ;enable UART interrupt to drive IRQ.                                        
		OUT       		DX,AL
	}
	_enable();
	

	// 2. Call external program to update Spo2
    system( "spo2_upg" );
    
	
	// 3. Set interrupt handler and communication settings
	InstallInterruptHandler(); 
    
    // Install timer handler 
	_disable();
	_dos_setvect(0x1C,pcTickHandler);
	_enable();

	// Re-initialize some variables
    rcvCount = 0;      
    for( int i=0; i<10; i++ )
		sndCount[i] = 0;
	KEY_STATUS = ENABLED;
	NEW_CONNECTION = YES;
}
 

/*********************************************************************
*  ROUTINE: sleep
*
*  DESCRIPTION:
**********************************************************************/
void sleep( clock_t wait )
{
   clock_t goal;
   goal = wait + clock();
   while( goal > clock() )
      ;
}

