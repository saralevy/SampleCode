/*********************************************************************
*  Author:    Sara Levy
*  Date:      
*  Modififed:
*  Revision:  TeleTool 2.0
*
*  FILE NAME: inthndlr.cpp
*
*  DESCRIPTION:
**********************************************************************/


#include <stdio.h>
#include <bios.h>
#include <dos.h> 
#include "crc.h" 
#include "protocol.h"
#include "packet.h" 
#include "comm.h"    

#define TIMEOUT 0x0100  

static unsigned char    rcvByte = 0;         
static unsigned char    sndByte;
unsigned int volatile	rcvCount = 0;
unsigned int			sndCount[10];
unsigned char			Rbuffer[255];
unsigned char			Sbuffer[10][100];
unsigned char			ACKbuffer[5];
unsigned char			CRC1 = 0x00;
unsigned char			CRC2 = 0x00;
int volatile			PACKET_RECD = NO;   
int	volatile			ERROR_FLAG = NO;   
int	volatile			PACKET_STATUS[10];
int	volatile			CURRENT_PACKET;
int						ACK_TO_SEND = NO;
int						ACKRECD = NO;   
static unsigned char	PacketLength = 0;
static unsigned char	DestSrc = 0;;
unsigned char volatile	TTlastpnum = 0x00; 
unsigned char volatile	TTcurrpnum = 0x00;
unsigned char volatile	TMlastpnum = 0x00;
unsigned char volatile	TMcurrpnum = 0x00; 
unsigned short			i = 0x0000; 
unsigned short			CRCresult;
unsigned short			CRCack;
unsigned short			CRCrecd;
unsigned short			CRCdata = 0x00; 

extern int volatile		Tcounter;
extern int volatile		SELFTEST_STATUS;
extern void sleep(int);

void (_cdecl _far _interrupt * oldUARTvector)();  

