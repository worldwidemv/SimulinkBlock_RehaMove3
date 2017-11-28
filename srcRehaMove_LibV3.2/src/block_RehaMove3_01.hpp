/*
 *      TU Berlin --- Fachgebiet Regelungssystem
 *      Simulink Block for the Hasomed GmbH device RehaMove3
 *
 *      Author: Markus Valtin
 *      Copyright © 2017 Markus Valtin <valtin@control.tu-berlin.de>. All rights reserved.
 *
 *      File:           block_RehaMove3_01.hpp -> Header file for the Simulink Wrapper.
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

#ifndef BLOCK_REHAMOVE3_01_HPP
#define BLOCK_REHAMOVE3_01_HPP

#include <RehaMove3Interface_SMPT32X.hpp>
using namespace nsRehaMove3_SMPT_32X_01;


// Constants
#define RM3_N_PULSES_MAX					10
#define RM3_STRING_SIZE_MAX					512

#define RM3_LOW_LEVEL_STIMULATION_PROTOCOL1	1
#define RM3_LOW_LEVEL_STIMULATION_PROTOCOL2	2
#define RM3_MID_LEVEL_STIMULATION_PROTOCOL	3


//sfunc_RehaMove3_XX
void lctRM3_Initialise(  void **work1, uint16_t stimOptions[], uint16_t sizeStimOptions, uint16_t llOptions[], uint16_t sizeLlOptions, uint16_t mlOptions[], uint16_t sizeMlOptions, uint16_t miscOptions[], uint16_t sizeMiscOptions, uint16_t inputSize1, uint16_t inputSize2, double sampleTime);
void lctRM3_InputOutput( void **work1, double u1[], double u2[], double y1[]);
void lctRM3_Deinitialise(void **work1);


// External declaration for class instance global storage
class block_RehaMove3 {
public:
	union convert64_t {
		double input;
		uint64_t output;
	};
	//public variables
	RehaMove3 *Device;

	struct rmpStatus_t {
		bool deviceIsInitialised;
		//
		bool deviceOpeningFailed;
		bool deviceIDsDidNotMatch;

		double	stimStatus1;
		uint32_t outputCounter;
		uint32_t outputCounterNext;
	} rmpStatus;

	// stimOptions = [size(stimDeviceID,2), uint8(stimDeviceID), size(stimDevicePath,2), uint8(stimDevicePath),
	// size(stimChannels,2), uint8(stimChannels), stimFrequency, stimRMPProtocol, stimMaxCurrent, stimMaxPulsWidth];
	struct stimOptions_t{
		char    blockID[RM3_STRING_SIZE_MAX];
		char    deviceID[RM3_STRING_SIZE_MAX];
		char    devicePath[RM3_STRING_SIZE_MAX];
		uint8_t numberOfActiveChannels;
		uint8_t channelsActive[RM3_N_PULSES_MAX];
		uint8_t stimFrequency;
		uint8_t rmpProtocol;
		float	maxCurrent;
		uint16_t maxPulseWidth;
		uint16_t errorAbortAfter;
		uint16_t errorRetestAfter;
		uint8_t useThreadForInit;
		uint8_t useThreadForAcks;
	} stimOptions;
	//llOptions = [ size(llPulseShape,2), uint16(llPulseShape), llNumberOfParts, uint16(llMaxStimVoltageValue) ];
	struct llOptions_t{
		uint8_t channelsPulseForm[RM3_N_PULSES_MAX];
		bool	pulseFormGivenAsInput;
		uint8_t	numberOfPulseParts;
		uint8_t	maxStimVoltage;
		uint8_t useDenervation;
	} llOptions;
	struct mlOptions_t{


	} mlOptions;
	// miscOptions= [uint8(miscPrintBlockParam), miscPrintDeviceInfos, miscPrintInitInfos, miscPrintRMPInitSettings,
	// miscPrintSendInfos, miscPrintReceiveInfos, miscPrintStimInfos, miscPrintSequenceErrors,
	// miscPrintCorrectionChargeWarnings, miscPrintStats, miscUseColors,   miscEnableAdvancedSettings, miscDisableVersionCheck];
	struct miscOptions_t{
		bool debugPrintBlockParameter;
		RehaMove3::rmpDebugSettings_t debug;
		bool enableAdvancedSettings;
		bool disableVersionCheck;
	} miscOptions;
	struct io_size_t {
		uint16_t sizeStimIn1;
		uint16_t sizeStimIn2;
	} ioSize;
	double sampleTime;

	actionResult_t		rmpResult;
	RehaMove3::rmpInitSettings_t rmpInitSettings;
	SequenceConfig_t	SequenceConfig;

	//public functions
	block_RehaMove3(void);
	~block_RehaMove3(void);

	void	TransverStimOptions(uint16_t *parameter, uint16_t parameterSize);
	void	TransverLlOptions(uint16_t *parameter, uint16_t parameterSize);
	void	TransverMlOptions(uint16_t *parameter, uint16_t parameterSize);
	void	TransverMiscOptions(uint16_t *parameter, uint16_t parameterSize, bool printDebugInfo);

private:
	//private variables

	//private functions
	void	CopyStringFromU16(char *to, uint16_t *from, uint16_t numberOfLetters);
};

#endif // BLOCK_REHAMOVE3_01_HPP
