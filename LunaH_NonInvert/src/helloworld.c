/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include "main.h"


int main()
{
    init_platform();

    // Initialize System
	init_platform();  		// This initializes the platform, which is ...
	ps7_post_config();
	Xil_DCacheDisable();	// Disable the data cache
	InitializeAXIDma();		// Initialize the AXI DMA Transfer Interface
	InitializeInterruptSystem(XPAR_PS7_SCUGIC_0_DEVICE_ID);

	// *********** Setup the Hardware Reset GPIO ****************//
	// GPIO/TEC Test Variables
	XGpioPs Gpio;
	int gpio_status = 0;
	XGpioPs_Config *GPIOConfigPtr;

	GPIOConfigPtr = XGpioPs_LookupConfig(XPAR_PS7_GPIO_0_DEVICE_ID);
	gpio_status = XGpioPs_CfgInitialize(&Gpio, GPIOConfigPtr, GPIOConfigPtr->BaseAddr);
	if(gpio_status != XST_SUCCESS)
		xil_printf("GPIO PS init failed\r\n");

	XGpioPs_SetDirectionPin(&Gpio, TEC_PIN, 1);
	XGpioPs_SetOutputEnablePin(&Gpio, TEC_PIN, 1);
	XGpioPs_WritePin(&Gpio, TEC_PIN, 0);	//disable TEC startup

	XGpioPs_SetDirectionPin(&Gpio, SW_BREAK_GPIO, 1);
	//******************Setup and Initialize IIC*********************//
	//IIC0 = sensor head
	//IIC1 = thermometer
	XIicPs Iic;
	//*******************Receive and Process Packets **********************//
	Xil_Out32 (XPAR_AXI_GPIO_0_BASEADDR, 0);	//baseline integration time	//subtract 38 from each int
	Xil_Out32 (XPAR_AXI_GPIO_1_BASEADDR, 35);	//short
	Xil_Out32 (XPAR_AXI_GPIO_2_BASEADDR, 131);	//long
	Xil_Out32 (XPAR_AXI_GPIO_3_BASEADDR, 1513);	//full
	Xil_Out32 (XPAR_AXI_GPIO_4_BASEADDR, 0);	//TEC stuff, 0 turns things off
	Xil_Out32 (XPAR_AXI_GPIO_5_BASEADDR, 0);	//TEC stuff
	Xil_Out32 (XPAR_AXI_GPIO_6_BASEADDR, 0);	//enable the system, allows data
	Xil_Out32 (XPAR_AXI_GPIO_7_BASEADDR, 0);	//enable 5V to sensor head
	Xil_Out32 (XPAR_AXI_GPIO_10_BASEADDR, 8500);	//threshold, max of 2^14 (16384)
	Xil_Out32 (XPAR_AXI_GPIO_16_BASEADDR, 16384);	//master-slave frame size
	Xil_Out32 (XPAR_AXI_GPIO_17_BASEADDR, 1);	//master-slave enable
	Xil_Out32(XPAR_AXI_GPIO_18_BASEADDR, 0);	//capture module enable

	//*******************Setup the UART **********************//
	int Status = 0;
//	XUartPs Uart_PS;

	XUartPs_Config *Config = XUartPs_LookupConfig(UART_DEVICEID);
	if (NULL == Config) { return 1;}
	Status = XUartPs_CfgInitialize(&Uart_PS, Config, Config->BaseAddress);
	if (Status != 0){ xil_printf("XUartPS did not CfgInit properly.\n");	}

	/* Conduct a Selftest for the UART */
	Status = XUartPs_SelfTest(&Uart_PS);
	if (Status != 0) { xil_printf("XUartPS failed self test.\n"); }			//handle error checks here better

	/* Set to normal mode. */
	XUartPs_SetOperMode(&Uart_PS, XUARTPS_OPER_MODE_NORMAL);

	// *********** Mount SD Card and Initialize Variables ****************//
	if( doMount == 0 ){			//Initialize the SD card here
		ffs_res = f_mount(NULL,"0:/",0);
		ffs_res = f_mount(&fatfs[0], "0:/",0);
		ffs_res = f_mount(NULL,"1:/",0);
		ffs_res = f_mount(&fatfs[1],"1:/",0);
		doMount = 1;
	}
	if( f_stat( cLogFile, &fno) ){	// f_stat returns non-zero(false) if no file exists, so open/create the file
		ffs_res = f_open(&logFile, cLogFile, FA_WRITE|FA_OPEN_ALWAYS);
		ffs_res = f_write(&logFile, cZeroBuffer, 10, &numBytesWritten);
		filptr_clogFile += numBytesWritten;		// Protect the first xx number of bytes to use as flags, in this case xx must be 10
		ffs_res = f_close(&logFile);
	}
	else // If the file exists, read it
	{
		ffs_res = f_open(&logFile, cLogFile, FA_READ|FA_WRITE);	//open with read/write access
		ffs_res = f_lseek(&logFile, 0);							//go to beginning of file
		ffs_res = f_read(&logFile, &filptr_buffer, 10, &numBytesRead);	//Read the first 10 bytes to determine flags and the size of the write pointer
		sscanf(filptr_buffer, "%d", &filptr_clogFile);			//take the write pointer from char -> integer so we may use it
		ffs_res = f_lseek(&logFile, filptr_clogFile);			//move the write pointer so we don't overwrite info
		iSprintfReturn = snprintf(cWriteToLogFile, LOG_FILE_BUFF_SIZE, "POWER RESET %f ", dTime);	//write that the system was power cycled
		ffs_res = f_write(&logFile, cWriteToLogFile, iSprintfReturn, &numBytesWritten);	//write to the file
		filptr_clogFile += numBytesWritten;						//update the write pointer
		ffs_res = f_close(&logFile);							//close the file
	}

	if( f_stat(cDirectoryLogFile0, &fnoDIR) )	//check if the file exists
	{
		ffs_res = f_open(&directoryLogFile, cDirectoryLogFile0, FA_WRITE|FA_OPEN_ALWAYS);	//if no, create the file
		ffs_res = f_write(&directoryLogFile, cZeroBuffer, 10, &numBytesWritten);			//write the zero buffer so we can keep track of the write pointer
		filptr_cDIRFile += 10;																//move the write pointer
		ffs_res = f_write(&directoryLogFile, cLogFile, 11, &numBytesWritten);				//write the name of the log file because it was created above
		filptr_cDIRFile += numBytesWritten;													//update the write pointer
		snprintf(cWriteToLogFile, 10, "%d", filptr_cDIRFile);								//write formatted output to a sized buffer; create a string of a certain length
		ffs_res = f_lseek(&directoryLogFile, (10 - LNumDigits(filptr_cDIRFile)));			// Move to the start of the file
		ffs_res = f_write(&directoryLogFile, cWriteToLogFile, LNumDigits(filptr_cDIRFile), &numBytesWritten);	//Record the new file pointer
		ffs_res = f_close(&directoryLogFile);												//close the file
	}
	else	//if the file exists, read it
	{
		ffs_res = f_open(&directoryLogFile, cDirectoryLogFile0, FA_READ);					//open the file
		ffs_res = f_lseek(&directoryLogFile, 0);											//move to the beginning of the file
		ffs_res = f_read(&directoryLogFile, &filptr_cDIRFile_buffer, 10, &numBytesWritten);	//read the write pointer
		sscanf(filptr_cDIRFile_buffer, "%d", &filptr_cDIRFile);								//write the pointer to the relevant variable
		ffs_res = f_close(&directoryLogFile);												//close the file
	}
	// *********** Mount SD Card and Initialize Variables ****************//

	//start timing
	XTime local_time_start = 0;
	XTime local_time_current = 0;
	XTime local_time = 0;
	XTime_GetTime(&local_time_start);	//get the time

	// Initialize buffers
	char filename_buff[50] = "";
	char full_filename_buff[54] = "";
	char report_buff[100] = "";
	char transfer_file_contents[DATA_PACKET_SIZE] = "";
	unsigned char error_buff[] = "FFFFFF\n";
	int ipollReturn = 0;
	int status = 0;
	int iterator = 0;
	int sent = 0;
	int bytes_sent = 0;
	int bytes_recvd = 0;
	int file_size = 0;
	int checksum1 = 0;
	int checksum2 = 0;
	int i_neutron_total = 0;
	unsigned int sync_marker = 0x352EF853;

	// ******************* POLLING LOOP *******************//
	xil_printf("Meg is the supreme overlord of all the land and it's beings\n\r");

	while(1){
		memset(RecvBuffer, '0', 32);	// Clear RecvBuffer Variable
		XUartPs_SetOptions(&Uart_PS, XUARTPS_OPTION_RESET_RX);
		iPollBufferIndex = 0;

		xil_printf("\r\n Devkit version 2.50 \r\n");
		xil_printf("BOOT MENU \r\n");
		xil_printf("*****\n\r");
		xil_printf(" 0) Set Mode of Operation\n\r");
		xil_printf(" 1) Enable or disable the system\n\r");
		xil_printf(" 2) Continuously Read of Processed Data\n\r");
		xil_printf("\n\r **Setup Parameters ** \n\r");
		xil_printf(" 3) Set Trigger Threshold\n\r");
		xil_printf(" 4) Set Integration Times (number of clock cycles * 4ns) \n\r");
		xil_printf("\n\r ** SD Card Commands ** \n\r");
		xil_printf(" 5) Check SD 0 \n\r");
		xil_printf(" 6) Check SD 1\n\r");
		xil_printf(" 7) TX file from the SD card (use the GUI)\n\r");
		xil_printf(" 8) Delete file from the SD card\n\r");
		xil_printf("\n\r ** TEC Board Check Commands ** \n\r");
		xil_printf(" 9) TEC Bleed Enable\n\r");
		xil_printf("10) TEC Bleed Disable\n\r");
		xil_printf("11) TEC Control Enable\n\r");
		xil_printf("\n\r ** Analog Board Check Commands ** \n\r");
		xil_printf("12) High Voltage Control\n\r");
		xil_printf("13) Read the temperature from Digital board\n\r");
		xil_printf("14) Read the temperature from Analog board\n\r");
		xil_printf("15) Read the temperature from Extra Temp Sensor Board\n\r");
		xil_printf("******\n\r");
		while(1)
		{
			ReadCommandPoll();
			menusel = 99999;
			sscanf(RecvBuffer,"%02u",&menusel);
			if ( menusel >= -1 && menusel <= 19 )
				break;

			//until we have found something useful, check to see if we need
			// to report SOH information, 1 Hz
			//the time below will not be reported by this configuration because
			// ReadCommandPoll loops internally instead of flowing through this
			// while loop, thus this code does not get reached
			XTime_GetTime(&local_time_current);
			if(((local_time_current - local_time_start)/COUNTS_PER_SECOND) >= (local_time +  1))
			{
				local_time = (local_time_current - local_time_start)/COUNTS_PER_SECOND;

				iSprintfReturn = snprintf(report_buff, 100, "%d_%d_%u_%u\n", 12, 34, i_neutron_total, (unsigned int)local_time);
				status = XUartPs_Send(&Uart_PS, (u8 *)report_buff, iSprintfReturn);
				xil_printf("sent %d bytes with Uart_send\r\n",status);
				i_neutron_total++;
			}
		}

		ipollReturn = 0;
		switch (menusel) { // Switch-Case Menu Select
		case -1:
			bytes_sent = XUartPs_Send(&Uart_PS, error_buff, sizeof(error_buff));
			break;
		case 0: //Set Mode of Operation
			mode = 99; //Detector->GetMode();
			xil_printf("\n\r Waveform Data: \t Enter 0 <return>\n\r");
			xil_printf(" LPF Waveform Data: \t Enter 1 <return>\n\r");
			xil_printf(" DFF Waveform Data: \t Enter 2 <return>\n\r");
			xil_printf(" TRG Waveform Data: \t Enter 3 <return>\n\r");
			xil_printf(" Processed Data: \t Enter 4 <return>\n\r");
			ReadCommandPoll();
			sscanf(RecvBuffer,"%01u",&mode);

			if (mode < 0 || mode > 4 ) { xil_printf("Invalid Command\n\r"); break; }
			Xil_Out32 (XPAR_AXI_GPIO_14_BASEADDR, ((u32)mode));
			// Register 14
			if ( mode == 0 ) { xil_printf("Transfer AA Waveforms\n\r"); }
			if ( mode == 1 ) { xil_printf("Transfer LPF Waveforms\n\r"); }
			if ( mode == 2 ) { xil_printf("Transfer DFF Waveforms\n\r"); }
			if ( mode == 3 ) { xil_printf("Transfer TRG Waveforms\n\r"); }
			if ( mode == 4 ) { xil_printf("Transfer Processed Data\n\r"); }
			sleep(1); 			// Built in Latency ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ 1 s

			dTime += 1;	//increment time for testing
			iSprintfReturn = snprintf(cWriteToLogFile, LOG_FILE_BUFF_SIZE, "Set mode %d %f ", mode, dTime);

			ffs_res = f_open(&logFile, cLogFile, FA_OPEN_ALWAYS | FA_WRITE);
			if(ffs_res){
				xil_printf("Could not open file %d", ffs_res);
				break;
			}
			ffs_res = f_lseek(&logFile, filptr_clogFile);
			ffs_res = f_write(&logFile, cWriteToLogFile, iSprintfReturn, &numBytesWritten);
			filptr_clogFile += numBytesWritten;
			snprintf(cWriteToLogFile, 10, "%d", filptr_clogFile);
			ffs_res = f_lseek(&logFile, (10 - LNumDigits(filptr_clogFile)));	// Move to the start of the file
			ffs_res = f_write(&logFile, cWriteToLogFile, LNumDigits(filptr_clogFile), &numBytesWritten);	//Record the new file pointer
			ffs_res = f_close(&logFile);
			break;

		case 1: //Enable or disable the system
			xil_printf("\n\r Disable: Enter 0 <return>\n\r");
			xil_printf(" Enable: Enter 1 <return>\n\r");
			ReadCommandPoll();
			sscanf(RecvBuffer,"%01u",&enable_state);
			if (enable_state != 0 && enable_state != 1) { xil_printf("Invalid Command\n\r"); break; }
			Xil_Out32(XPAR_AXI_GPIO_18_BASEADDR, ((u32)enable_state));
			// Register 18 Out enabled, In Disabled
			if ( enable_state == 1 )
			{
				xil_printf("DAQ Enabled\n\r");
				Xil_Out32(XPAR_AXI_GPIO_6_BASEADDR, 1);
				Xil_Out32 (XPAR_AXI_GPIO_7_BASEADDR, 1);
			}
			if ( enable_state == 0 )
			{
				Xil_Out32(XPAR_AXI_GPIO_6_BASEADDR, 0);
				Xil_Out32 (XPAR_AXI_GPIO_7_BASEADDR, 0);
				xil_printf("DAQ Disabled\n\r"); }
			sleep(1); 			// Built in Latency ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ 1 s

			dTime += 1;
			iSprintfReturn = snprintf(cWriteToLogFile, LOG_FILE_BUFF_SIZE, "Enable system %d %f ", enable_state, dTime);

			ffs_res = f_open(&logFile, cLogFile, FA_OPEN_ALWAYS | FA_WRITE);
			if(ffs_res){
				xil_printf("Could not open file %d", ffs_res);
				break;
			}
			ffs_res = f_lseek(&logFile, filptr_clogFile);
			ffs_res = f_write(&logFile, cWriteToLogFile, iSprintfReturn, &numBytesWritten);
			filptr_clogFile += numBytesWritten;												// Add the number of bytes written to the file pointer
			snprintf(cWriteToLogFile, 10, "%d", filptr_clogFile);							// Write that to a string for saving
			ffs_res = f_lseek(&logFile, (10 - LNumDigits(filptr_clogFile)));				// Seek to the beginning of the file skipping the leading zeroes
			ffs_res = f_write(&logFile, cWriteToLogFile, LNumDigits(filptr_clogFile), &numBytesWritten); // Write the new file pointer
			ffs_res = f_close(&logFile);

			break;

		case 2: //Continuously Read of Processed Data

			dTime += 1;
			iSprintfReturn = snprintf(cWriteToLogFile, LOG_FILE_BUFF_SIZE, "Print data %d %f ", enable_state, dTime);

			ffs_res = f_open(&logFile, cLogFile, FA_OPEN_ALWAYS | FA_WRITE);
			if(ffs_res){
				xil_printf("Could not open file %d\n\r", ffs_res);
				break;
			}
			ffs_res = f_lseek(&logFile, filptr_clogFile);
			ffs_res = f_write(&logFile, cWriteToLogFile, iSprintfReturn, &numBytesWritten);
			filptr_clogFile += numBytesWritten;
			snprintf(cWriteToLogFile, 10, "%d", filptr_clogFile);							// Write that to a string for saving
			ffs_res = f_lseek(&logFile, (10 - LNumDigits(filptr_clogFile)));				// Seek to the beginning of the file skipping the leading zeroes
			ffs_res = f_write(&logFile, cWriteToLogFile, LNumDigits(filptr_clogFile), &numBytesWritten); // Write the new file pointer
			ffs_res = f_close(&logFile);

			xil_printf("\n\r ********Data Acquisition:\n\r");
			xil_printf(" Press 'q' to Stop or Press Hardware USR reset button  \n\r");
			xil_printf(" Press <return> to Start\n\r");
			ReadCommandPoll();	//Just get a '\n' to continue

			//read the time and save it to the data file
			XTime_GetTime(&local_time_current);
			//reuse the same FIL structure from before
			ffs_res = f_open(&logFile, "1:/data_test_001.bin", FA_WRITE|FA_READ|FA_OPEN_ALWAYS);	//open the file
			if(ffs_res)
				xil_printf("Could not open file %d\n", ffs_res);
			ffs_res = f_lseek(&logFile, file_size(&logFile));	//seek to the end of the file
			ffs_res = f_write(&logFile, &sync_marker, sizeof(unsigned int), &numBytesWritten);	//write the sync marker
			ffs_res = f_write(&logFile, &local_time_current, sizeof(XTime), &numBytesWritten);
			ffs_res = f_close(&logFile);
			//enable the system
			Xil_Out32(XPAR_AXI_GPIO_18_BASEADDR, 1);	//triggers a false event which gives
			Xil_Out32(XPAR_AXI_GPIO_6_BASEADDR, 1);		// us a time to sync with the above
			Xil_Out32 (XPAR_AXI_GPIO_7_BASEADDR, 1);	// logged time

			getWFDAQ(i_neutron_total);
			sw = 0;   // broke out of the read loop, stop switch reset to 0
			break;

		case 3: //Set Threshold
			iThreshold0 = Xil_In32(XPAR_AXI_GPIO_10_BASEADDR);
			xil_printf("\n\r Existing Threshold = %d \n\r",Xil_In32(XPAR_AXI_GPIO_10_BASEADDR));
			xil_printf(" Enter Threshold (6144 to 32769) <return> \n\r");
			ReadCommandPoll();
			sscanf(RecvBuffer,"%u",&iThreshold1);
			Xil_Out32(XPAR_AXI_GPIO_10_BASEADDR, ((u32)iThreshold1));
			xil_printf("New Threshold = %d \n\r",Xil_In32(XPAR_AXI_GPIO_10_BASEADDR));
			sleep(1); 			// Built in Latency ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ 1 s

			dTime += 1;
			iSprintfReturn = snprintf(cWriteToLogFile, LOG_FILE_BUFF_SIZE, "Set trigger threshold from %d to %d %f ", iThreshold0, iThreshold1, dTime);

			ffs_res = f_open(&logFile, cLogFile, FA_OPEN_ALWAYS | FA_WRITE);
			if(ffs_res){
				xil_printf("Could not open file %d", ffs_res);
				break;
			}
			ffs_res = f_lseek(&logFile, filptr_clogFile);
			ffs_res = f_write(&logFile, cWriteToLogFile, iSprintfReturn, &numBytesWritten);
			filptr_clogFile += numBytesWritten;
			snprintf(cWriteToLogFile, 10, "%d", filptr_clogFile);							// Write that to a string for saving
			ffs_res = f_lseek(&logFile, (10 - LNumDigits(filptr_clogFile)));				// Seek to the beginning of the file skipping the leading zeroes
			ffs_res = f_write(&logFile, cWriteToLogFile, LNumDigits(filptr_clogFile), &numBytesWritten); // Write the new file pointer
			ffs_res = f_close(&logFile);
			break;

		case 4: //Set Integration Times
//			setIntsArray[4] = ((Xil_In32(XPAR_AXI_GPIO_0_BASEADDR) - 1) * 4) - 200;	//40 is to account for arbitrary offset
//			setIntsArray[5] = ((Xil_In32(XPAR_AXI_GPIO_1_BASEADDR) - 1) * 4) - 200;
//			setIntsArray[6] = ((Xil_In32(XPAR_AXI_GPIO_2_BASEADDR) - 1) * 4) - 200;
//			setIntsArray[7] = ((Xil_In32(XPAR_AXI_GPIO_3_BASEADDR) - 1) * 4) - 200;
			setIntsArray[4] = -52 + ((int)Xil_In32(XPAR_AXI_GPIO_0_BASEADDR))*4;
			setIntsArray[5] = -52 + ((int)Xil_In32(XPAR_AXI_GPIO_1_BASEADDR))*4;
			setIntsArray[6] = -52 + ((int)Xil_In32(XPAR_AXI_GPIO_2_BASEADDR))*4;
			setIntsArray[7] = -52 + ((int)Xil_In32(XPAR_AXI_GPIO_3_BASEADDR))*4;

			xil_printf("\n\r Existing Integration Times \n\r");
			xil_printf(" Time = 0 ns is when the Pulse Crossed Threshold \n\r");
			xil_printf(" Baseline Integral Window \t [-200ns,%dns] \n\r",setIntsArray[4]);
			xil_printf(" Short Integral Window \t [-200ns,%dns] \n\r",setIntsArray[5]);
			xil_printf(" Long Integral Window  \t [-200ns,%dns] \n\r",setIntsArray[6]);
			xil_printf(" Full Integral Window  \t [-200ns,%dns] \n\r",setIntsArray[7]);
			xil_printf(" Change: (Y)es (N)o <return>\n\r");
			ReadCommandPoll();
			sscanf(RecvBuffer,"%c",&updateint);

			if (updateint == 'N' || updateint == 'n') {
				dTime += 1;
				iSprintfReturn = snprintf(cWriteToLogFile, LOG_FILE_BUFF_SIZE, "No change to integration times of %d %d %d %d %f ", setIntsArray[4], setIntsArray[5], setIntsArray[6], setIntsArray[7], dTime);
				ffs_res = f_open(&logFile, cLogFile, FA_OPEN_ALWAYS | FA_WRITE);
				if(ffs_res){
					xil_printf("Could not open file %d", ffs_res);
					break;
				}
				ffs_res = f_lseek(&logFile, filptr_clogFile);
				ffs_res = f_write(&logFile, cWriteToLogFile, iSprintfReturn, &numBytesWritten);
				filptr_clogFile += numBytesWritten;
				snprintf(cWriteToLogFile, 10, "%d", filptr_clogFile);							// Write that to a string for saving
				ffs_res = f_lseek(&logFile, (10 - LNumDigits(filptr_clogFile)));				// Seek to the beginning of the file skipping the leading zeroes
				ffs_res = f_write(&logFile, cWriteToLogFile, LNumDigits(filptr_clogFile), &numBytesWritten); // Write the new file pointer
				ffs_res = f_close(&logFile);
				break;
			}

			if (updateint == 'Y' || updateint == 'y') {
				SetIntegrationTimes(&setIntsArray, &setSamples);
				dTime += 1;
				iSprintfReturn = snprintf(cWriteToLogFile, LOG_FILE_BUFF_SIZE, "Set integration times to %d %d %d %d from %d %d %d %d %f ",
						setIntsArray[0], setIntsArray[1], setIntsArray[2], setIntsArray[3], setIntsArray[4], setIntsArray[5], setIntsArray[6], setIntsArray[7], dTime);
				ffs_res = f_open(&logFile, cLogFile, FA_OPEN_ALWAYS | FA_WRITE);
				if(ffs_res){
					xil_printf("Could not open file %d", ffs_res);
					break;
				}
				ffs_res = f_lseek(&logFile, filptr_clogFile);
				ffs_res = f_write(&logFile, cWriteToLogFile, iSprintfReturn, &numBytesWritten);
				filptr_clogFile += numBytesWritten;
				snprintf(cWriteToLogFile, 10, "%d", filptr_clogFile);							// Write that to a string for saving
				ffs_res = f_lseek(&logFile, (10 - LNumDigits(filptr_clogFile)));				// Seek to the beginning of the file skipping the leading zeroes
				ffs_res = f_write(&logFile, cWriteToLogFile, LNumDigits(filptr_clogFile), &numBytesWritten); // Write the new file pointer
				ffs_res = f_close(&logFile);

			}

			sleep(1); 			// Built in Latency ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ 1 s
			break;

		case 5: //write to SD0 //open, close, open, read, write, close, create, write, close
			//re-mount the drive; this re-initializes the filesystem
			//ffs_res == 0 --> FR_OK is the return value of each FAT File System function
			ffs_res = f_mount(&fatfs[0], "0:/",0);
			if(ffs_res == 0)
				xil_printf("Re-mount succeeded\r\n");
			else
			{
				xil_printf("Re-mount failed\r\n");
				break;
			}

			xil_printf("\r\n*** Testing binary file operations on SD Card 0 ***\r\n\r\n");
			ffs_res = f_open(&logFile, "0:/test_file_1.bin", FA_OPEN_ALWAYS |FA_READ| FA_WRITE);
			if(ffs_res == 0)
				xil_printf("Open succeeded\r\n");
			else
			{
				xil_printf("Open failed\r\n");
				break;
			}
			ffs_res = f_close(&logFile);
			if(ffs_res == 0)
				xil_printf("Close succeeded\r\n");
			else
			{
				xil_printf("Close failed\r\n");
				break;
			}

			ffs_res = f_open(&logFile, "0:/test_file_1.bin", FA_OPEN_ALWAYS |FA_READ| FA_WRITE);
			snprintf(cWriteToLogFile, 10, "%d", 123456);
			ffs_res = f_write(&logFile, cWriteToLogFile, 4, &numBytesWritten);
			if(ffs_res == 0)
			{
				xil_printf("Write succeeded\r\n");
				xil_printf("%d bytes written to file\r\n", numBytesWritten);
			}
			else
			{
				xil_printf("Write failed\r\n");
				break;
			}
			ffs_res = f_lseek(&logFile, 0);
			ffs_res = f_read(&logFile, &c_read_from_log_file, 4, &numBytesRead);
			if(ffs_res == 0)
			{
				xil_printf("Read succeeded\r\n");
				xil_printf("%d bytes read from file\r\n", numBytesRead);
			}
			else
			{
				xil_printf("Read failed\r\n");
				break;
			}
			ffs_res = f_close(&logFile);

			ffs_res = f_open(&logFile, "0:/test_file_create.bin", FA_OPEN_ALWAYS |FA_READ| FA_WRITE);
			if(ffs_res == 0)
				xil_printf("File create succeeded\r\n");
			else
			{
				xil_printf("File create failed\r\n");
				break;
			}
			ffs_res = f_write(&logFile, cWriteToLogFile, 4, &numBytesWritten); // Write the new file pointer
			if(ffs_res == 0)
				xil_printf("Write to created file succeeded\r\n");
			else
			{
				xil_printf("Write to created file failed\r\n");
				break;
			}
			ffs_res = f_close(&logFile);

			//if we made it here without breaking out, then binary file operations are good
			xil_printf("\r\n*** Binary SD Card operations on Card 0 are: ***\r\n*** GOOD ***\r\n");

			//do it for a text file
			xil_printf("\r\n*** Testing text file operations on SD Card 0 ***\r\n\r\n");
			ffs_res = f_open(&logFile, "0:/test_file_1.txt", FA_OPEN_ALWAYS |FA_READ| FA_WRITE);
			if(ffs_res == 0)
				xil_printf("Open succeeded\r\n");
			else
			{
				xil_printf("Open failed\r\n");
				break;
			}
			ffs_res = f_close(&logFile);
			if(ffs_res == 0)
				xil_printf("Close succeeded\r\n");
			else
			{
				xil_printf("Close failed\r\n");
				break;
			}

			ffs_res = f_open(&logFile, "0:/test_file_1.txt", FA_OPEN_ALWAYS |FA_READ| FA_WRITE);
			ffs_res = f_write(&logFile, cWriteToLogFile, 4, &numBytesWritten);
			if(ffs_res == 0)
			{
				xil_printf("Write succeeded\r\n");
				xil_printf("%d bytes written to file\r\n", numBytesWritten);
			}
			else
			{
				xil_printf("Write failed\r\n");
				break;
			}
			ffs_res = f_lseek(&logFile, 0);
			ffs_res = f_read(&logFile, &c_read_from_log_file, 4, &numBytesRead);
			if(ffs_res == 0)
			{
				xil_printf("Read succeeded\r\n");
				xil_printf("%d bytes read from file\r\n", numBytesRead);
			}
			else
			{
				xil_printf("Read failed\r\n");
				break;
			}
			ffs_res = f_close(&logFile);

			ffs_res = f_open(&logFile, "0:/test_file_create.txt", FA_OPEN_ALWAYS |FA_READ| FA_WRITE);
			if(ffs_res == 0)
				xil_printf("File create succeeded\r\n");
			else
			{
				xil_printf("File create failed\r\n");
				break;
			}
			ffs_res = f_write(&logFile, cWriteToLogFile, 4, &numBytesWritten); // Write the new file pointer
			if(ffs_res == 0)
				xil_printf("Write to created file succeeded\r\n");
			else
			{
				xil_printf("Write to created file failed\r\n");
				break;
			}
			ffs_res = f_close(&logFile);

			//if we made it here without breaking out, then binary file operations are good
			xil_printf("\r\n*** Text SD Card operations on Card 0 are: ***\r\n*** GOOD ***\r\n");

			sleep(1); //built in latency
			break;
		case 6: //write to SD1
			//re-mount the drive; this re-initializes the filesystem
			//ffs_res == 0 --> FR_OK is the return value of each FAT File System function
			ffs_res = f_mount(&fatfs[1],"1:/",0);
			if(ffs_res == 0)
				xil_printf("Re-mount succeeded\r\n");
			else
			{
				xil_printf("Re-mount failed\r\n");
				break;
			}

			xil_printf("\r\n*** Testing binary file operations on SD Card 1 ***\r\n\r\n");
			ffs_res = f_open(&logFile, "1:/test_file_1.bin", FA_OPEN_ALWAYS |FA_READ| FA_WRITE);
			if(ffs_res == 0)
				xil_printf("Open succeeded\r\n");
			else
			{
				xil_printf("Open failed\r\n");
				break;
			}
			ffs_res = f_close(&logFile);
			if(ffs_res == 0)
				xil_printf("Close succeeded\r\n");
			else
			{
				xil_printf("Close failed\r\n");
				break;
			}

			ffs_res = f_open(&logFile, "1:/test_file_1.bin", FA_OPEN_ALWAYS |FA_READ| FA_WRITE);
			snprintf(cWriteToLogFile, 10, "%d", 123456);
			ffs_res = f_write(&logFile, cWriteToLogFile, 4, &numBytesWritten);
			if(ffs_res == 0)
			{
				xil_printf("Write succeeded\r\n");
				xil_printf("%d bytes written to file\r\n", numBytesWritten);
			}
			else
			{
				xil_printf("Write failed\r\n");
				break;
			}
			ffs_res = f_lseek(&logFile, 0);
			ffs_res = f_read(&logFile, &c_read_from_log_file, 4, &numBytesRead);
			if(ffs_res == 0)
			{
				xil_printf("Read succeeded\r\n");
				xil_printf("%d bytes read from file\r\n", numBytesRead);
			}
			else
			{
				xil_printf("Read failed\r\n");
				break;
			}
			ffs_res = f_close(&logFile);

			ffs_res = f_open(&logFile, "1:/test_file_create.bin", FA_OPEN_ALWAYS |FA_READ| FA_WRITE);
			if(ffs_res == 0)
				xil_printf("File create succeeded\r\n");
			else
			{
				xil_printf("File create failed\r\n");
				break;
			}
			ffs_res = f_write(&logFile, cWriteToLogFile, 4, &numBytesWritten); // Write the new file pointer
			if(ffs_res == 0)
				xil_printf("Write to created file succeeded\r\n");
			else
			{
				xil_printf("Write to created file failed\r\n");
				break;
			}
			ffs_res = f_close(&logFile);

			//if we made it here without breaking out, then binary file operations are good
			xil_printf("\r\n*** Binary SD Card operations on Card 1 are: ***\r\n*** GOOD ***\r\n");

			//do it for a text file
			xil_printf("\r\n*** Testing text file operations on SD Card 1 ***\r\n\r\n");
			ffs_res = f_open(&logFile, "1:/test_file_1.txt", FA_OPEN_ALWAYS |FA_READ| FA_WRITE);
			if(ffs_res == 0)
				xil_printf("Open succeeded\r\n");
			else
			{
				xil_printf("Open failed\r\n");
				break;
			}
			ffs_res = f_close(&logFile);
			if(ffs_res == 0)
				xil_printf("Close succeeded\r\n");
			else
			{
				xil_printf("Close failed\r\n");
				break;
			}

			ffs_res = f_open(&logFile, "1:/test_file_1.txt", FA_OPEN_ALWAYS |FA_READ| FA_WRITE);
			ffs_res = f_write(&logFile, cWriteToLogFile, 4, &numBytesWritten);
			if(ffs_res == 0)
			{
				xil_printf("Write succeeded\r\n");
				xil_printf("%d bytes written to file\r\n", numBytesWritten);
			}
			else
			{
				xil_printf("Write failed\r\n");
				break;
			}
			ffs_res = f_lseek(&logFile, 0);
			ffs_res = f_read(&logFile, &c_read_from_log_file, 4, &numBytesRead);
			if(ffs_res == 0)
			{
				xil_printf("Read succeeded\r\n");
				xil_printf("%d bytes read from file\r\n", numBytesRead);
			}
			else
			{
				xil_printf("Read failed\r\n");
				break;
			}
			ffs_res = f_close(&logFile);

			ffs_res = f_open(&logFile, "1:/test_file_create.txt", FA_OPEN_ALWAYS |FA_READ| FA_WRITE);
			if(ffs_res == 0)
				xil_printf("File create succeeded\r\n");
			else
			{
				xil_printf("File create failed\r\n");
				break;
			}
			ffs_res = f_write(&logFile, cWriteToLogFile, 4, &numBytesWritten); // Write the new file pointer
			if(ffs_res == 0)
				xil_printf("Write to created file succeeded\r\n");
			else
			{
				xil_printf("Write to created file failed\r\n");
				break;
			}
			ffs_res = f_close(&logFile);

			//if we made it here without breaking out, then binary file operations are good
			xil_printf("\r\n*** Text SD Card operations on Card 1 are: ***\r\n*** GOOD ***\r\n");

			sleep(1); //built in latency
			break;
		case 7:	//TX Files
			//use this case to send files from the SD card to the screen
			//just use the data_test_001.bin file to be sent atm
			//have the file name to TX, prepare it for sending
			ffs_res = f_open(&logFile, "1:/data_test_001.bin", FA_READ);	//open the file to transfer
			if(ffs_res != FR_OK)
			{
				iSprintfReturn = snprintf(report_buff, 100, "FFFFFF\n");
				bytes_sent = XUartPs_Send(&Uart_PS, (u8 *)report_buff, iSprintfReturn);
				break;
			}

			//init the CCSDS packet with some parts of the header
			memset(transfer_file_contents, 0, DATA_PACKET_SIZE);
			transfer_file_contents[0] = sync_marker >> 24; //sync marker
			transfer_file_contents[1] = sync_marker >> 16;
			transfer_file_contents[2] = sync_marker >> 8;
			transfer_file_contents[3] = sync_marker >> 0;
			transfer_file_contents[4] = 43;	//7:5 CCSDS version
			transfer_file_contents[5] = 42;	//APID lsb (0x200-0x3ff)
			transfer_file_contents[6] = 41;	//7:6 grouping flags//5:0 seq. count msb
			transfer_file_contents[7] = 0;	//seq. count lsb

			ffs_res = f_lseek(&logFile, 0);	//seek to the beginning of the file
			while(1)
			{
				//loop over the data in the file
				//read in one payload_max amount of data to our packet
				ffs_res = f_read(&logFile, (void *)&(transfer_file_contents[10]), PAYLOAD_MAX_SIZE, &numBytesRead);

				//packet size goes in byte 8-9
				//value is num bytes after CCSDS header minus one
				// so, data size + 2 checksums - 1 (for array indexing)
				file_size = numBytesRead + 2 - 1;
				transfer_file_contents[8] = file_size >> 8;
				transfer_file_contents[9] = file_size;

				//calculate simple and Fletcher checksums
				while(iterator < numBytesRead)
				{
					checksum1 = (checksum1 + transfer_file_contents[iterator + 10]) % 255;	//simple
					checksum2 = (checksum1 + checksum2) % 255;
					iterator++;
				}

				//save the checksums
				transfer_file_contents[file_size + 9] = checksum2;
				transfer_file_contents[file_size + 9 + 1] = checksum1;

				//we have filled up the packet header, send the data
				sent = 0;
				bytes_sent = 0;
				while(sent < (file_size + 11))
				{
					bytes_sent = XUartPs_Send(&Uart_PS, &(transfer_file_contents[0]) + sent, (file_size + 11) - sent);
					sent += bytes_sent;
				}

				if(numBytesRead < PAYLOAD_MAX_SIZE)
					break;
				else
					transfer_file_contents[7]++;
			} //end of while TX loop

			ffs_res = f_close(&logFile);
			break;
		case 8:
			//use this case to delete files from the SD card
			//just use the data_test_001.bin file to be deleted atm
			if(!f_stat("1:/data_test_001.bin", &fno))	//check if the file exists on disk
			{
				ffs_res = f_unlink("1:/data_test_001.bin");	//try to delete the file
				if(ffs_res == FR_OK)
				{
					//successful delete
					iSprintfReturn = snprintf(report_buff, 100, "%d_AAAA\n", ffs_res);
					bytes_sent = XUartPs_Send(&Uart_PS,(u8 *)report_buff, iSprintfReturn);
				}
				else
				{
					//failed to delete
					iSprintfReturn = snprintf(report_buff, 100, "FFFFFF\n");
					bytes_sent = XUartPs_Send(&Uart_PS, (u8 *)report_buff, iSprintfReturn);
				}
			}
			else
			{
				//file did not exist
				iSprintfReturn = snprintf(report_buff, 100, "FFFFFF\n");
				bytes_sent = XUartPs_Send(&Uart_PS, (u8 *)report_buff, iSprintfReturn);
			}
			sleep(1); //built in latency
			break;
		case 9: //TEC Bleed Enable
			Xil_Out32 (XPAR_AXI_GPIO_5_BASEADDR, 1);	//TEC Bleed Enable//signal to regulator
			XGpioPs_WritePin(&Gpio, TEC_PIN, 1);		//TEC Startup//signal to controller
			xil_printf("TEC ON, Bleed Enable ON\r\n");
			sleep(1); //built in latency
			break;
		case 10: //TEC Bleed Enable Cancel
			XGpioPs_WritePin(&Gpio, TEC_PIN, 0);		//TEC Startup disable
			Xil_Out32 (XPAR_AXI_GPIO_5_BASEADDR, 0);	//TEC Bleed disable
			xil_printf("TEC OFF, Bleed Enable OFF\r\n");
			sleep(1); //built in latency
			break;
		case 11: //TEC startup -> TEC control enable -> TEC off
			//change VTSET before getting in to temp loop
			Xil_Out32 (XPAR_AXI_GPIO_4_BASEADDR, 1);	//TEC Control Enable//signal to regulator
			XGpioPs_WritePin(&Gpio, TEC_PIN, 1);		//TEC Startup//signal to controller
			xil_printf("TEC ON, Control Enable ON\r\n");

			//poll for user value of a temperature to move to
			xil_printf("Enter a number of taps to move the wiper to:\r\n");
			ReadCommandPoll();
			data = 0;	//reset
			menusel = 99999;
			sscanf(RecvBuffer,"%d",&data);
			if( data < 0 || data > 255) { xil_printf("Invalid number of taps.\n\r"); sleep(1); break; }

			//poll for a time to wait for the temperature to settle
			xil_printf("Enter a timeout value to wait for, in seconds\r\n");
			ReadCommandPoll();
			timeout = 0;
			sscanf(RecvBuffer,"%d",&timeout);
			if( timeout < 1 ) { xil_printf("Invalid timeout value\n\r"); sleep(1); break; }

			IIC_SLAVE_ADDR=&IIC_SLAVE_ADDR4;
			cntrl = 0x2;	//write to RDAC
			i2c_Send_Buffer[0] = cntrl;
			i2c_Send_Buffer[1] = data;
			Status = IicPsMasterSend(IIC_DEVICE_ID_1, i2c_Send_Buffer, i2c_Recv_Buffer, IIC_SLAVE_ADDR);

			//VTSET has been changed, now loop while checking the temperature sensors
			// and user input to see if we should stop
			xil_printf("Press 'q' to stop\n\r");
			//set variables we'll need when looping
			IIC_SLAVE_ADDR=&IIC_SLAVE_ADDR5;
			i2c_Send_Buffer[0] = 0x0;
			i2c_Send_Buffer[1] = 0x0;
			//take the time so we know how long to run for
			XTime_GetTime(&local_time_current);	//take the time in cycles
			local_time = (local_time_current - local_time_start)/COUNTS_PER_SECOND; //compute the time in seconds
			while(1)
			{
				//check timeout to see if we should be done
				XTime_GetTime(&local_time_current);
				if(((local_time_current - local_time_start)/COUNTS_PER_SECOND) >= (local_time +  timeout))
					break;

				//check user input
				bytes_received = XUartPs_Recv(&Uart_PS, &RecvBuffer, 32);
				if ( RecvBuffer[0] == 'q' )
					break;
				else
					memset(RecvBuffer, '0', 32);

				//check temp sensor(s)
				//read the temperature from the analog board temp sensor, data sheet Extra Temp Sensor Board (0x4A)
				Status = IicPsMasterSend(IIC_DEVICE_ID_0, i2c_Send_Buffer, i2c_Recv_Buffer, IIC_SLAVE_ADDR);
				Status = IicPsMasterRecieve(IIC_DEVICE_ID_0, i2c_Recv_Buffer, IIC_SLAVE_ADDR);
				a = i2c_Recv_Buffer[0]<< 5;
				b = a | i2c_Recv_Buffer[1] >> 3;
				b /= 16;
				//check to see if the temp reported is the temp requested by the user
				//if it is, break out
				//check temp is not outside of acceptable range
				if(b > 50 || b < 0)
					break;
			}

			XGpioPs_WritePin(&Gpio, TEC_PIN, 0);		//TEC Startup disable
			Xil_Out32 (XPAR_AXI_GPIO_4_BASEADDR, 0);	//TEC Control disable
			xil_printf("TEC OFF, Control Enable OFF\n\r");
			sleep(1); //built in latency
			break;
		case 12: //HV and temp control
			xil_printf("************HIGH VOLTAGE CONTROL************\n\r");
			xil_printf(" 0) Write value to a potentiometer\n\r");
			xil_printf(" 1) Set all pots high\n\r");
			xil_printf(" 2) Set all pots low\n\r");
			xil_printf(" 3) Store value to EEPROM\n\r");

			ReadCommandPoll();
			menusel = 99999;
			sscanf(RecvBuffer,"%01u",&menusel);
			if( menusel < 0 || menusel > 3) { xil_printf("Invalid command.\n\r"); sleep(1); break; }

			switch(menusel){
			case 0:	//Write to RDAC1-4 to set value on digital pots on Analog board - see AD5144 datasheet, p26
				IIC_SLAVE_ADDR=&IIC_SLAVE_ADDR1;
				cntrl = 16;
				xil_printf(" Enter a value from 0-255 taps to write to the potentiometer  \n\r");
//				xil_printf(" 3.2V per tap with range of -1000 to -180\n\r");
				ReadCommandPoll();
				sscanf(RecvBuffer,"%u",&data);
				if(data < 0 || data > 255) { xil_printf("Invalid number of taps.\n\r"); sleep(1); break; }
				xil_printf(" Enter a number 1-4 to select a particular potentiometer to adjust\n\r");
				ReadCommandPoll();
				sscanf(RecvBuffer,"%01u",&rdac);
				if(rdac < 1 || rdac > 4) { xil_printf("Invalid pot selection.\n\r"); sleep(1); break; }

				switch(rdac){
				case 1:
					addr = 0x0;
					break;
				case 2:
					addr = 0x2;	//Note: on the analog board, pot 2 is routed to pot 3 and pot 3 is routed to pot 2
					break;		//thus they have their addresses switched here and in the next case
				case 3:
					addr = 0x1;	//This is to correct for a placement issue on the analog board
					break;
				case 4:
					addr = 0x3;
					break;
				default:
					xil_printf("Invalid. No changes made.");
					break;
				}

				i2c_Send_Buffer[0] = cntrl | addr;
				i2c_Send_Buffer[1] = data;
				Status = IicPsMasterSend(IIC_DEVICE_ID_0, i2c_Send_Buffer, i2c_Recv_Buffer, IIC_SLAVE_ADDR);
				//check the status
				break;
			case 1:	//Write to RDAC1-4 to set value on digital pots high (-1000 V)
				IIC_SLAVE_ADDR=&IIC_SLAVE_ADDR1;
				cntrl = 16;	//write command
				data = 255; //max voltage

				//change pot 1
				i2c_Send_Buffer[0] = cntrl | 0x0; //write to pot 1
				i2c_Send_Buffer[1] = data;
				Status = IicPsMasterSend(IIC_DEVICE_ID_0, i2c_Send_Buffer, i2c_Recv_Buffer, IIC_SLAVE_ADDR);

				//change pot 2
				i2c_Send_Buffer[0] = cntrl | 0x1; //write to pot 2
				i2c_Send_Buffer[1] = data;
				Status = IicPsMasterSend(IIC_DEVICE_ID_0, i2c_Send_Buffer, i2c_Recv_Buffer, IIC_SLAVE_ADDR);

				//change pot 3
				i2c_Send_Buffer[0] = cntrl | 0x2; //write to pot 3
				i2c_Send_Buffer[1] = data;
				Status = IicPsMasterSend(IIC_DEVICE_ID_0, i2c_Send_Buffer, i2c_Recv_Buffer, IIC_SLAVE_ADDR);

				//change pot 4
				i2c_Send_Buffer[0] = cntrl | 0x3; //write to pot 4
				i2c_Send_Buffer[1] = data;
				Status = IicPsMasterSend(IIC_DEVICE_ID_0, i2c_Send_Buffer, i2c_Recv_Buffer, IIC_SLAVE_ADDR);
				break;
			case 2:	//Write to RDAC1-4 to set value on digital pots low (-180 V)
				IIC_SLAVE_ADDR=&IIC_SLAVE_ADDR1;
				cntrl = 16;	//write command
				data = 0; //min voltage

				//change pot 1
				i2c_Send_Buffer[0] = cntrl | 0x0; //write to pot 1
				i2c_Send_Buffer[1] = data;
				Status = IicPsMasterSend(IIC_DEVICE_ID_0, i2c_Send_Buffer, i2c_Recv_Buffer, IIC_SLAVE_ADDR);

				//change pot 2
				i2c_Send_Buffer[0] = cntrl | 0x1; //write to pot 2
				i2c_Send_Buffer[1] = data;
				Status = IicPsMasterSend(IIC_DEVICE_ID_0, i2c_Send_Buffer, i2c_Recv_Buffer, IIC_SLAVE_ADDR);

				//change pot 3
				i2c_Send_Buffer[0] = cntrl | 0x2; //write to pot 3
				i2c_Send_Buffer[1] = data;
				Status = IicPsMasterSend(IIC_DEVICE_ID_0, i2c_Send_Buffer, i2c_Recv_Buffer, IIC_SLAVE_ADDR);

				//change pot 4
				i2c_Send_Buffer[0] = cntrl | 0x3; //write to pot 4
				i2c_Send_Buffer[1] = data;
				Status = IicPsMasterSend(IIC_DEVICE_ID_0, i2c_Send_Buffer, i2c_Recv_Buffer, IIC_SLAVE_ADDR);
				break;
			case 3:	//Write EEPROM
				IIC_SLAVE_ADDR=&IIC_SLAVE_ADDR1;
				cntrl = 0x7; //command 9 - Copy RDAC register to EEPROM - see AD5144 datasheet pg 20
				xil_printf("Select which potentiometer value to store in EEPROM (1-4)   \n\r");
				ReadCommandPoll();
				sscanf(RecvBuffer,"%01u",&rdac);
				if(rdac < 1 || rdac > 4) { xil_printf("Invalid pot selection.\n\r"); sleep(1); continue; }

				switch(rdac){
				case 1:
					addr = 0x0;
					break;
				case 2:
					addr = 0x2;	//see note above in Case 0 about why these addresses are switched
					break;
				case 3:
					addr = 0x1;
					break;
				case 4:
					addr = 0x3;
					break;
				default:
					xil_printf("Invalid. No changes made.");
					break;
				}

				i2c_Send_Buffer[0] = cntrl | addr;
				i2c_Send_Buffer[1] = 0x1;	// 7 x's followed by 1 -> 0x1
				Status = IicPsMasterSend(IIC_DEVICE_ID_0, i2c_Send_Buffer, i2c_Recv_Buffer, IIC_SLAVE_ADDR);
				break;
			default:
				i = 1;
				break;
			}//end of case statement
			sleep(1); //built in latency
			break;
		case 13:	//Read Temp on the digital board
			IIC_SLAVE_ADDR=&IIC_SLAVE_ADDR2;
			i2c_Send_Buffer[0] = 0x0;
			i2c_Send_Buffer[1] = 0x0;
			Status = IicPsMasterSend(IIC_DEVICE_ID_1, i2c_Send_Buffer, i2c_Recv_Buffer, IIC_SLAVE_ADDR);
			Status = IicPsMasterRecieve(IIC_DEVICE_ID_1, i2c_Recv_Buffer, IIC_SLAVE_ADDR);
			a = i2c_Recv_Buffer[0]<< 5;
			b = a | i2c_Recv_Buffer[1] >> 3;
			b /= 16;
			xil_printf("%d\xf8\x43\n\r", b); //take integer, which is in degrees C \xf8 = degree symbol, \x43 = C
			sleep(1); //built in latency
			break;
		case 14:
			//read the temperature from the analog board temp sensor
			IIC_SLAVE_ADDR=&IIC_SLAVE_ADDR3;
			i2c_Send_Buffer[0] = 0x0;
			i2c_Send_Buffer[1] = 0x0;
			Status = IicPsMasterSend(IIC_DEVICE_ID_0, i2c_Send_Buffer, i2c_Recv_Buffer, IIC_SLAVE_ADDR);
			Status = IicPsMasterRecieve(IIC_DEVICE_ID_0, i2c_Recv_Buffer, IIC_SLAVE_ADDR);
			a = i2c_Recv_Buffer[0]<< 5;
			b = a | i2c_Recv_Buffer[1] >> 3;
			b /= 16;
			xil_printf("%d\xf8\x43\n\r", b);
			sleep(1); //built in latency
			break;
		case 15:
			//read the temperature from the Extra Temp Sensor Board
			IIC_SLAVE_ADDR=&IIC_SLAVE_ADDR5;
			i2c_Send_Buffer[0] = 0x0;
			i2c_Send_Buffer[1] = 0x0;
			Status = IicPsMasterSend(IIC_DEVICE_ID_0, i2c_Send_Buffer, i2c_Recv_Buffer, IIC_SLAVE_ADDR);
			Status = IicPsMasterRecieve(IIC_DEVICE_ID_0, i2c_Recv_Buffer, IIC_SLAVE_ADDR);
			a = i2c_Recv_Buffer[0]<< 5;
			b = a | i2c_Recv_Buffer[1] >> 3;
			b /= 16;
			xil_printf("%d\xf8\x43\n\r", b);
			sleep(1); //built in latency
			break;
		default :
			break;
		} // End Switch-Case Menu Select

	}	// ******************* POLLING LOOP *******************//

	ffs_res = f_mount(NULL,"0:/",0);
	ffs_res = f_mount(NULL,"1:/",0);
    cleanup_platform();
    return 0;
}