void _cdecl _far _interrupt UARThandler()
{
	_asm
	{
		CLI												;disable interrupts
		
		IN				AL,21H							;disable IRQ4
		OR				AL,PIC_DISABLE
		OUT				21H,AL
		
		MOV				DX,MCR							;disable UART bit in MCR
		MOV				AL,07H		
		OUT				DX,AL
		
		
		MOV             DX,IIR							;load received char
		IN              AL,DX
		AND             AL,07H              			;mask bits 3-7

		CMP             AL,06H              			;test for Framing, Overrun Errors/BREAK
		JE              _ErrorHandler

		CMP             AL,04H              			;test for Data Received
		JE              _ReadByteOne

		CMP             AL,01H              			;test for No Byte Pending
		JE              _NoBytePending
		JMP             BogusInt

		_ErrorHandler:  JMP ErrorHandler
		_ReadByteOne:   JMP ReadByteOne
		_NoBytePending: JMP End
    }


    _asm
    {
		ErrorHandler:
		MOV             DX,LSR
		IN              AL,DX							;clear LSR
		MOV             DX,RBR_THR
		IN              AL,DX							;clear RBR_THR
		MOV             DX,MSR
		IN              AL,DX							;clear MSR
 		JMP             End                 			;ignore error or BREAK at this point
	
		// Read the received character, then test for overrun invalidating data.
		ReadByteOne: 
		MOV             DX,RBR_THR						;load received char
		IN              AL,DX
		MOV             rcvByte,AL          			;copy recd char to rcvByte  
    }                             


	// Beginning of code to read FF's and subsequent EA/E5
	while( (rcvByte != TALK) && (rcvByte != HUSH) )
	{
		_asm 
		{                                       
			MOV				i,0000H
			
			BegLoopFF:	
			INC				i
			CMP             i,TIMEOUT
			JE				NoDataRecdFF				
	
			MOV             DX,IIR						;load received char
			IN              AL,DX               	
			AND             AL,07H              		;mask bits 3-7

			CMP             AL,06H              		;Test 1: Framing, Overrun Errors/BREAK
			JNE             NoErrorFF 
			MOV             DX,LSR              
			IN              AL,DX						;clear LSR
			MOV             DX,RBR_THR
			IN              AL,DX						;clear RBR_THR
			MOV             DX,MSR
			IN              AL,DX						;clear MSR
			JMP 			BegLoopFF
			
			NoDataRecdFF:	
		}
		ERROR_FLAG = YES;								// timeout - jump to end 
 		_asm 
		{
			JMP				End
	
			NoErrorFF:
			CMP             AL,04H              		;Test 2: Data Received
			JNE				BegLoopFF
			MOV             DX,RBR_THR					;load received char
			IN              AL,DX
			MOV             rcvByte,AL      			;copy recd char to FFchar
		}
	}
	// End of code to read FF's and subsequent EA/E5

    
	if ( rcvByte == TALK )  							// EA
	{
		Tcounter = 0;
	
	     // Read F0, then send data ....
		_asm 
		{                                       
			MOV				i,0000H
			
			BegLoopF0:
			INC				i
			CMP             i,TIMEOUT
			JE				NoDataRecdF0				
			
			MOV             DX,IIR						;load received char
			IN              AL,DX
			AND             AL,07H              		;mask bits 3-7

			CMP             AL,06H              		;Test 1: Framing, Overrun Errors/BREAK
			JNE             NoErrorF0
			MOV             DX,LSR              
			IN              AL,DX						;clear LSR
			MOV             DX,RBR_THR
			IN              AL,DX						;clear RBR_THR
			MOV             DX,MSR
			IN              AL,DX						;clear MSR
			JMP 			BegLoopF0
			
			NoDataRecdF0:	
		}
		ERROR_FLAG = YES;								// timeout - jump to end 
		_asm 
		{
			JMP				End
		
			NoErrorF0:
			CMP             AL,04H              		;Test 2: Data Received
			JNE				BegLoopF0
			MOV             DX,RBR_THR					;load received char
			IN              AL,DX
			MOV             rcvByte,AL     				;copy F0 char to rcvByte  
		}
        // End of code to read F0
            
		if( (ACK_TO_SEND == YES) || 
			(PACKET_STATUS[CURRENT_PACKET] == AWAITING_SEND) || 
			(PACKET_STATUS[CURRENT_PACKET] == RESEND_PACKET) )
		{
			_asm    JMP			SendDataSection 
		}
		else if( PACKET_STATUS[CURRENT_PACKET] >= AWAITING_ACK )
		{
			PACKET_STATUS[CURRENT_PACKET]++;
			_asm    JMP			End
     	}
	}
	
	
	else if ( rcvByte == HUSH ) 						// E5 - incoming data to follow....
	{
		Tcounter = 0;
		rcvCount = 0;
		CRCresult = 0;
	
	     // Beginning of code to read Destination/Source
		_asm 
		{                                       
			MOV				i,0000H
			
			BegLoopDS:
			INC				i
			CMP             i,TIMEOUT
			JE				NoDataRecdDS				
			
			MOV             DX,IIR						;load received char
			IN              AL,DX
			AND             AL,07H              		;mask bits 3-7

			CMP             AL,06H              		;Test 1: Framing, Overrun Errors/BREAK
			JNE             NoErrorDS
			MOV             DX,LSR              
			IN              AL,DX						;clear LSR
			MOV             DX,RBR_THR
			IN              AL,DX						;clear RBR_THR
			MOV             DX,MSR
			IN              AL,DX						;clear MSR
			JMP 			BegLoopDS
			
			NoDataRecdDS:	
		}
		ERROR_FLAG = YES;								// timeout - jump to end 
		_asm 
		{
			JMP				End
		
			NoErrorDS:
			CMP             AL,04H              		;Test 2: Data Received
			JNE				BegLoopDS
			MOV             DX,RBR_THR					;load received char
			IN              AL,DX
			MOV             DestSrc,AL     				;copy Destination/Source to rcvByte  
		}
        // End of code to read Destination/Source
        
		if( DestSrc == TT_TM )
		{
			Rbuffer[rcvCount++] = DestSrc;
			calcCRC( CRCresult, DestSrc );
		}  else if (DestSrc == 0xF0 )
			_asm JMP End
            
		// Beginning of code to read PacketLength
		_asm 
		{                                       
			PacketLengthCode:
			MOV				i,0000H
			
			BegLoop1:
			INC				i
			CMP             i,TIMEOUT
			JE				NoDataRecd1				
			
			MOV             DX,IIR					;load received char
			IN              AL,DX
			AND             AL,07H              	;mask bits 3-7

			CMP             AL,06H              	;Test 1: Framing, Overrun Errors/BREAK
			JNE             NoError1 
			MOV             DX,LSR              
			IN              AL,DX					;clear LSR
			MOV             DX,RBR_THR
			IN              AL,DX					;clear RBR_THR
			MOV             DX,MSR
			IN              AL,DX					;clear MSR
			JMP 			BegLoop1
				
			NoDataRecd1:	
		}
		ERROR_FLAG = YES;							// timeout - jump to end 
		_asm 
		{
			JMP				End

			NoError1:
			CMP             AL,04H              	;Test 2: Data Received
			JNE				BegLoop1
			MOV             DX,RBR_THR				;load received char
			IN              AL,DX
			MOV             PacketLength,AL     	;copy recd char to rcvByte  
		}
        // End of code to read PacketLength
            
		if( DestSrc == TT_TM )
		{
    	    Rbuffer[rcvCount++] = PacketLength;
        	calcCRC( CRCresult, PacketLength );
		}
			
		for( int k=2; k<PacketLength; k++ )
		{
			_asm 
			{
				MOV				i,0000H
			
				BegLoop2:
				INC				i
				CMP             i,TIMEOUT
				JE				NoDataRecd2	
				
				MOV             DX,IIR				;load received char
				IN              AL,DX
				AND             AL,07H              ;mask bits 3-7

				CMP             AL,06H              ;Test 1: Framing, Overrun Errors/BREAK
				JNE             NoError2 
				MOV             DX,LSR
				IN              AL,DX				;clear LSR
				MOV             DX,RBR_THR
				IN              AL,DX				;clear RBR_THR
				MOV             DX,MSR
				IN              AL,DX				;clear MSR
				JMP 			BegLoop2
					
				NoDataRecd2:
			}
			ERROR_FLAG = YES; 
			_asm 
			{
				JMP				End
			    	
				NoError2:
				CMP             AL,04H              ;Test 2: Data Received
				JNE				BegLoop2
				MOV             DX,RBR_THR			;load received char
				IN              AL,DX
				MOV             rcvByte,AL    	 	;copy recd char to rcvByte  
			}
				
			if( DestSrc == TT_TM )
            {
        		Rbuffer[rcvCount++] = rcvByte;
        		if( k < PacketLength-2 )				// don't calcCRC the CRC bytes
      				calcCRC( CRCresult, rcvByte );
      			if( k == 2 )
      				TMcurrpnum = rcvByte;
			}
		}	

		if( DestSrc == TT_TM )
        {
			CRCrecd = Rbuffer[rcvCount-1] << 8;
			CRCrecd = CRCrecd | Rbuffer[rcvCount-2];
		
			if( rcvCount == 5 )					// packet is an ACK packet - will always be sent first
			{								
				if( CRCresult == CRCrecd )
				{
					ERROR_FLAG = NO;
					rcvCount = 0;				// faster not to reset Rbuffer - will be over-written
     			}
     			
     			// if packet number of incoming ack packet matches packet
     			// number from last outgoing request, set ACKRECD (Vic)
     			if( (TMcurrpnum & 0x0F) == (Sbuffer[CURRENT_PACKET][2] & 0x0F) )
     				ACKRECD = YES;
                    
				if( SELFTEST_STATUS == IN_PROGRESS )
					SELFTEST_STATUS = SELFTEST_ACK_RECD;
			}
			else if( CRCresult != CRCrecd )
			{
				PACKET_RECD = YES;				// Reset to NO in InterpretBuffer
				ERROR_FLAG = YES;
				_asm    JMP			End
			}
			else if( (ERROR_FLAG == NO) && (CRCresult == CRCrecd) )
			{
				// if the packet number from the newly received data packet
				// from tUHFman is one greater than the packet number from 
				// the last received data packet from tUHFman (Brian)
     			if( (TMcurrpnum & 0x0F) != ((TMlastpnum & 0x0F)+1) ) 
     			{
     				ERROR_FLAG = YES;                             
				}
     			else if( ACKRECD == YES )
     			{	
					PACKET_STATUS[CURRENT_PACKET] = EMPTY;
					CURRENT_PACKET++;
					ACKRECD == NO;
				}
				
				PACKET_RECD = YES;
				ACK_TO_SEND = YES;
				CRCack = 0;
				
				ACKbuffer[0] = TM_TT;
				ACKbuffer[1] = 0x05;
				ACKbuffer[2] = 0x80 | (TMcurrpnum & 0x0F);

				calcCRC( CRCack, ACKbuffer[0] ); 
				calcCRC( CRCack, ACKbuffer[1] ); 
				calcCRC( CRCack, ACKbuffer[2] ); 
			
				ACKbuffer[3] = (unsigned int)CRCack & 0xFF;
				ACKbuffer[4] = (unsigned int)CRCack >> 8; 
				
				if( (ERROR_FLAG == NO) && (ACKRECD == YES) )
					TMlastpnum = TMcurrpnum;
			}
		}
   		_asm	JMP			TestForAnotherPacket
	}
	
    _asm
    {
    	TestForAnotherPacket:
		MOV				i,0000H
			
		BegLoop3:
		INC				i
		CMP             i,TIMEOUT
		JE				NoDataRecd3	

		MOV             DX,IIR					;load received char
		IN              AL,DX
		AND             AL,07H              	;mask bits 3-7

		CMP             AL,06H              	;Test 1: Framing, Overrun Errors/BREAK
		JNE             NoError3 
		MOV             DX,LSR              
		IN              AL,DX					;clear LSR
		MOV             DX,RBR_THR
		IN              AL,DX					;clear RBR_THR
		MOV             DX,MSR
		IN              AL,DX					;clear MSR
		JMP 			BegLoop3
		
		NoDataRecd3:	
	}
	//ERROR_FLAG = YES; 
 	_asm 
	{
		JMP				End
		
		NoError3:
		CMP             AL,04H              	;Test 2: Data Received
		JNE				BegLoop3
		MOV             DX,RBR_THR				;load received char
		IN              AL,DX
		MOV             DestSrc,AL     			;copy recd char to rcvByte  
	}
    
   	if( DestSrc == TT_TM )
   	{
		CRCresult = 0;
   	    Rbuffer[rcvCount++] = DestSrc;
       	calcCRC( CRCresult, DestSrc );
   		_asm	JMP			PacketLengthCode
   	} 
   	else if( DestSrc == 0xF0 )
   		_asm	JMP			End	
	else
  		_asm	JMP			PacketLengthCode	
                         
	_asm SendDataSection:
	// First make sure the LSR is empty, then send 5 FF's:
	_asm    
	{
		ACK_Poll_TRegister1:
		MOV				DX,LSR
		IN				AL,DX
		AND				AL,20H
		CMP				AL,20H
		JNE				ACK_Poll_TRegister1
	} 
	for (unsigned int m=0; m<5; m++)
	{	
		_asm
		{
			MOV			AL,0xFF
			MOV			DX,RBR_THR 
			OUT			DX,AL

			ACK_Poll_TRegister2:				;Make sure LSR is empty
			MOV				DX,LSR
			IN				AL,DX
			AND				AL,20H
			CMP				AL,20H
			JNE				ACK_Poll_TRegister2
		}
	}
    
	// Check if there is ACK data to send:
	if( ACK_TO_SEND == YES )
	{ 
		ACK_TO_SEND = NO;   
		for (unsigned int n=0; n<5; n++)
		{	
			sndByte = ACKbuffer[n];
			_asm
			{
				MOV			AL,sndByte
				MOV			DX,RBR_THR 
				OUT			DX,AL

				ACK_Poll_TRegister3:			;First make sure LSR is empty
				MOV				DX,LSR
				IN				AL,DX
				AND				AL,20H
				CMP				AL,20H
				JNE				ACK_Poll_TRegister3
			}
		}
	}
		
	if( PACKET_STATUS[CURRENT_PACKET] == RESEND_PACKET )
	{	
		PACKET_STATUS[CURRENT_PACKET] = AWAITING_ACK;
	
		// set packet number in state byte:
		if( ACKRECD == YES )
		{               
			// set packet number in state byte:
		    if( (Sbuffer[CURRENT_PACKET][2] & 0x40) == 0x00) // CONN_ESTAB_OFF 
	    	{
				if( TTlastpnum != 15 )
					TTcurrpnum++;
				else
					TTcurrpnum = 0;

	        	Sbuffer[CURRENT_PACKET][2] = 
	        		(Sbuffer[CURRENT_PACKET][2] & 0xF0) | TTcurrpnum;
        	}
		}

		for (unsigned int j=0; j<sndCount[CURRENT_PACKET]; j++)
		{
			sndByte = Sbuffer[CURRENT_PACKET][j];
			calcCRC( CRCdata, sndByte ); 
			_asm
			{
				MOV			AL,sndByte
				MOV			DX,RBR_THR 
				OUT			DX,AL
 		
				Poll_Transmit_Register1: 
				MOV				DX,LSR
				IN				AL,DX
				AND				AL,20H
				CMP				AL,20H
				JNE				Poll_Transmit_Register1
			}
		}
		
		CRC1 = (unsigned int)CRCdata & 0xFF;
		CRC2 = (unsigned int)CRCdata >> 8;
		CRCdata = 0x00;
		_asm
		{
			MOV				AL,CRC1
			MOV				DX,RBR_THR 
			OUT				DX,AL
 		
			Poll_Transmit_RegisterCRC1: 
			MOV				DX,LSR
			IN				AL,DX
			AND				AL,20H
			CMP				AL,20H
			JNE				Poll_Transmit_RegisterCRC1


			MOV				AL,CRC2
			MOV				DX,RBR_THR 
			OUT				DX,AL
 		
			Poll_Transmit_RegisterCRC2: 
			MOV				DX,LSR
			IN				AL,DX
			AND				AL,20H
			CMP				AL,20H
			JNE				Poll_Transmit_RegisterCRC2
		}
		ACKRECD = NO;
	}                                              

	else if( PACKET_STATUS[CURRENT_PACKET] == AWAITING_SEND )
	{	
		PACKET_STATUS[CURRENT_PACKET] = AWAITING_ACK;
	
		// set packet number in state byte:
	    if( (Sbuffer[CURRENT_PACKET][2] & 0x40) == 0x00) // CONN_ESTAB_OFF 
	    {
			if( TTlastpnum != 15 )
				TTcurrpnum++;
			else
				TTcurrpnum = 0;

	        Sbuffer[CURRENT_PACKET][2] = 
	        		(Sbuffer[CURRENT_PACKET][2] & 0xF0) | TTcurrpnum;
        }
        
		for (unsigned int j=0; j<sndCount[CURRENT_PACKET]; j++)
		{
			sndByte = Sbuffer[CURRENT_PACKET][j];
			calcCRC( CRCdata, sndByte ); 
			_asm
			{
				MOV			AL,sndByte
				MOV			DX,RBR_THR 
				OUT			DX,AL
 		
				Poll_Transmit_Register2: 
				MOV				DX,LSR
				IN				AL,DX
				AND				AL,20H
				CMP				AL,20H
				JNE				Poll_Transmit_Register2
			}
		}                                             
		
		CRC1 = (unsigned int)CRCdata & 0xFF;
		CRC2 = (unsigned int)CRCdata >> 8; 
		CRCdata = 0x00;
		_asm
		{
			MOV				AL,CRC1
			MOV				DX,RBR_THR 
			OUT				DX,AL
 		
			Poll_Transmit_RegisterCRC3: 
			MOV				DX,LSR
			IN				AL,DX
			AND				AL,20H
			CMP				AL,20H
			JNE				Poll_Transmit_RegisterCRC3


			MOV				AL,CRC2
			MOV				DX,RBR_THR 
			OUT				DX,AL
 		
			Poll_Transmit_RegisterCRC4: 
			MOV				DX,LSR
			IN				AL,DX
			AND				AL,20H
			CMP				AL,20H
			JNE				Poll_Transmit_RegisterCRC4
		}
		ACKRECD = NO;
	}                                              
	
	// Send ending bytes: F0, FF, FF and clear LSR:		
	_asm
	{
		MOV				AL,0xF0
		MOV				DX,RBR_THR 
		OUT				DX,AL
		Poll_Transmit_Register3: 
		MOV				DX,LSR
		IN				AL,DX
		AND				AL,20H
		CMP				AL,20H
		JNE				Poll_Transmit_Register3

		MOV				AL,0xFF
		MOV				DX,RBR_THR 
		OUT				DX,AL
		Poll_Transmit_Register4: 
		MOV				DX,LSR
		IN				AL,DX
		AND				AL,20H
		CMP				AL,20H
		JNE				Poll_Transmit_Register4

		MOV				AL,0xFF
		MOV				DX,RBR_THR 
		OUT				DX,AL
 		Poll_Transmit_Register5:				 
		MOV				DX,LSR
		IN				AL,DX
		AND				AL,20H
		CMP				AL,20H
		JNE				Poll_Transmit_Register5

  		Poll_Transmit_Register6:
		MOV				DX,LSR
		IN				AL,DX
		AND				AL,60H
		CMP				AL,60H
		JNE				Poll_Transmit_Register6
		JMP				End
	}


    _asm
    {
		BogusInt:                           	;isource unrecognizable
		MOV             DX,RBR_THR 
		IN              AL,DX
		OUT             DX,AL
		MOV             DX,LSR
		IN              AL,DX
		MOV             DX,MSR
		IN              AL,DX

		End:
		MOV             AL,20H              	;OK to clear current interrupt
		OUT             20H,AL              	;write 20H to 20H

        IN        		AL,21H					;unmask bit int PIC's IMR
        AND       		AL,PIC_ENABLE			;enable IRQ4/COM1: EF  (IRQ3/COM2: F7)
        OUT       		21H,AL
        
		MOV 			AL,0FH		     		;assert GPO2 - enable INTRP line (MCR)
		MOV				DX,MCR					;enable UART interrupt to drive IRQ.
		OUT				DX,AL

		STI										;enable interrupts
	}   
}


