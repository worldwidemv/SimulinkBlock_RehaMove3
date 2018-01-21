/*
 *      TU Berlin --- Fachgebiet Regelungssystem
 *      Simulink Block for the Hasomed GmbH device RehaMove3
 *
 *      Author: Markus Valtin
 *      Copyright © 2017 Markus Valtin <valtin@control.tu-berlin.de>. All rights reserved.
 *
 *      File:           block_RehaMove3_01.cpp -> Source file for the Simulink Wrapper.
 *      Version:        01 (2017)
 *      Changelog:
 *      	- 09.2017: initial release
 *
 *
 *      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 *      NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *      IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *      WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *      SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <cmath>

#include <block_RehaMove3_01.hpp>
//#define WITH_HW // Define for Debugging only

using namespace nsRehaMove3_SMPT_32X_01;

//sfunc_RehaMove3_XX
void lctRM3_Initialise(  void **work1, uint16_t stimOptions[], uint16_t sizeStimOptions, uint16_t llOptions[], uint16_t sizeLlOptions, double mlOptions[], uint16_t sizeMlOptions, uint16_t miscOptions[], uint16_t sizeMiscOptions, uint16_t inputSize1, uint16_t inputSize2_1, uint16_t inputSize2_2, double sampleTime )
{
	// create the stimulator block object and add it to the Simulink block struct
	block_RehaMove3 *bRehaMove3 = new block_RehaMove3();
	*work1 = (void *) bRehaMove3;

	// Debug options without printing the options
	bRehaMove3->TransverMiscOptions(miscOptions, sizeMiscOptions, false);

	if (bRehaMove3->miscOptions.debugPrintBlockParameter){
		printf("\n\nDEBUG OUTPUT for RehaMove3 Init -> START\n#############################################\n\n");
	}

	// Stim / General options
	bRehaMove3->TransverStimOptions(stimOptions, sizeStimOptions);
	bRehaMove3->rmStatus.deviceInitialisationAborted = false;
	switch (bRehaMove3->stimOptions.rmProtocol){
	case RM3_LOW_LEVEL_STIMULATION_PROTOCOL1:
	case RM3_LOW_LEVEL_STIMULATION_PROTOCOL2:
		// LowLevel options
		bRehaMove3->TransverLlOptions(llOptions, sizeLlOptions);
		break;
	case RM3_MID_LEVEL_STIMULATION_PROTOCOL:
		// MidLevel options
		bRehaMove3->TransverMlOptions(mlOptions, sizeMlOptions);
		break;
	default:
		printf("%s Error: Communication protocol %u is invalid! -> Initialisation aborted!\n\n", bRehaMove3->stimOptions.blockID, bRehaMove3->stimOptions.rmProtocol);
		bRehaMove3->rmStatus.deviceInitialisationAborted = true;
	}
	// Debug options
	bRehaMove3->TransverMiscOptions(miscOptions, sizeMiscOptions, true);
	// input/output sizes and sample time
	bRehaMove3->ioSize.sizeStimIn1 = inputSize1;
	bRehaMove3->ioSize.sizeStimIn2_1 = inputSize2_1;
	bRehaMove3->ioSize.sizeStimIn2_2 = inputSize2_2;
	bRehaMove3->sampleTime = sampleTime;
	if (bRehaMove3->miscOptions.debugPrintBlockParameter){
		printf("%s Block Debug: Input/Output Parameter\n  Input Size 'Pulse Width': %ux%u\n  Input Size 'Current': %ux%u\n  Output Size Stim Status: %ux%u\n  Sample Time: %f\n\n",
				bRehaMove3->stimOptions.blockID, bRehaMove3->ioSize.sizeStimIn1, bRehaMove3->ioSize.sizeStimIn2_1, bRehaMove3->ioSize.sizeStimIn1, bRehaMove3->ioSize.sizeStimIn2_2, 3, 1, bRehaMove3->sampleTime);
	}

#ifdef WITH_HW
	// Create Device Class
	bRehaMove3->Device = new nsRehaMove3_SMPT_32X_01::RehaMove3(bRehaMove3->stimOptions.blockID, bRehaMove3->stimOptions.devicePath);
	if (!bRehaMove3->rmStatus.deviceInitialisationAborted){
		// initialise the RehaMove Pro system
		if (bRehaMove3->Device->InitialiseRehaMove3(&bRehaMove3->rmInitSettings, &bRehaMove3->rmResult)) {
			// the initialisation was successful
			bRehaMove3->rmStatus.deviceIsInitialised = true;
			bRehaMove3->rmStatus.stimStatus1 = 1;
		} else {
			// the initialisation failed
			if (bRehaMove3->rmResult.finished){
				printf("%s Error: Initialisation failed!\n\n", bRehaMove3->stimOptions.blockID);
			}
			bRehaMove3->rmStatus.deviceIsInitialised = false;
			bRehaMove3->rmStatus.stimStatus1 = -1.0*(double)actionError_notInitialised;
			// take care of specific errors
			switch(bRehaMove3->rmResult.errorCode){
			case actionError_openingDevice:
				bRehaMove3->rmStatus.deviceOpeningFailed = true;
				bRehaMove3->rmStatus.stimStatus1 = -1.0*(double)actionError_openingDevice;
				break;
			case actionError_checkDeviceIDs:
				bRehaMove3->rmStatus.deviceIDsDidNotMatch = true;
				bRehaMove3->rmStatus.stimStatus1 = -1.0*(double)actionError_checkDeviceIDs;
				break;
			default:;
			}
		}
	}
#endif

	if (bRehaMove3->miscOptions.debugPrintBlockParameter){
		printf("\n#############################################\nDEBUG OUTPUT for RehaMove3 Init -> STOP\n\n");
	}
}


void lctRM3_InputOutput( void **work1, double u1[], double u2[], double y1[])
{
	block_RehaMove3 *bRehaMove3 = (block_RehaMove3*) *work1;
	double *pwIn		= u1;
	double *currentIn 	= u2;

	if (bRehaMove3->rmStatus.deviceInitialisationAborted){
		y1[0] = -2; // stimulator is NOT initialised and initialisation was aborted
		y1[1] = 0;
		y1[2] = 0;
		return;
	}

#ifdef WITH_HW
	if (bRehaMove3->rmStatus.deviceIsInitialised){
		/*
		 * Read the responses
		 */
		bool LastStimulationSuccessful = false;
		double PulseErrors = 0;

		switch(bRehaMove3->stimOptions.rmProtocol){
		case RM3_LOW_LEVEL_STIMULATION_PROTOCOL1:
		case RM3_LOW_LEVEL_STIMULATION_PROTOCOL2:
			LastStimulationSuccessful = bRehaMove3->Device->GetLastLowLevelStimulationResult(&PulseErrors);
			break;
		case RM3_MID_LEVEL_STIMULATION_PROTOCOL:
			LastStimulationSuccessful = bRehaMove3->Device->GetLastMidLevelStimulationResult(&PulseErrors);
			break;
		}

		if (LastStimulationSuccessful){
			y1[0] = bRehaMove3->rmStatus.stimStatus1;
			y1[1] = 0.0;	// no errors during pulse generation
		} else {
			y1[0] =  0.0; 	// mark this as error
			y1[1] = (double)PulseErrors;
		}

		/*
		 * build and send the new LowLevel sequence configuration
		 */
		switch(bRehaMove3->stimOptions.rmProtocol){
		case RM3_LOW_LEVEL_STIMULATION_PROTOCOL1:{
			// LowLevel with predefined stimulation pulse forms
			uint8_t j = 0;
			for (uint8_t i=0; i < bRehaMove3->stimOptions.numberOfActiveChannels; i++){
				// check that the pulse width is not 0; the current can be 0
				if (pwIn[i] != 0.0){
					// build stimulation configuration
					if (bRehaMove3->stimOptions.channelsActive[i] == 0){
						// the channel is 0 -> go to the next channel
						continue;
					}
					bRehaMove3->LlSequenceConfig.PulseConfig[j].Channel = bRehaMove3->stimOptions.channelsActive[i];
					bRehaMove3->LlSequenceConfig.PulseConfig[j].Shape = bRehaMove3->llOptions.channelsPulseForm[i];
					bRehaMove3->LlSequenceConfig.PulseConfig[j].PulseWidth = (uint16_t)pwIn[i];
					bRehaMove3->LlSequenceConfig.PulseConfig[j].Current = (float)currentIn[i];
					// next stimulation pulse
					j++;
				}
			}
			bRehaMove3->LlSequenceConfig.NumberOfPulses = j;

			// send the new sequence
			bRehaMove3->Device->SendNewPreDefinedLowLevelSequence(&bRehaMove3->LlSequenceConfig);
			break;}

		case RM3_LOW_LEVEL_STIMULATION_PROTOCOL2:{
			// LowLevel with user supplied stimulation pulse forms
			// TODO
			if (bRehaMove3->rmStatus.outputCounter  >= bRehaMove3->rmStatus.outputCounterNext){
				printf("%s Error: 'LowLevel' with user supplied pulse forms is not yet supported!\n", bRehaMove3->stimOptions.blockID);
				bRehaMove3->rmStatus.outputCounter = 0;
				bRehaMove3->rmStatus.outputCounterNext = (bRehaMove3->rmStatus.outputCounterNext +1) *2;
			} else {
				bRehaMove3->rmStatus.outputCounter++;
			}
			break;}

		case RM3_MID_LEVEL_STIMULATION_PROTOCOL:{
			// MidLevel stimulation
			memset(&bRehaMove3->MlUpdateConfig, 0, sizeof(RehaMove3::MlUpdateConfig_t));

			int8_t iCh = 0;
			for (uint8_t i=0; i < bRehaMove3->stimOptions.numberOfActiveChannels; i++){
				iCh = bRehaMove3->stimOptions.channelsActive[i] -1;
				if (iCh >= 0 && iCh < REHAMOVE_NUMBER_OF_CHANNELS){
					// check that the pulse width is not 0; the current can be 0
					if (pwIn[i] != 0.0){
						// build stimulation configuration
						bRehaMove3->MlUpdateConfig.ForceUpdate = false;
						bRehaMove3->MlUpdateConfig.RedoRamp = false;
						bRehaMove3->MlUpdateConfig.ActiveChannels[iCh] = true;
						bRehaMove3->MlUpdateConfig.PulseConfig[iCh].Channel = iCh;
						bRehaMove3->MlUpdateConfig.PulseConfig[iCh].Shape = Shape_Balanced_Symetric_Biphasic;
						if (bRehaMove3->rmInitSettings.MidLevelConfig.UseDynamicStimulationFrequncy){
							bRehaMove3->MlUpdateConfig.PulseConfig[iCh].Frequency =  (uint16_t)pwIn[i*2+0];
							bRehaMove3->MlUpdateConfig.PulseConfig[iCh].PulseWidth = (uint16_t)pwIn[i*2+1];
						} else {
							bRehaMove3->MlUpdateConfig.PulseConfig[iCh].Frequency = (float)bRehaMove3->rmInitSettings.MidLevelConfig.GeneralStimFrequency;
							bRehaMove3->MlUpdateConfig.PulseConfig[iCh].PulseWidth = (uint16_t)pwIn[i];
						}
						bRehaMove3->MlUpdateConfig.PulseConfig[iCh].Current = (float)currentIn[i];
					}
				}
			}

			// send the update
			bRehaMove3->Device->SendMidLevelUpdate(&bRehaMove3->MlUpdateConfig);
			break;}

		default:
			if (bRehaMove3->rmStatus.outputCounter  >= bRehaMove3->rmStatus.outputCounterNext){
				printf("%s Error: Unknown stimulation protocol! Only 'LowLevel' or 'MidLevel' is supported!\n\n", bRehaMove3->stimOptions.blockID);
				bRehaMove3->rmStatus.outputCounter = 0;
				bRehaMove3->rmStatus.outputCounterNext = (bRehaMove3->rmStatus.outputCounterNext +1) *2;
			} else {
				bRehaMove3->rmStatus.outputCounter++;
			}
		}

	} else {
		// the device is not initialised -> check if it is initialised now
		if (bRehaMove3->Device->IsDeviceInitialised(&bRehaMove3->rmResult)) {
			// the initialisation was successful
			bRehaMove3->rmStatus.deviceIsInitialised = true;
			bRehaMove3->rmStatus.stimStatus1 = 1;
		} else {
			// the initialisation failed
			if (bRehaMove3->rmResult.finished && (bRehaMove3->rmStatus.outputCounter  >= bRehaMove3->rmStatus.outputCounterNext)){
				printf("%s Error: Initialisation failed!\n\n", bRehaMove3->stimOptions.blockID);
				bRehaMove3->rmStatus.outputCounter = 0;
				bRehaMove3->rmStatus.outputCounterNext = (bRehaMove3->rmStatus.outputCounterNext +1) *2;
			} else {
				bRehaMove3->rmStatus.outputCounter++;
			}
			bRehaMove3->rmStatus.deviceIsInitialised = false;
			bRehaMove3->rmStatus.stimStatus1 = -1.0*(double)actionError_notInitialised;
			// take care of specific errors
			switch(bRehaMove3->rmResult.errorCode){
			case actionError_openingDevice:
				bRehaMove3->rmStatus.deviceOpeningFailed = true;
				bRehaMove3->rmStatus.stimStatus1 = -1.0*(double)actionError_openingDevice;
				break;
			case actionError_checkDeviceIDs:
				bRehaMove3->rmStatus.deviceIDsDidNotMatch = true;
				bRehaMove3->rmStatus.stimStatus1 = -1.0*(double)actionError_checkDeviceIDs;
				break;
			default:;
			}
		}
		y1[0] = -1; // stimulator is NOT initialised
		y1[1] = 0;
		y1[2] = 0;
	}