//////////////////////////// InitializeAXIDma////////////////////////////////
// Sets up the AXI DMA
int InitializeAXIDma(void) {
	u32 tmpVal_0 = 0;
	//u32 tmpVal_1 = 0;

	tmpVal_0 = Xil_In32(XPAR_AXI_DMA_0_BASEADDR + 0x30);

	tmpVal_0 = tmpVal_0 | 0x1001; //<allow DMA to produce interrupts> 0 0 <run/stop>

	Xil_Out32 (XPAR_AXI_DMA_0_BASEADDR + 0x30, tmpVal_0);
	Xil_In32(XPAR_AXI_DMA_0_BASEADDR + 0x30);	//what does the return value give us? What do we do with it?

	return 0;
}
//////////////////////////// InitializeAXIDma////////////////////////////////

//////////////////////////// InitializeInterruptSystem////////////////////////////////
int InitializeInterruptSystem(u16 deviceID) {
	int Status;

	GicConfig = XScuGic_LookupConfig (deviceID);

	if(NULL == GicConfig) {

		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(&InterruptController, GicConfig, GicConfig->CpuBaseAddress);
	if(Status != XST_SUCCESS) {
		return XST_FAILURE;

	}

	Status = SetUpInterruptSystem(&InterruptController);
	if(Status != XST_SUCCESS) {
		return XST_FAILURE;

	}

	Status = XScuGic_Connect (&InterruptController,
			XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR,
			(Xil_ExceptionHandler) InterruptHandler, NULL);
	if(Status != XST_SUCCESS) {
		return XST_FAILURE;

	}

	XScuGic_Enable(&InterruptController, XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR );

	return XST_SUCCESS;

}
//////////////////////////// InitializeInterruptSystem////////////////////////////////


//////////////////////////// Interrupt Handler////////////////////////////////
void InterruptHandler (void ) {

	u32 tmpValue;
	tmpValue = Xil_In32(XPAR_AXI_DMA_0_BASEADDR + 0x34);
	tmpValue = tmpValue | 0x1000;
	Xil_Out32 (XPAR_AXI_DMA_0_BASEADDR + 0x34, tmpValue);

	global_frame_counter++;
}
//////////////////////////// Interrupt Handler////////////////////////////////


//////////////////////////// SetUp Interrupt System////////////////////////////////
int SetUpInterruptSystem(XScuGic *XScuGicInstancePtr) {
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XScuGic_InterruptHandler, XScuGicInstancePtr);
	Xil_ExceptionEnable();
	return XST_SUCCESS;

}
//////////////////////////// SetUp Interrupt System////////////////////////////////