void
InstallInterruptHandler()
{
    _disable();
    
    _asm
    {
        MOV             DX,RBR_THR
        IN              AL,DX
        AND             AL,00H
        CMP             AL,00H
    }
     
    oldUARTvector = _dos_getvect(VEC_ADDR);	 
    _dos_setvect(VEC_ADDR, UARThandler);

    _asm
    {
        MOV       AL,80H						;set DLAB
        MOV       DX,LCR
        OUT       DX,AL

        MOV       AL,01H						;set baud rate to 115200
        MOV       DX,RBR_THR    
        OUT       DX,AL

        SUB       AL,AL							;clear transmitter register
        MOV       DX,IER
        OUT       DX,AL

        MOV       AL,03H						;clear DLAB, set 8n1
        MOV       DX,LCR
        OUT       DX,AL

        MOV       DX,MCR						;clear loopback
        XOR       AX,AX
        OUT       DX,AL

        MOV       AL,05H						;enable receive and line status INTs
        MOV       DX,IER 
        OUT       DX,AL

        MOV       AL,00H						;disable FIFOs in case of 16550
        MOV       DX,IIR
        OUT       DX,AL

        IN        AL,21H						;unmask bit int PIC's IMR
        AND       AL,PIC_ENABLE					;enable IRQ4/COM1: EF  (IRQ3/COM2: F7)
        OUT       21H,AL

        MOV       DX,MCR 
        IN        AL,DX							;assert GPO2 - enable INTRP line (MCR)
        OR        AL,08H						;enable UART interrupt to drive IRQ. 
        OUT       DX,AL
    }
    _enable();
}
