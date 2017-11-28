/*
 *      TU Berlin --- Fachgebiet Regelungssystem
 *      C++ Interface class for the Hasomed GmbH device RehaMove3
 *
 *      Author: Markus Valtin
 *      Copyright © 2017 Markus Valtin <valtin@control.tu-berlin.de>. All rights reserved.
 *
 *      File:           RehaMove3Interface_SMPT324.hpp -> Header file for the C++ interface class.
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


#ifndef REHAMOVE3INTERFACE_SMPT324_H
#define REHAMOVE3INTERFACE_SMPT324_H

#if defined(__APPLE__) || defined(__unix__)
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>
#else
// Windows is not supported at the moment
//#include <windows.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <cstring>
#include <math.h>
#include <cmath>
#include <fcntl.h>
#include <unistd.h>

extern "C" {
// General
#include "smpt_client.h"
// Low_lev
#include "smpt_ll_client.h"
}

extern "C" {
	// Lib Error printf function
	void PrintLibError(const char* format);
}

/*
 * RehaMove3 version number(s) which are supported by this version of the interface class!
 *
 * Make sure every supported version has a value for {major, minor, revision}
 */
static const uint8_t RMP_NumberOfSupportedVersionsMax 	= 3;
static const uint8_t RMP_NumberOfSupportedVersionsMain 	= 1;
static const uint8_t RMP_SupportedVersionsMain[][3] 	= {{2,1,16}};
static const uint8_t RMP_NumberOfSupportedVersionsStim 	= 1;
static const uint8_t RMP_SupportedVersionsStim[][3] 	= {{2,1,0}};
static const uint8_t RMP_NumberOfSupportedVersionsSMPT 	= 1;
static const uint8_t RMP_SupportedVersionsSMPT[][3] 	= {{3,2,4}};
static const uint8_t RMP_ErrorWarningBand				= 3; // warning if difference of minor version number is smaller


/*
 * RehaMove3 Interface defines
 */
#define REHAMOVE_MAX_RESETS_INIT							20
#define REHAMOVE_MAX_SEQUENCE_SIZE							12
#define REHAMOVE_RESPONSE_QUEUE_SIZE						100
#define REHAMOVE_RESPONSE_ERROR_DESC_SIZE					100
#define REHAMOVE_SEQUENCE_QUEUE_SIZE						5
#define REHAMOVE_ACK_THREAD_DELAY_US						1000

#define REHAMOVE_SHAPES__NUMBER_OF_POINTS_MAX				16
#define REHAMOVE_SHAPES__PW_MIN                        		10		// min. 10 us
#define REHAMOVE_SHAPES__PW_MAX                        		4000	// max. 4000 us
#define REHAMOVE_SHAPES__I_MIN                         		0.5		// min. 0.5 mA
#define REHAMOVE_SHAPES__I_MAX                         		150.0	// max. 150.5 mA, depending on the Vstim_max
#define REHAMOVE_SHAPES__PULSWIDTH_BALANCED_UNSYMETRIC		4000
#define REHAMOVE_SHAPES__CURRENT_MIN_BALANCED_UNSYMETRIC	3

#define REHAMOVE_SHAPES__TRIAGLE_MAX_POINTS_BI              14		// max. 14 from 16 points for biphasic / balanced pulse
#define REHAMOVE_SHAPES__TRIAGLE_MAX_POINTS_MONO            16		// max. 16 points for monophasic / unbalanced pulse
#define REHAMOVE_SHAPES__TRIAGLE_USE_FIXED_PW_STEP          false	//true
#define REHAMOVE_SHAPES__TRIAGLE_PW_STEP                    10		// min. 10 us


/*
 * RehaMove3 namespace for the library version 3.2.X
 */