#endif

}

void lctRM3_Deinitialise(void **work1 )
{
	block_RehaMove3 *bRehaMove3 = (block_RehaMove3*) *work1;
	delete bRehaMove3;
	printf("\n\n");
}




block_RehaMove3::block_RehaMove3(void)
{
	// initialise the parameters
	Device = NULL;
	memset(&this->rmStatus, 0, sizeof(rmStatus_t));
	memset(&this->stimOptions, 0, sizeof(stimOptions_t));
	memset(&this->llOptions, 0, sizeof(llOptions_t));
	memset(&this->mlOptions, 0, sizeof(mlOptions_t));
	memset(&this->miscOptions, 0, sizeof(miscOptions_t));
	memset(&this->ioSize, 0, sizeof(io_size_t));
	this->sampleTime = 0.0;

	memset(&this->rmResult, 0, sizeof(this->rmResult));
	memset(&this->rmInitSettings, 0, sizeof(this->rmInitSettings));
	memset(&this->LlSequenceConfig, 0, sizeof(this->LlSequenceConfig));
	memset(&this->LlCustomSequenceConfig, 0, sizeof(this->LlCustomSequenceConfig));
	memset(&this->MlUpdateConfig, 0, sizeof(this->MlUpdateConfig));
}
block_RehaMove3::~block_RehaMove3(void)
{
#ifdef WITH_HW
	if (!this->rmStatus.deviceInitialisationAborted){
		this->Device->DeInitialiseDevice(this->miscOptions.debug.printInitInfos, this->miscOptions.debug.printStats);
	}
	this->rmStatus.deviceIsInitialised = false;
	delete Device;
#endif
}



