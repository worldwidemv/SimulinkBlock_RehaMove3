/*
 * RehaMove3.cpp
 *
 *  Created on: 07.02.2016
 *      Author: valtin
 */

#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <limits.h>

#include <RehaMove3Interface_SMPT32X.hpp>

using namespace std;
using namespace nsRehaMove3_SMPT_32X_01;

int main(int argc, char *argv[]) {
	char *DeviceName, TempString[100] = {0};
	FILE *fd_data;
	int ValuesToGet = 0, PackageNumber = 0;;
	int StatusProzent = 0, StatusProzentOld = 0;
	struct timeval Time;
	long T0 = 0, T1 = 0, T2 = 0;

	RehaMove3 *Device;

	if (argc < 3){ // at least 2 Arguments
		// We print argv[0] assuming it is the program name
		cout << "Usage: " << argv[0] << " <devicename> <number of pulses>\n";
		return -1;
	} else {
		// We assume argv[1] is a filename to open
		DeviceName = argv[1];

		ValuesToGet = atoi(argv[2]);
		if ((ValuesToGet <= 0) | (ValuesToGet > 100000000)) {
			fprintf(stderr,	"Error: Number of pulses invalid! Must be a number between 1 and 100000000! (IS: \'%i\')\n", ValuesToGet);
			exit(0);
		}

		sprintf(TempString, "EMG_Data.csv");
		fd_data = fopen(TempString, "w");
		if (fd_data < NULL){
			fprintf(stderr,"Error: Output Datei \'%s\' lässt sich nicht öffnen. \n\n", TempString);
			exit(0);
		}
	}

	// Haupprogrammteil
	// ##############################################################################################
	printf("Testprogram für den HASOMED RehaMove Pro Version 3.1.9\n=========================================\n");

	// INIT
	// Open the Device
	Device = new nsRehaMove3_SMPT_32X_01::RehaMove3("STIM Standalone", DeviceName);   // Create Device Class
	//Device->DoDeviceReset();
	// START
	gettimeofday(&Time, NULL);
	T0 = Time.tv_sec * 1000 + Time.tv_usec / 1000;

	RehaMove3::rmpInitSettings_t InitSetup = {0};

	// LowLevel
	InitSetup.StimConfig.StimFrequency = 20;
	InitSetup.StimConfig.ErrorAbortAfter = 2;
	InitSetup.StimConfig.ErrorRetestAfter = 20;
	InitSetup.StimConfig.PulseWidthMax = 500;
	InitSetup.StimConfig.CurrentMax    = 50.0;
	InitSetup.StimConfig.UseThreadForInit = false;
	InitSetup.StimConfig.UseThreadForAcks = true;
	InitSetup.LowLevelConfig.HighVoltageLevel = Smpt_High_Voltage_60V;
	InitSetup.LowLevelConfig.UseDenervation = false;
	// Debug
	InitSetup.DebugConfig.printErrorsSequence = true;
	InitSetup.DebugConfig.printDeviceInfos = true;		// done
//	InitSetup.DebugConfig.printInitInfos = true;		// done
//	InitSetup.DebugConfig.printInitSettings = true;		// done
//	InitSetup.DebugConfig.printStats = true;			// done
	InitSetup.DebugConfig.printCorrectionChargeWarnings = true;
//	InitSetup.DebugConfig.printReceivedAckInfos = true;
//	InitSetup.DebugConfig.printSendCmdInfos = true;
	InitSetup.DebugConfig.printStimInfos = true;
	InitSetup.DebugConfig.disableVersionCheck = true;
	InitSetup.DebugConfig.useColors = true;

	SequenceConfig_t SC = {};
	SC.NumberOfPulses = 1;
	SC.PulseConfig[0].Channel = Smpt_Channel_Red+1;
	SC.PulseConfig[0].Current = 5;
	SC.PulseConfig[0].PulseWidth = 300;
	SC.PulseConfig[0].Shape = Shape_Balanced_Symetric_Biphasic;
	SC.PulseConfig[1].Channel = Smpt_Channel_Red+1;
	SC.PulseConfig[1].Current = 0;
	SC.PulseConfig[1].PulseWidth = 200;
	SC.PulseConfig[1].Shape = Shape_UNbalanced_UNsymetric_Monophasic;
	SC.PulseConfig[2].Channel = Smpt_Channel_Red+1;
	SC.PulseConfig[2].Current = 10.0;
	SC.PulseConfig[2].PulseWidth = 0;
	SC.PulseConfig[2].Shape = Shape_Balanced_Symetric_Biphasic_NEGATIVE;

	// Initialisation RehaMove Pro System
	actionResult_t Result;
	if (!Device->InitialiseRehaMove3(&InitSetup, &Result)) {
		if (!InitSetup.StimConfig.UseThreadForInit){
			printf("RehaMove Pro: Initialisation failed!\n\n");
			// free memory
			delete Device;

			gettimeofday(&Time, NULL);
			T2 = Time.tv_sec * 1000 + Time.tv_usec / 1000;

			printf("\nExit\n->  benötigte Gesamtzeit: %f Sekunden\n", (T2 - T0) / 1000.0);
			exit(-1);
		} else {
			// wait until the init was successful
			uint32_t watchDog = 4*60;
			do {
				if (Device->IsDeviceInitialised(&Result)){
					break;
				}
				usleep(250000); // 250ms
				watchDog--;
			} while (watchDog > 0);
			if (watchDog == 0){
				printf("RehaMove Pro: Initialisation failed!\n\n");
				// free memory
				delete Device;

				gettimeofday(&Time, NULL);
				T2 = Time.tv_sec * 1000 + Time.tv_usec / 1000;

				printf("\nExit\n->  benötigte Gesamtzeit: %f Sekunden\n", (T2 - T0) / 1000.0);
				exit(-1);
			}
		}
	}

	gettimeofday(&Time, NULL);
	T2 = Time.tv_sec * 1000.0 + Time.tv_usec / 1000.0;
	printf("\nInit took: %fms\n", (double) (T2 - T0));

	// LOOP
	// Send Stim Commands ....
	gettimeofday(&Time, NULL);
	T1 = Time.tv_sec * 1000 + Time.tv_usec / 1000;
	printf("\n   -->  0 %% done ....");
	fflush(stdout);

	RehaMove3::rmpGetStatus_t Status = Device->GetCurrentStatus(false, false, 0);
	uint16_t Errors;
	bool     SequenceComplete;

	usleep(50000); // 50ms
	while (PackageNumber < (int) ValuesToGet) {
		// time
		gettimeofday(&Time,NULL);
		T2 = Time.tv_sec * 1000.0 + Time.tv_usec / 1000.0;

		// get the results
		SequenceComplete = false;
		Errors = 0;
		if (!Device->GetLastSequenceResult(&Errors, &SequenceComplete)){
			if (Errors != 0){
				printf("\n### Sequence failed -> Channels: %d\n\n", Errors);
			}
			if (!SequenceComplete){
				printf("\n### Sequence was incomplete\n\n");
			}
		} else {
			printf("\n### Sequence was SUCCESSFUL\n\n");
		}

		// Stimulation Command
		Device->SendNewPreDefinedLowLevelSequence(&SC);

		PackageNumber++;

		// status
		Status = Device->GetCurrentStatus(false, false, 0);
		// status info
		StatusProzent = (int) ((100.0 / (double) ValuesToGet) * (double) PackageNumber);
		if (StatusProzent != StatusProzentOld) {
			printf("\r   --> % 2d %% done ....", StatusProzent);
			fflush(stdout);
			StatusProzentOld = StatusProzent;
		}

		usleep(50000); // 50ms
	}
	printf("\n\n");
	// END

	// Stopping STIMULATOR
	Device->DeInitialiseDevice(InitSetup.DebugConfig.printInitInfos, InitSetup.DebugConfig.printStats);
	// free memory
	delete Device;
	gettimeofday(&Time, NULL);
	T2 = Time.tv_sec * 1000 + Time.tv_usec / 1000;

	printf("\nExit\n->  benötigte Gesamtzeit: %f Sekunden\n", (T2 - T0) / 1000.0);
	exit(0);
}