namespace nsRehaMove3_SMPT_32X_01 {


struct PulseConfig_t {
	uint8_t  Channel;
	uint8_t  Shape;
	uint16_t PulseWidth;
	float    Current;
};
struct SequenceConfig_t {
	uint8_t 		NumberOfPulses;
	PulseConfig_t 	PulseConfig[REHAMOVE_MAX_SEQUENCE_SIZE];
};

enum actionErrorCode_t {
	actionError_NoError,
	actionError_notInitialised,
	actionError_openingDevice,
	actionError_getDeviceInfo,
	actionError_checkDeviceIDs,
	actionError_initFailed,
	actionError_initLL,
	actionError_initML
};
struct actionResult_t {
	bool 	finished;
	bool	successful;
	actionErrorCode_t errorCode;
	char	errorMessage[1000];
};

struct LowLevelAcks_t {
	Smpt_get_device_id_ack G_device_id_ack;
	Smpt_get_version_ack G_version_ack;
	Smpt_ll_init_ack G_ll_init_ack; 					/* Struct for init response */
};


struct DeviceStatistic_t {
	// inputs
	uint64_t InvalidInput;
	uint32_t InputCorrections_PulswidthOver;
	uint32_t InputCorrections_PulswidthUnder;
	uint32_t InputCorrections_CurrentOver;
	uint32_t InputCorrections_CurrentUnder;
	// sequence execution
	uint64_t SequencesSend;
	uint64_t SequencesNotSend;
	uint64_t SequencesSuccessful;
	uint64_t SequencesFailed;
	uint64_t SequencesFailed_StimError;
	uint64_t StimultionPulsesSend;
	uint64_t StimultionPulsesNotSend;
	uint64_t StimultionPulsesSuccessful;
	uint64_t StimultionPulsesFailed;
	uint64_t StimultionPulsesFailed_StimError;
};

struct SingleResponse_t {
	Smpt_Cmd Request;
	bool WaitForResponce;
	bool WaitTimedOut;
	bool ResponseReceived;
	Smpt_ack Ack;
	bool Error;
	char ErrorDescription[REHAMOVE_RESPONSE_ERROR_DESC_SIZE]; // todo: add define for Error size
};

struct ResponseQueue_t {
	SingleResponse_t  	Queue[REHAMOVE_RESPONSE_QUEUE_SIZE];
    uint8_t		  	QueueHead;
    uint8_t			QueueTail;
};

struct SingleStimulationPulse_t {
	uint8_t Channel;
	uint8_t PackageNumber;
	uint8_t Result;
};

struct StimulationSequence_t {
	SingleStimulationPulse_t 	StimulationPulse[REHAMOVE_MAX_SEQUENCE_SIZE];
	uint64_t 					SequenceNumber;
	bool						SequenceWasSuccessful;
	uint8_t 					NumberOfPulses;
	uint8_t 					NumberOfAcks;
};

struct SequenceQueue_t {
	StimulationSequence_t Queue[REHAMOVE_SEQUENCE_QUEUE_SIZE];
    uint8_t		  	QueueHead;
    uint8_t			QueueTail;
    uint8_t			QueueSize;
	bool 			DoNotReportUnclaimedSequenceResults;

};

enum PulseShapes_t {
	Shape_Balanced_Symetric_Biphasic		 			= 0, // 0  -> Biphasischer gleichmässiger ausgeglichener Puls, erster Puls POSITIV
	Shape_Balanced_Symetric_Biphasic_NEGATIVE 			= 1, // 1  -> Biphasischer gleichmässiger ausgeglichener Puls, erstem, Puls NEGATIV
	Shape_Balanced_UNsymetric_Biphasic 					= 2, // 2  -> Biphasischer ungleichmässiger ausgeglichener Puls mit erstem, großem Puls POSITIV
	Shape_Balanced_UNsymetric_Biphasic_NEGATIVE 		= 3, // 3  -> Biphasischer ungleichmässiger ausgeglichener Puls mit erstem, großem Puls NEGATIV
	Shape_UNbalanced_UNsymetric_Monophasic 				= 4, // 4  -> monophasischer Puls POSITIV; polarity depends on the current sign
	Shape_UNbalanced_UNsymetric_Monophasic_NEGATIVE 	= 5, // 5  -> monophasischer Puls NEGATIV
	//
	Shape_UNbalanced_UNsymetric_Biphasic_FIRST 			= 6, // 6  -> erster  Teil eines biphasischen, UNgleichmässigen und UNausgeglichenen Pulses, Polarität wird durch den Strom bestimmt
	Shape_UNbalanced_UNsymetric_Biphasic_SECOUND 		= 7, // 7  -> zweiter Teil eines biphasischen, UNgleichmässigen und UNausgeglichenen Pulses, Polarität entgegengesetzt zum ersten Puls
	//
	Shape_Balanced_UNsymetric_Biphasic_FIRST 			= 8, // 8  -> erster  Teil eines biphasischen, UNgleichmässigen und ausgeglichenen Pulses, Polarität wird durch den Strom bestimmt   (drei Teile)
	Shape_Balanced_UNsymetric_Biphasic_SECOUND 			= 9, // 9  -> zweiter Teil eines biphasischen, UNgleichmässigen und ausgeglichenen Pulses, Polarität entgegengesetzt zum ersten Puls (drei Teile)
	//
	Shape_Balanced_UNsymetric_LONG_Biphasic_FIRST 		= 10, // 10  -> erster  Teil eines LANGEN biphasischen, UNgleichmässigen und ausgeglichenen Pulses, Polarität wird durch den Strom bestimmt   (drei Teile)
	Shape_Balanced_UNsymetric_LONG_Biphasic_SECOUND 	= 11, // 11  -> zweiter Teil eines LANGEN biphasischen, UNgleichmässigen und ausgeglichenen Pulses, Polarität entgegengesetzt zum ersten Puls (drei Teile)
	//
	Shape_Balanced_UNsymetric_RisingTriangle 			= 12, // 12 -> dreieckiger UNgleichmässigen und ausgeglichenen Pulses, Polarität wird durch den Strom bestimmt; Dreieck in der ersten Flange, erster Impuls
	Shape_Balanced_UNsymetric_FallingTriangle 			= 13, // 13 -> dreieckiger UNgleichmässigen und ausgeglichenen Pulses, Polarität wird durch den Strom bestimmt; Dreieck in der zweiten Flange, zweiter Impuls
	Shape_UNbalanced_UNsymetric_RisingTriangle 			= 14, // 14 -> dreieckiger UNgleichmässigen und UNausgeglichener Pulse, Polarität wird durch den Strom bestimmt; Dreieck in der ersten Flange
	Shape_UNbalanced_UNsymetric_FallingTriangle 		= 15, // 15 -> dreieckiger UNgleichmässigen und ausgeglichenen Pulses, Polarität wird durch den Strom bestimmt; Dreieck in der zweiten Flange
	//
	Shape_UNbalanced_Charge_Compensation 				= 16, // 16  -> Kompensataionspuls um den Ladungsausgleich herzustellen
	//
	LastPulsShape										= 16
};

class RehaMove3 {
public:
	struct rmpStimSettings_t {
		uint8_t  StimFrequency;
		uint16_t PulseWidthMax;
		float	 CurrentMax;
		uint16_t ErrorAbortAfter;
		uint16_t ErrorRetestAfter;
		bool  	 UseThreadForInit;
		bool	 UseThreadForAcks;
	};
	struct rmpLowLevelSettings_t {
		uint8_t  HighVoltageLevel;
		bool 	 UseDenervation;  // not implemented yet
	};
	struct rmpDebugSettings_t{
		bool printDeviceInfos;
		bool printInitInfos;
		bool printInitSettings;
		bool printSendCmdInfos;
		bool printStimInfos;
		bool printReceivedAckInfos;
		bool printCorrectionChargeWarnings;
		bool printErrorsSequence;
		bool printStats;
		bool useColors;
		bool disableVersionCheck;
	};