void block_RehaMove3::TransverStimOptions(uint16_t *parameter, uint16_t parameterSize)
{
	uint32_t i = 1;
	block_RehaMove3::CopyStringFromU16(this->stimOptions.blockID, &parameter[1], (uint16_t)parameter[0]);
	i += (uint32_t)parameter[0];
	block_RehaMove3::CopyStringFromU16(this->stimOptions.deviceID, &parameter[i+1], (uint16_t)parameter[i]);
	i += (uint32_t)parameter[i] +1;
	block_RehaMove3::CopyStringFromU16(this->stimOptions.devicePath, &parameter[i+1], (uint16_t)parameter[i]);
	i += (uint32_t)parameter[i] +1;
	this->stimOptions.numberOfActiveChannels = (uint8_t)parameter[i];
	if (this->stimOptions.numberOfActiveChannels > RM3_N_PULSES_MAX){
		this->stimOptions.numberOfActiveChannels = RM3_N_PULSES_MAX;
	}
	block_RehaMove3::CopyStringFromU16((char*)this->stimOptions.channelsActive, &parameter[i+1], (uint16_t)parameter[i]);
	i += (uint32_t)parameter[i] +1;
	this->stimOptions.stimFrequency = (uint8_t)parameter[i++];
	this->stimOptions.rmProtocol   = (uint8_t)parameter[i++];
	this->stimOptions.maxCurrent    = ((float)parameter[i++])/(float)10.0;
	this->stimOptions.maxPulseWidth = (uint16_t)parameter[i++];
	this->stimOptions.errorAbortAfter  = (uint16_t)parameter[i++];
	this->stimOptions.errorRetestAfter = (uint16_t)parameter[i++];
	this->stimOptions.useThreadForInit   = (uint8_t)parameter[i++];
	this->stimOptions.useThreadForAcks   = (uint8_t)parameter[i++];

	// update the rm init struct
	if (strlen(this->stimOptions.deviceID) == 9){
		this->rmInitSettings.checkDeviceIDs = true;
		memcpy(this->rmInitSettings.RequestedDeviceID, this->stimOptions.deviceID, strlen(this->stimOptions.deviceID));
	}
	this->rmInitSettings.StimConfig.rmProtocol 		= this->stimOptions.rmProtocol;
	this->rmInitSettings.StimConfig.StimFrequency 		= this->stimOptions.stimFrequency;
	this->rmInitSettings.StimConfig.PulseWidthMax 		= this->stimOptions.maxPulseWidth;
	this->rmInitSettings.StimConfig.CurrentMax    		= this->stimOptions.maxCurrent;
	this->rmInitSettings.StimConfig.ErrorAbortAfter    = this->stimOptions.errorAbortAfter;
	this->rmInitSettings.StimConfig.ErrorRetestAfter   = this->stimOptions.errorRetestAfter;
	this->rmInitSettings.StimConfig.UseThreadForInit   = (bool)this->stimOptions.useThreadForInit;
	this->rmInitSettings.StimConfig.UseThreadForAcks   = (bool)this->stimOptions.useThreadForAcks;

	// print debug output
	if (this->miscOptions.debugPrintBlockParameter){
		printf("%s Block Debug: General Parameter (%u values)\n  Block ID: '%s'\n  Device Serial Number: '%s'\n  Device Path: '%s'\n  Number of Active Channels: %u\n  Active Channels: [ ",
				this->stimOptions.blockID, parameterSize, this->stimOptions.blockID,  this->stimOptions.deviceID, this->stimOptions.devicePath, this->stimOptions.numberOfActiveChannels);
		for (uint8_t i=0; i<this->stimOptions.numberOfActiveChannels; i++){
			printf("%u ", this->stimOptions.channelsActive[i]);
		}
		char rmProtocol[100];
		if (this->stimOptions.rmProtocol == RM3_MID_LEVEL_STIMULATION_PROTOCOL){
			sprintf(rmProtocol, "MidLevel Protocol");
		} else {
			sprintf(rmProtocol, "LowLevel Protocol");
		}
		printf("]\n  Stimulation Frequency: %u.00 Hz\n  RehaMove3 Protocol: %s\n  Max. Current: %0.1f mA\n  Max. Pulse Width: %u µs\n  Abort after N Errors: %u\n  ReTest after N seconds: %0.2f s\n",
				this->stimOptions.stimFrequency, rmProtocol, this->stimOptions.maxCurrent, this->stimOptions.maxPulseWidth, this->stimOptions.errorAbortAfter, ((double)this->stimOptions.errorRetestAfter / (double)this->stimOptions.stimFrequency));
		printf("  Use Thread for Init: %u\n  Use Thread for Data: %u\n",
				this->stimOptions.useThreadForInit, this->stimOptions.useThreadForAcks);
	}
}
void block_RehaMove3::TransverLlOptions(uint16_t *parameter, uint16_t parameterSize)
{
	uint16_t i = 0;
	block_RehaMove3::CopyStringFromU16((char*)this->llOptions.channelsPulseForm, &parameter[i+1], (uint16_t)parameter[i]);
	i += (uint32_t)parameter[i] +1;
	this->llOptions.numberOfPulseParts = (uint8_t)parameter[i++];
	if (this->llOptions.numberOfPulseParts > 0){
		this->llOptions.pulseFormGivenAsInput = true;
	}
	this->llOptions.maxStimVoltage = (uint8_t)parameter[i++];
	this->llOptions.useDenervation = (uint8_t)parameter[i++];


	this->rmInitSettings.LowLevelConfig.HighVoltageLevel = this->llOptions.maxStimVoltage;
	this->rmInitSettings.LowLevelConfig.UseDenervation = false; // TODO add feature if supported

	if (this->miscOptions.debugPrintBlockParameter){
		uint8_t maxStimVoltage = 0;
		switch (this->llOptions.maxStimVoltage){
			case 0:
			case 1: maxStimVoltage = 0; break;
			case 2: maxStimVoltage = 30; break;
			case 3: maxStimVoltage = 60; break;
			case 4: maxStimVoltage = 90; break;
			case 5: maxStimVoltage = 120; break;
			case 6: maxStimVoltage = 150; break;
			default: maxStimVoltage = 150;
		}
		printf("%s Block Debug: LowLevel Parameter (%u values)\n  Stimulation Pulse Forms: [ ",
				this->stimOptions.blockID, parameterSize);
		for (uint8_t i=0; i<this->stimOptions.numberOfActiveChannels; i++){
			printf("%u ", this->llOptions.channelsPulseForm[i]);
		}
		printf("]\n  Pulse Form given as Input: %u\n  Number of Pulse Parts: %u\n  Max. Stimulation Voltage: %u V\n  Stimulate Denerved Muscles: %u\n",
				this->llOptions.pulseFormGivenAsInput, this->llOptions.numberOfPulseParts, maxStimVoltage, this->llOptions.useDenervation);
	}
}
void block_RehaMove3::TransverMlOptions(double *parameter, uint16_t parameterSize)
{
	uint16_t i = 0;
	this->mlOptions.fStimML = parameter[i++];
	this->mlOptions.useDynamicStimulationFrequncy = (parameter[i++] == 1);
	this->mlOptions.useSoftStart = (parameter[i++] == 1);
	this->mlOptions.useRamps = (parameter[i++] == 1);
	this->mlOptions.rampsUpdates = parameter[i++];
	this->mlOptions.rampsZeroUpdates = parameter[i++];
	this->mlOptions.nKeepAlive = parameter[i++];

	this-> rmInitSettings.MidLevelConfig.GeneralStimFrequency = this->mlOptions.fStimML;
	this-> rmInitSettings.MidLevelConfig.UseDynamicStimulationFrequncy = this->mlOptions.useDynamicStimulationFrequncy;
	this-> rmInitSettings.MidLevelConfig.UseSoftStart = this->mlOptions.useSoftStart;
	this-> rmInitSettings.MidLevelConfig.SetRampsDuringPeriodicMlUpdateCall = true;
	this-> rmInitSettings.MidLevelConfig.UseRamps = this->mlOptions.useRamps;
	this-> rmInitSettings.MidLevelConfig.RampsUpdates = this->mlOptions.rampsUpdates;
	this-> rmInitSettings.MidLevelConfig.RampsZeroUpdates = this->mlOptions.rampsZeroUpdates;
	this-> rmInitSettings.MidLevelConfig.SendKeepAliveSignalDuringPeriodicMlUpdateCall = true;
	this-> rmInitSettings.MidLevelConfig.KeepAliveNumberOfUpdateCalls = this->mlOptions.nKeepAlive;

	if (this->miscOptions.debugPrintBlockParameter){
		printf("%s Block Debug: MidLevel Parameter (%u values)\n  Stimulation Frequency: %f; dynamic: %u\n  Number of In/Out calls until KeepAlive signal is send: %1.0f\n  SoftStart: %u\n  intensity ramp up: %u; during %1.0f updates; re-do the ramp after %1.0f zero updates\n",
						this->stimOptions.blockID, parameterSize, this->mlOptions.fStimML, this->mlOptions.useDynamicStimulationFrequncy, this->mlOptions.nKeepAlive, this->mlOptions.useSoftStart,
						this->mlOptions.useRamps, this->mlOptions.rampsUpdates, this->mlOptions.rampsZeroUpdates);
	}
}
void block_RehaMove3::TransverMiscOptions(uint16_t *parameter, uint16_t parameterSize, bool printDebugInfo)
{
	uint16_t i = 0;
	this->miscOptions.debugPrintBlockParameter = (bool)parameter[i++];
	this->miscOptions.debug.printDeviceInfos = (bool)parameter[i++];
	this->miscOptions.debug.printInitInfos = (bool)parameter[i++];
	this->miscOptions.debug.printInitSettings = (bool)parameter[i++];
	this->miscOptions.debug.printSendCmdInfos = (bool)parameter[i++];
	this->miscOptions.debug.printReceivedAckInfos = (bool)parameter[i++];
	this->miscOptions.debug.printStimInfos = (bool)parameter[i++];
	this->miscOptions.debug.printErrorsSequence = (bool)parameter[i++];
	this->miscOptions.debug.printCorrectionChargeWarnings = (bool)parameter[i++];
	this->miscOptions.debug.printStats = (bool)parameter[i++];
	this->miscOptions.debug.useColors = (bool)parameter[i++];

	this->miscOptions.enableAdvancedSettings = (bool)parameter[i++];
	this->miscOptions.debug.disableVersionCheck = (bool)parameter[i++];

	memcpy(&this->rmInitSettings.DebugConfig, &this->miscOptions.debug, sizeof(this->rmInitSettings.DebugConfig));


	if (printDebugInfo){
		if (this->miscOptions.debugPrintBlockParameter){
			printf("%s Block Debug: Misc Parameter (%u values)\n  Print Block Parameter: %u\n  Print Device Infos: %u\n  Print Init Infos: %u\n  Print Init Settings: %u\n  Print Send CMD Infos: %u\n  Print Received ACK Infos: %u\n  Print Pulse Config; %u\n  Print Sequence Errors: %u\n  Print Input Corrections / Unbalanced Charge: %u\n  Print Stats: %u\n  Use Colors for Errors/Warnings: %u\n",
					this->stimOptions.blockID, parameterSize, this->miscOptions.debugPrintBlockParameter, this->miscOptions.debug.printDeviceInfos, this->miscOptions.debug.printInitInfos, this->miscOptions.debug.printInitSettings, this->miscOptions.debug.printSendCmdInfos, this->miscOptions.debug.printReceivedAckInfos, this->miscOptions.debug.printStimInfos, this->miscOptions.debug.printErrorsSequence, this->miscOptions.debug.printCorrectionChargeWarnings, this->miscOptions.debug.printStats, this->miscOptions.debug.useColors);
			printf("  Enable Advanced Settings: %u\n  Disable Version Check: %u\n",
					this->miscOptions.enableAdvancedSettings, this->miscOptions.disableVersionCheck);
		}
	}
}


void block_RehaMove3::CopyStringFromU16(char *to, uint16_t *from, uint16_t numberOfLetters)
{
	for (uint16_t i=0 ; i<numberOfLetters; i++){
		to[i] = (uint8_t)from[i];
	}
}
