/*
 * ReadCommandType.h
 *
 *  Created on: Apr 12, 2018
 *      Author: gstoddard
 */

#ifndef SRC_READCOMMANDTYPE_H_
#define SRC_READCOMMANDTYPE_H_

#include <stdio.h>		//needed for unsigned types
#include "xuartps.h"	//needed for uart functions

//global variables
char commandBuffer[20] = "";
char commandBuffer2[50] = "";
int i_SprintfReturn = 0;
int ibytesSent = 0;

//used by LApp
int iPollBufferIndex;

int ReadCommandType(char * RecvBuffer, XUartPs *Uart_PS);
int PollUart(char * RecvBuffer, XUartPs *Uart_PS);

#endif /* SRC_READCOMMANDTYPE_H_ */