	struct rmpInitSettings_t {
		// General
		bool 	checkDeviceIDs;
		char 	RequestedDeviceID[Smpt_Length_Device_Id];
		rmpStimSettings_t StimConfig;
		// LowLevel
		rmpLowLevelSettings_t LowLevelConfig;
		// MidLevel
		// Misc/Debug
		rmpDebugSettings_t DebugConfig;
	};

	struct rmpGetStatus_t {
		bool DeviceIsOpen;
		bool DeviceLlIsInitialised;
		bool DeviceMlIsInitialised;
		bool DeviceInitialised;
		bool DoNotStimulate;
		uint16_t NumberOfStimErrors;
		uint64_t LastUpdated;
		uint8_t	 BatteryLevel;
		float	 BatteryVoltage;
		uint8_t  HighVoltageVoltage;
	};

	RehaMove3(const char *DeviceID, const char *SerialDeviceFile);
	~RehaMove3(void);

	rmpGetStatus_t GetCurrentStatus(bool DoPrintStatus, bool DoPrintStatistic, uint16_t WaitTimeout);

	bool 	InitialiseRehaMove3(rmpInitSettings_t *InitSetup, actionResult_t *InitResult);
	bool	InitialiseDevice(void);
	bool 	IsDeviceInitialised(actionResult_t *InitResult);

	bool 	SendNewPreDefinedLowLevelSequence(SequenceConfig_t *SequenceConfig);
    bool 	GetLastSequenceResult(uint16_t *PulseErrors, bool *SequenceWasComplete);

	bool 	DeInitialiseDevice(bool doPrintInfos, bool doPrintStats);

	bool 	ReadAcks(void);
    bool 	DoDeviceReset(void);

private:
    enum printMessageType_t {
    	printMSG_general		= 1,
		printMSG_warning		= 2,
		printMSG_error			= 3,
		printMSG_rmpDeviceInfo,
		printMSG_rmpInitInfo,
		printMSG_rmpInitParam,
		printMSG_rmpSendCMD,
		printMSG_rmpReceiveACK,
		printMSG_rmpPulseConfig,
		printMSG_rmpErrorUnclaimedSequence,
		printMSG_rmpWarningCorrectionChargeInbalace,
		printMSG_rmpSequenceError,
		printMSG_rmpStats
    };