//////////////////////////// ReadCommandPoll////////////////////////////////
// Function used to clear the read buffer
// Read In new command, expecting a <return>
// Returns buffer size read
int ReadCommandPoll() {
	u32 rbuff = 0;			// read buffer size returned

	XUartPs_SetOptions(&Uart_PS,XUARTPS_OPTION_RESET_RX);	// Clear UART Read Buffer
	memset(RecvBuffer, '0', 32);	// Clear RecvBuffer Variable
	while (!(RecvBuffer[rbuff-1] == '\n' || RecvBuffer[rbuff-1] == '\r'))
	{
		rbuff += XUartPs_Recv(&Uart_PS, &RecvBuffer[rbuff],(32 - rbuff));
		sleep(0.1);			// Built in Latency ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ 0.1 s
	}
	return rbuff;
}
//////////////////////////// ReadCommandPoll////////////////////////////////


//////////////////////////// Set Integration Times ////////////////////////////////
// wfid = 	0 for AA
//			1 for DFF
//			2 for LPF
// At the moment, the software is expecting a 5 char buffer.  This needs to be fixed.
void SetIntegrationTimes(int * setIntsArray, int * setSamples){
	int baseline_samples = 0;
	int short_samples = 0;
	int long_samples = 0;
	int full_samples = 0;

	xil_printf("  Enter Baseline Stop Time in ns: -52 to 0 <return> \t");
	ReadCommandPoll();
	sscanf(RecvBuffer,"%d",&setIntsArray[0]);
	xil_printf("\n\r");

	xil_printf("  Enter Short Integral Stop Time in ns: -52 to 8000 <return> \t");
	ReadCommandPoll();
	sscanf(RecvBuffer,"%d",&setIntsArray[1]);
	xil_printf("\n\r");

	xil_printf("  Enter Long Integral Stop Time in ns: -52 to 8000 <return> \t");
	ReadCommandPoll();
	sscanf(RecvBuffer,"%d",&setIntsArray[2]);
	xil_printf("\n\r");

	xil_printf("  Enter Full Integral Stop Time in ns: -52 to 8000 <return> \t");
	ReadCommandPoll();
	sscanf(RecvBuffer,"%d",&setIntsArray[3]);
	xil_printf("\n\r");

	if(!((setIntsArray[0]<setIntsArray[1])&&(setIntsArray[1]<setIntsArray[2])&&(setIntsArray[2]<setIntsArray[3])))
	{
		xil_printf("Invalid input: Integration times not greater than the previous.\r\n");
		sleep(1);
	}
	else
	{
//		setSamples[0] = ((setIntsArray[0] + 200) / 4) + 1;
//		setSamples[1] = ((setIntsArray[1] + 200) / 4) + 1;
//		setSamples[2] = ((setIntsArray[2] + 200) / 4) + 1;
//		setSamples[3] = ((setIntsArray[3] + 200) / 4) + 1;
		setSamples[0] = ((u32)((setIntsArray[0]+52)/4));
		setSamples[1] = ((u32)((setIntsArray[1]+52)/4));
		setSamples[2] = ((u32)((setIntsArray[2]+52)/4));
		setSamples[3] = ((u32)((setIntsArray[3]+52)/4));

		Xil_Out32 (XPAR_AXI_GPIO_0_BASEADDR, setSamples[0]);
		Xil_Out32 (XPAR_AXI_GPIO_1_BASEADDR, setSamples[1]);
		Xil_Out32 (XPAR_AXI_GPIO_2_BASEADDR, setSamples[2]);
		Xil_Out32 (XPAR_AXI_GPIO_3_BASEADDR, setSamples[3]);

		usleep(1);

		//read back the number of samples that the system has just set for the baseline integral window
		baseline_samples = Xil_In32 (XPAR_AXI_GPIO_0_BASEADDR);
		short_samples = Xil_In32 (XPAR_AXI_GPIO_1_BASEADDR);	//short integral window
		long_samples = Xil_In32 (XPAR_AXI_GPIO_2_BASEADDR);		//long integral window
		full_samples = Xil_In32 (XPAR_AXI_GPIO_3_BASEADDR);		//full integral window

		xil_printf("\n\r  Inputs Rounded to the Nearest 4 ns : Number of Samples\n\r");
		xil_printf("  Baseline Integral Window  [-200ns,%dns]: %d \n\r", (baseline_samples*4)-52, 38+baseline_samples);
		xil_printf("  Short Integral Window 	  [-200ns,%dns]: %d \n\r",(short_samples*4)-52, 38+short_samples);
		xil_printf("  Long Integral Window      [-200ns,%dns]: %d \n\r",(long_samples*4)-52, 38+long_samples);
		xil_printf("  Full Integral Window      [-200ns,%dns]: %d \n\r",(full_samples*4)-52, 38+full_samples);
	}

	return;
}
//////////////////////////// Set Integration Times ////////////////////////////////

//////////////////////////// read_data ////////////////////////////////
int read_data(unsigned int * data_array, FIL data_file){
	int array_index = 0;
	int dram_addr;
	int dram_base = 0xa000000;
	int dram_ceiling = 0xA004000; //read out just adjacent average (0xA004000 - 0xa000000 = 16384)

	dram_addr = dram_base;

	//read out the DRAM into our buffer
	while(dram_addr <= dram_ceiling)
	{
		data_array[array_index] = Xil_In32(dram_addr);
		dram_addr+=4;
		array_index++;
	}

	return sw;
}
//////////////////////////// read_data ////////////////////////////////

//////////////////////////// Clear Processed Data Buffers ////////////////////////////////
void ClearBuffers() {
	Xil_Out32(XPAR_AXI_GPIO_9_BASEADDR,1);
	usleep(1);
	Xil_Out32(XPAR_AXI_GPIO_9_BASEADDR,0);
}
//////////////////////////// Clear Processed Data Buffers ////////////////////////////////

//////////////////////////// getWFDAQ ////////////////////////////////

int getWFDAQ(int i_neutron_total){
	char c_SOH_buff[100] = "";
	int buffsize = 0; 	//BRAM buffer size
	int buff_num = 0;	//keep track of which buffer we are writing
	int i_sprintf_ret = 0;
	unsigned int sync_marker = 0x352EF853;
	//buffers are 4096 ints long (512 events total)
	unsigned int *data_array;
	data_array = (unsigned int *)malloc(sizeof(unsigned int)*4096*4);
	memset(data_array, '0', 4096 * sizeof(unsigned int)); //zero out the array
	FIL data_file;

	//timing
	XTime local_time_start, local_time_current, local_time,t_daq_start,t_daq_end;
	XTime_GetTime(&local_time_start);	//get the time
	local_time = 0; local_time_current = 0;
	t_daq_start = 0; t_daq_end = 0;

	XUartPs_SetOptions(&Uart_PS,XUARTPS_OPTION_RESET_RX);

	Xil_Out32 (XPAR_AXI_DMA_0_BASEADDR + 0x48, 0xa000000); 		// DMA Transfer Step 1
	Xil_Out32 (XPAR_AXI_DMA_0_BASEADDR + 0x58 , 65536);			// DMA Transfer Step 2
	sleep(1);
	ClearBuffers();

	while(1){
		XUartPs_Recv(&Uart_PS, &RecvBuffer, 32);
		if ( RecvBuffer[0] == 'q' ) { sw = 1; }
		if(sw) { free(data_array); return sw;	}

		buffsize = Xil_In32 (XPAR_AXI_GPIO_11_BASEADDR);	// AA write pointer // tells how far the system has read in the AA module
		if(buffsize == 1)
		{
			Xil_Out32 (XPAR_AXI_GPIO_15_BASEADDR, 1);				// init mux to transfer data between integrater modules to DMA
			Xil_Out32 (XPAR_AXI_DMA_0_BASEADDR + 0x48, 0xa000000);
			Xil_Out32 (XPAR_AXI_DMA_0_BASEADDR + 0x58 , 65536);
			usleep(54); 												// this will change
			Xil_Out32 (XPAR_AXI_GPIO_15_BASEADDR, 0);

			Xil_DCacheInvalidateRange(0xa0000000, 65536);

			switch(buff_num)
			{
			case 0:
				//fetch the data from the DRAM to the data array
				read_data(data_array, data_file);
				ClearBuffers();
				buff_num++;
				break;
			case 1:
				//fetch the data from the DRAM to the data array
				read_data(data_array + 4096, data_file);
				ClearBuffers();
				buff_num++;
				break;
			case 2:
				//fetch the data from the DRAM to the data array
				read_data(data_array + 8192, data_file);
				ClearBuffers();
				buff_num++;
				break;
			case 3:
				//fetch the data from the DRAM to the data array
				read_data(data_array + 12288, data_file);
				process_data(data_array, data_file);
				ClearBuffers();
				buff_num = 0;

				//check the timer to see when DAQ here starts
				XTime_GetTime(&t_daq_start);

				//write the event data to SD card
				ffs_res = f_open(&data_file, "1:/data_test_001.bin", FA_WRITE|FA_READ|FA_OPEN_ALWAYS);	//open the file
				if(ffs_res)
					xil_printf("Could not open file %d\n", ffs_res);
				ffs_res = f_lseek(&data_file, file_size(&data_file));	//seek to the end of the file
				//write the data //4 buffers total // 512 events per buff
				ffs_res = f_write(&data_file, data_array, sizeof(u32)*4096*4, &numBytesWritten);
				ffs_res = f_close(&data_file);

				XTime_GetTime(&t_daq_end);
				ffs_res = f_open(&data_file, "1:/data_test_001.bin", FA_WRITE|FA_READ|FA_OPEN_ALWAYS);	//open the file
				if(ffs_res)
					xil_printf("Could not open file %d\n", ffs_res);
				ffs_res = f_lseek(&data_file, file_size(&data_file));	//seek to the end of the file
				ffs_res = f_write(&data_file, &sync_marker, sizeof(unsigned int), &numBytesWritten);	//write the sync marker
				ffs_res = f_write(&data_file, &t_daq_end, sizeof(XTime), &numBytesWritten);
				ffs_res = f_write(&data_file, &t_daq_start, sizeof(XTime), &numBytesWritten);
				ffs_res = f_close(&data_file);
			}
		}

		//check the clock to see if we need to print SOH
		XTime_GetTime(&local_time_current);
		if(((local_time_current - local_time_start)/COUNTS_PER_SECOND) >= (local_time +  1))
		{
			local_time = (local_time_current - local_time_start)/COUNTS_PER_SECOND;

			i_sprintf_ret = snprintf(c_SOH_buff, 100, "%d_%d_%u_%u\n", 12, 34, i_neutron_total, (unsigned int)local_time);
			XUartPs_Send(&Uart_PS, (u8 *)c_SOH_buff, i_sprintf_ret);
			i_neutron_total++;
		}
	}

	free(data_array);
	return i_neutron_total;
}

//////////////////////////// getWFDAQ ////////////////////////////////