    //global parameter
	bool ClassInstanceInitialised;
	Smpt_device Device;
	char DeviceIDClass[100];
	char DeviceFileName[255];

	struct rmpStatus_t {
		// General
		bool DeviceIsOpen;
		bool DeviceInitialised;
		bool DeviceLlIsInitialised;
		bool DeviceMlIsInitialised;
		bool InitThreatRunning;
		bool InitThreatActive;
		bool ReceiverThreatRunning;
		bool ReceiverThreatActive;
		uint8_t LocalPackageNumber;
		uint64_t StartTime_ms;
		uint64_t CurrentTime_ms;
		uint64_t LastUpdated;
		bool DoNotStimulate;
		uint16_t NumberOfStimErrors;
		bool DoReTestTheStimError;
		uint16_t NumberOfSequencesUntilErrorRetest;

		struct rmpDeviceStatus_t {
			// General
			char DeviceID[Smpt_Length_Device_Id+1];
			Smpt_uc_version MainVersion;
			Smpt_uc_version StimVersion;
			Smpt_Main_Status  MainStatus;
			Smpt_Stim_Status  StimStatus;
			uint8_t	BatteryLevel;
			float	BatteryVoltage;
			// Demultiplexer
			char DemuxID[Smpt_Length_Demux_Id];
			// LowLevel
			uint8_t HighVoltageLevel;
			uint8_t HighVoltageVoltage;
		} Device;
	} rmpStatus;

	rmpInitSettings_t 	rmpInitSettings;
	struct rmpSettings_t {
		// Stimulation
		uint8_t  StimFrequency;
		uint16_t MaxPulseWidth;
		float    MaxCurrent;
		uint16_t NumberOfErrorsAfterWhichToAbort;
		uint16_t NumberOfSequencesAfterWhichToRetestForError;
		bool  	 UseThreadForInit;
		bool	 UseThreadForAcks;
		bool 	 DenervationIsUsed;
	}rmpSettings;

	pthread_t       InitThread;
	actionResult_t  rmpInitResult;
	actionResult_t* rmpInitResultExtern;
    pthread_t       ReceiverThread;
    pthread_mutex_t ReadPackage_mutex;
    LowLevelAcks_t 	Acks;
    pthread_mutex_t AcksLock_mutex;
    ResponseQueue_t	ResponseQueue;
    pthread_mutex_t ResponseQueueLock_mutex;
    SequenceQueue_t	SequenceQueue;
    pthread_mutex_t SequenceQueueLock_mutex;

	DeviceStatistic_t 	Stats;
	//private functions
	bool 	 OpenSerial(void);
	bool 	 CloseSerial(void);
	void 	 AbortDeviceInitialisation();

	inline void	 ReadAcksBlocking(void);
	void 	 PutResponse(SingleResponse_t *Response);
	int 	 GetResponse(Smpt_Cmd ExpectedCommand, bool DoIncreaseAckCounter, int MilliSecondsToWait);

	void 	 PutChannelResponseExpectation(uint64_t SequenceNumber, Smpt_Channel Channel, uint8_t PackageNumber);
	void 	 PutChannelResponse(uint8_t PackageNumber, Smpt_Result Result, Smpt_Channel ChannelError);

	bool 	 CheckSupportedVersion(const uint8_t SupportedVersions[][3], Smpt_version *DeviceVersion, bool disablePedanticVersionCheck, bool *printWarning, bool *printError);
	bool 	 CheckChannel(uint8_t ChannelIn);
	uint16_t CheckAndCorrectPulsewidth(int Pulse_Width_IN, bool *Corrected);
	float 	 CheckAndCorrectCurrent(float Current_IN, bool *Corrected);
	uint8_t	 GetMinimalCurrentPulse(uint16_t *PulseWidth, float *Current, double *Charge, uint8_t MaxNumberOfPoints);

	uint8_t  GetPackageNumber(void);
	uint8_t  GetLastPackageNumber(void);
	bool 	 NewStatusUpdateReceived(uint32_t MilliSecondsToWait);
	double 	 GetCurrentTime(void);
	double 	 GetCurrentTime(bool DoUpdate);

	const char*	GetResultString(Smpt_Result Result);
	const char*	GetChannelNameString(Smpt_Channel Channel);
	// Debug
	void printMessage(printMessageType_t type, const char *format, ... );
	void printBits(char *msgString, void const * const ptr, size_t const size);
	void printSupportedVersion(char *msgString, const uint8_t SupportedVersions[][3]);
};

} // namespace

#endif /* REHAMOVE3INTERFACE_SMPT324_H */
