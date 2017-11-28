/*
 *      TU Berlin --- Fachgebiet Regelungssystem
 *      C++ Interface class for the Hasomed GmbH device RehaMove3
 *
 *      Author: Markus Valtin
 *      Copyright © 2017 Markus Valtin <valtin@control.tu-berlin.de>. All rights reserved.
 *
 *      File:           RehaMove3Interface_SMPT324.cpp -> Source file for the C++ interface class.
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


#include <RehaMove3Interface_SMPT32X.hpp>
#include <termios.h>  // TODO remove once the fix for close_serial delay is implemented -> https://ti1.control.tu-berlin.de/redmine/issues/273


namespace nsRehaMove3_SMPT_32X_01 {

/*
 * Definition of the printf function to use by the smpt library
 */
extern "C" {
void PrintLibError(const char* format) {
	//printf(format);
	printf("%s", format);
	printf("\n");
}
}

/* ### #########################################################################
 * THREADS
 * ### #########################################################################
 */
void *InitialisationThreadFunc(void *data)
{
	RehaMove3 *Device = (RehaMove3 *) data;
	Device->InitialiseDevice();
	pthread_exit(NULL);
}

void *ReceiverThreadFunc(void *data)
{
    RehaMove3 *Device = (RehaMove3 *) data;
    Device->ReadAcks();
    pthread_exit(NULL);
}

RehaMove3::RehaMove3(const char *DeviceID, const char *SerialDeviceFile)
{
	/*
	 * Initialise the private variables
	 */
	this->ClassInstanceInitialised = false;
	// Device specific initialisations
	memset(&(this->Device),  0, sizeof(this->Device));
	memset(  this->DeviceIDClass, 0, sizeof(this->DeviceIDClass));
	if ( snprintf(this->DeviceIDClass, sizeof(this->DeviceIDClass), "%s", DeviceID) < 0){ // C++11
		// the device id could not be added, maybe it is to long? -> reset it to "RehaMove3"
		memset(  this->DeviceIDClass, 0, sizeof(this->DeviceIDClass));
		snprintf(this->DeviceIDClass, sizeof(this->DeviceIDClass), "%s", "RehaMove3");
	}
	memset(  this->DeviceFileName, 0, sizeof(this->DeviceFileName));
	if (snprintf(this->DeviceFileName, sizeof(this->DeviceFileName), "%s", SerialDeviceFile) < 0 ){ // C++11
		// the device filename could not be set up correctly, so opening the device will probably fail anyway -> return
		return;
	}
	memset(&this->rmpStatus, 0, sizeof(rmpStatus_t));

	// Setup specific initialisations
	memset(&(this->rmpSettings), 0, sizeof(this->rmpSettings));
	memset(&(this->rmpInitResult), 0, sizeof(this->rmpInitResult));
	this->rmpInitResultExtern = &this->rmpInitResult;
	// Status specific initialisations

	// TODO Global structs init
	// Statistic specific initialisations
	memset(&(this->Stats), 0, sizeof(this->Stats));

	this->InitThread = 0;
	this->ReceiverThread = 0;
	pthread_mutex_init(&this->ReadPackage_mutex, NULL);
	pthread_mutex_init(&this->AcksLock_mutex, NULL);
	pthread_mutex_init(&this->ResponseQueueLock_mutex, NULL);
	pthread_mutex_init(&this->SequenceQueueLock_mutex, NULL);

	// the initialisation of the class instance is done
	this->ClassInstanceInitialised = true; // todo: abragen ob initialisiert

	/*
	 * Set up SMPT print function to use
	 */
	smpt_init_error_callback(&PrintLibError);
}

RehaMove3::~RehaMove3(void) {
	if (this->rmpStatus.DeviceIsOpen) {
		while(this->rmpStatus.InitThreatRunning){
			this->rmpStatus.InitThreatActive  = false;
			usleep(100000); // 100ms
		}
		RehaMove3::printMessage(printMSG_rmpDeviceInfo, "RehaMove3 DEBUG: Closing serial interface\n");
		RehaMove3::CloseSerial();
	}

	pthread_mutex_unlock(&this->ReadPackage_mutex);
	pthread_mutex_destroy(&this->ReadPackage_mutex);
	pthread_mutex_destroy(&this->AcksLock_mutex);
	pthread_mutex_destroy(&this->ResponseQueueLock_mutex);
	pthread_mutex_destroy(&this->SequenceQueueLock_mutex);
}

bool RehaMove3::IsDeviceInitialised(actionResult_t *InitResult) {
	memcpy(InitResult, &this->rmpInitResult, sizeof(this->rmpInitResult));
	return this->rmpStatus.DeviceInitialised;
}


bool RehaMove3::InitialiseRehaMove3(rmpInitSettings_t *InitSetup, actionResult_t *InitResult)
{
	// Check if the class was successfully created
	if (!this->ClassInstanceInitialised){
		RehaMove3::printMessage(printMSG_error, "RehaMove3 FAITAL ERROR: The class instance is already successfully initialised!\n");
		return false;
	}
	// Check if the pointer is valid
	if (InitSetup == NULL) {
		// the pointer is invalid
		RehaMove3::printMessage(printMSG_error, "%s Error: The configuration for the device initialisation is invalid!\n     -> Please check the pointer to the configuration!\n", this->DeviceIDClass);
		return false;
	}
	// Check that the thread is not already running
	if (this->rmpStatus.InitThreatRunning) {
		// thread is running
		RehaMove3::printMessage(printMSG_error, "%s Error: The initialisation thread is already running!\n", this->DeviceIDClass);
		return false;
	}

	// Check the SMPT library version
	Smpt_version SMPT_Version = smpt_library_version();
	bool printWarning = false, printError = false;
	if (!RehaMove3::CheckSupportedVersion(RMP_SupportedVersionsSMPT, &SMPT_Version, false, &printWarning, &printError)){
		char versionString[100] = {};
		if (printWarning && !printError){
			// show a warning about the unsupported firmware version
			RehaMove3::printSupportedVersion(versionString, RMP_SupportedVersionsSMPT);
			RehaMove3::printMessage(printMSG_warning, "%s Warning: The version of the SMPT library is probably not fully supported!\n   -> Supported Versions: %s\n   -> Version of the linked library: %u.%u.%u\n",
					this->DeviceIDClass, versionString, SMPT_Version.major, SMPT_Version.minor, SMPT_Version.revision);
		}
		if (printError){
			// show an error about the unsupported firmware version and stop the initialisation
			RehaMove3::printSupportedVersion(versionString, RMP_SupportedVersionsMain);
			RehaMove3::printMessage(printMSG_error, "%s Warning: The version of the SMPT library is not supported!\n   -> Supported Versions: %s\n   -> Version of the linked library: %u.%u.%u\n   -> The initialisation will be aborted!",
					this->DeviceIDClass, versionString, SMPT_Version.major, SMPT_Version.minor, SMPT_Version.revision);
			return false;
		}
	}

	// Save the init struct
	memcpy(&(this->rmpInitSettings), InitSetup, sizeof(this->rmpInitSettings));

	// update the settings
	this->rmpSettings.StimFrequency 	 = InitSetup->StimConfig.StimFrequency;
	this->rmpSettings.NumberOfErrorsAfterWhichToAbort 			  = InitSetup->StimConfig.ErrorAbortAfter;
	this->rmpSettings.NumberOfSequencesAfterWhichToRetestForError = InitSetup->StimConfig.ErrorRetestAfter;
	this->rmpSettings.MaxPulseWidth 	 = InitSetup->StimConfig.PulseWidthMax;
	this->rmpSettings.MaxCurrent     	 = fabsf(InitSetup->StimConfig.CurrentMax);
	this->rmpSettings.UseThreadForInit   = InitSetup->StimConfig.UseThreadForInit;
	this->rmpSettings.UseThreadForAcks   = InitSetup->StimConfig.UseThreadForAcks;

	// save the current time as offset
	struct timeval time;
	gettimeofday(&time,NULL);
	this->rmpStatus.StartTime_ms = (uint64_t)(time.tv_sec*1000.0) + (uint64_t)(time.tv_usec/1000.0);

	// use a thread for the initialisation?
	bool returnValue = false;
	this->rmpInitResultExtern = InitResult;
	if (this->rmpSettings.UseThreadForInit){
		this->rmpStatus.InitThreatActive = true;
		if (pthread_create(&(this->InitThread), NULL, InitialisationThreadFunc, (void *)this) != 0) {
			RehaMove3::printMessage(printMSG_error, "%s Error: The initialisation threat could not be started:\n     -> %s (%d)\n", this->DeviceIDClass, strerror(errno), errno);
			returnValue = RehaMove3::InitialiseDevice();
			memcpy(InitResult, &this->rmpInitResult, sizeof(this->rmpInitResult));
			return returnValue;
		}
		RehaMove3::printMessage(printMSG_rmpDeviceInfo, "RehaMove3 DEBUG: Starting the initialisation threat was successfully.\n");
	} else {
		this->rmpStatus.InitThreatActive = true;
		returnValue = RehaMove3::InitialiseDevice();
		memcpy(InitResult, &this->rmpInitResult, sizeof(this->rmpInitResult));
		return returnValue;
	}
	return false;
}

bool RehaMove3::InitialiseDevice(void)
{
	if (!this->rmpStatus.InitThreatActive){
		return false;
	}
	this->rmpStatus.InitThreatRunning = true;
	/*
	 * open the device and start the receiver threat
	 */
	if (!RehaMove3::OpenSerial()){
		RehaMove3::printMessage(printMSG_error, "%s Error: The device %s could not be opened!\n     -> Please check that the device exists and is accessible!\n", this->DeviceIDClass, this->DeviceFileName);
		RehaMove3::AbortDeviceInitialisation();
		return false;
	}

	/*
	 * Reset the device
	 */
	bool DoDeviceReset = false;
	uint8_t ResetCounter = 0;
	do {
		if (DoDeviceReset){
			// reset the device only when needed because the reset takes about 10-15 seconds
			if (ResetCounter >= REHAMOVE_MAX_RESETS_INIT){
				RehaMove3::printMessage(printMSG_error, "%s Error: The device was reseted %d times but the initialisation still does fail!\n     -> Initialisation aborted!\n", this->DeviceIDClass, (int)ResetCounter);
				RehaMove3::AbortDeviceInitialisation();
				return false;
			} else {
				RehaMove3::printMessage(printMSG_warning, "\n%s: Executing a device reset ... ", this->DeviceIDClass);
				fflush(stdout);
				if (smpt_send_reset(&(this->Device), RehaMove3::GetPackageNumber())) {
					int ret;
					for (uint8_t i = 0; i<15; i++){
						if ((ret = RehaMove3::GetResponse(Smpt_Cmd_Reset_Ack, true, 1000)) == Smpt_Cmd_Reset_Ack) {
							RehaMove3::printMessage(printMSG_warning, "done.\n");
							break;
						}
						// check if the initialise thread should still be running
						if (!this->rmpStatus.InitThreatActive){
							RehaMove3::AbortDeviceInitialisation();
							return false;
						}
					}
					if (ret != Smpt_Cmd_Reset_Ack ){
						RehaMove3::printMessage(printMSG_error, "   Should be done. The reset acknowledgement is missing!\n");
					}
				} else {
					RehaMove3::printMessage(printMSG_error, "Error\n%s Error: Sending the command %d failed!\n", this->DeviceIDClass, Smpt_Cmd_Reset_Ack);
					RehaMove3::AbortDeviceInitialisation();
					return false;
				}
				DoDeviceReset = false;
				ResetCounter++;
			}
		}

		// check if the initialise thread should still be running
		if (!this->rmpStatus.InitThreatActive){
			RehaMove3::AbortDeviceInitialisation();
			return false;
		}

		/*
		 * General commands
		 */
		// get the device id
		if (smpt_send_get_device_id(&(this->Device), RehaMove3::GetPackageNumber())) {
			if (RehaMove3::GetResponse(Smpt_Cmd_Get_Device_Id_Ack, true, 200) != Smpt_Cmd_Get_Device_Id_Ack) {
				// error
				RehaMove3::printMessage(printMSG_error, "%s Error: The device ID could not be read!\n", this->DeviceIDClass);
				DoDeviceReset = true;
				continue;
			} else {
				// response received -> check the ID
				memcpy(this->rmpStatus.Device.DeviceID, this->Acks.G_device_id_ack.device_id, (size_t)Smpt_Length_Device_Id);
				this->rmpStatus.Device.DeviceID[Smpt_Length_Device_Id] = 0x00;
				if (this->rmpInitSettings.checkDeviceIDs){
					// check if the device IDs match
					if (strcmp(this->rmpInitSettings.RequestedDeviceID, this->rmpStatus.Device.DeviceID) != 0){
						// IDs do not match
						this->rmpInitResult.successful = false;
						this->rmpInitResult.errorCode = actionError_checkDeviceIDs;
						sprintf(this->rmpInitResult.errorMessage, "The serial numbers do not match\n.     -> Requested SN: '%s';  Found SN: '%s'!\n", this->rmpInitSettings.RequestedDeviceID, this->rmpStatus.Device.DeviceID);
						RehaMove3::printMessage(printMSG_error, "%s Error: The serial numbers do not match\n   -> Requested SN: '%s';  Found SN: '%s'!\n", this->DeviceIDClass, this->rmpInitSettings.RequestedDeviceID, this->rmpStatus.Device.DeviceID);
						RehaMove3::AbortDeviceInitialisation();
						return false;
					}
				}
			}
		} else {
			RehaMove3::printMessage(printMSG_error, "%s Error: Sending the command %d failed!\n", this->DeviceIDClass, Smpt_Cmd_Get_Device_Id_Ack);
			RehaMove3::AbortDeviceInitialisation();
			return false;
		}

		// get the current status -> saving the data is done in the response handlers
		if (smpt_send_get_battery_status(&(this->Device), RehaMove3::GetPackageNumber())) {
			if (!RehaMove3::NewStatusUpdateReceived(200)){
				// error
				RehaMove3::printMessage(printMSG_error, "%s Error: The current device status could net be read!\n", this->DeviceIDClass);
				DoDeviceReset = true;
				continue;
			}
		} else {
			RehaMove3::printMessage(printMSG_error, "%s Error: Sending the command %d failed!\n", this->DeviceIDClass, Smpt_Cmd_Get_Battery_Status);
			RehaMove3::AbortDeviceInitialisation();
			return false;
		}

		bool printWarning = false, printError = false;
		// get the main version
		if (smpt_send_get_version_main(&(this->Device), RehaMove3::GetPackageNumber())) {
			if (RehaMove3::GetResponse(Smpt_Cmd_Get_Version_Main_Ack, true, 500) != Smpt_Cmd_Get_Version_Main_Ack) {
				// error
				RehaMove3::printMessage(printMSG_error, "%s Error: The version of the 'Main MCU' could net be read!\n", this->DeviceIDClass);
				DoDeviceReset = true;
				continue;
			} else {
				// response received -> get the firmware version
				memcpy(&(this->rmpStatus.Device.MainVersion), &(this->Acks.G_version_ack.uc_version), sizeof(Smpt_uc_version));
				// test if the device version is supported
				// main firmware version
				if (!RehaMove3::CheckSupportedVersion(RMP_SupportedVersionsMain, &this->rmpStatus.Device.MainVersion.fw_version, this->rmpInitSettings.DebugConfig.disableVersionCheck, &printWarning, &printError)){
					char versionString[100] = {};
					if (printWarning && !printError){
						// show a warning about the unsupported firmware version
						RehaMove3::printSupportedVersion(versionString, RMP_SupportedVersionsMain);
						RehaMove3::printMessage(printMSG_warning, "%s Warning: The firmware version of the 'Main MCU' is probably not fully supported!\n   -> Supported Versions: %s\n   -> Version of the device: %u.%u.%u\n",
								this->DeviceIDClass, versionString, this->rmpStatus.Device.MainVersion.fw_version.major, this->rmpStatus.Device.MainVersion.fw_version.minor, this->rmpStatus.Device.MainVersion.fw_version.revision);
					}
					if (printError){
						// show an error about the unsupported firmware version and stop the initialisation
						RehaMove3::printSupportedVersion(versionString, RMP_SupportedVersionsMain);
						RehaMove3::printMessage(printMSG_error, "%s Error: The firmware version of the 'Main MCU' is not supported!\n   -> Supported Versions: %s\n   -> Version of the device: %u.%u.%u\n\n   -> The initialisation will be aborted!",
								this->DeviceIDClass, versionString, this->rmpStatus.Device.MainVersion.fw_version.major, this->rmpStatus.Device.MainVersion.fw_version.minor, this->rmpStatus.Device.MainVersion.fw_version.revision);
						RehaMove3::AbortDeviceInitialisation();
						return false;
					}
				}
				// main SMPT version
				if (!RehaMove3::CheckSupportedVersion(RMP_SupportedVersionsSMPT, &this->rmpStatus.Device.MainVersion.smpt_version, this->rmpInitSettings.DebugConfig.disableVersionCheck, &printWarning, &printError)){
					char versionString[100] = {};
					if (printWarning && !printError){
						// show a warning about the unsupported SMPT version
						char versionString[100] = {};
						RehaMove3::printSupportedVersion(versionString, RMP_SupportedVersionsSMPT);
						RehaMove3::printMessage(printMSG_warning, "%s Warning: The SMPT version of the 'Main MCU' is probably not fully supported!\n   -> Supported Versions: %s\n   -> Version of the device: %u.%u.%u\n",
								this->DeviceIDClass, versionString, this->rmpStatus.Device.MainVersion.smpt_version.major, this->rmpStatus.Device.MainVersion.smpt_version.minor, this->rmpStatus.Device.MainVersion.smpt_version.revision);
					}
					if (printError){
						// show an error about the unsupported SMPT version and stop the initialisation
						RehaMove3::printSupportedVersion(versionString, RMP_SupportedVersionsSMPT);
						RehaMove3::printMessage(printMSG_error, "%s Error: The SMPT version of the 'Main MCU' is not supported!\n   -> Supported Versions: %s\n   -> Version of the device: %u.%u.%u\n\n   -> The initialisation will be aborted!",
								this->DeviceIDClass, versionString, this->rmpStatus.Device.MainVersion.smpt_version.major, this->rmpStatus.Device.MainVersion.smpt_version.minor, this->rmpStatus.Device.MainVersion.smpt_version.revision);
						RehaMove3::AbortDeviceInitialisation();
						return false;
					}
				}
			}
		} else {
			RehaMove3::printMessage(printMSG_error, "%s Error: Sending the command %d failed!\n", this->DeviceIDClass, Smpt_Cmd_Get_Version_Main_Ack);
			RehaMove3::AbortDeviceInitialisation();
			return false;
		}

		// get the stim version
		if (smpt_send_get_version_stim(&(this->Device), RehaMove3::GetPackageNumber())) {
			if (RehaMove3::GetResponse(Smpt_Cmd_Get_Version_Stim_Ack, true, 500) != Smpt_Cmd_Get_Version_Stim_Ack) {
				// error
				RehaMove3::printMessage(printMSG_error, "%s Error: The version of the 'Stim MCU' could net be read!\n", this->DeviceIDClass);
				DoDeviceReset = true;
				continue;
			} else {
				// response received -> get the firmware version
				memcpy(&(this->rmpStatus.Device.StimVersion), &(this->Acks.G_version_ack.uc_version), sizeof(Smpt_uc_version));
				// test if the device version is supported
				// stim firmware version
				if (!RehaMove3::CheckSupportedVersion(RMP_SupportedVersionsStim, &this->rmpStatus.Device.StimVersion.fw_version, this->rmpInitSettings.DebugConfig.disableVersionCheck, &printWarning, &printError)){
					char versionString[100] = {};
					if (printWarning && !printError){
						RehaMove3::printSupportedVersion(versionString, RMP_SupportedVersionsStim);
						// show a warning about the unsupported firmware version
						RehaMove3::printMessage(printMSG_warning, "%s Warning: The firmware version of the 'Stim MCU' is probably not fully supported!\n   -> Supported Versions: %s\n   -> Version of the device: %u.%u.%u\n",
								this->DeviceIDClass, versionString, this->rmpStatus.Device.StimVersion.fw_version.major, this->rmpStatus.Device.StimVersion.fw_version.minor, this->rmpStatus.Device.StimVersion.fw_version.revision);
					}
					if (printError){
						RehaMove3::printSupportedVersion(versionString, RMP_SupportedVersionsStim);
						// show an error about the unsupported firmware version and stop the initialisation
						RehaMove3::printMessage(printMSG_error, "%s Error: The firmware version of the 'Stim MCU' is not supported!\n   -> Supported Versions: %s\n   -> Version of the device: %u.%u.%u\n\n   -> The initialisation will be aborted!",
								this->DeviceIDClass, versionString, this->rmpStatus.Device.StimVersion.fw_version.major, this->rmpStatus.Device.StimVersion.fw_version.minor, this->rmpStatus.Device.StimVersion.fw_version.revision);
						RehaMove3::AbortDeviceInitialisation();
						return false;
					}
				}
				// stim SMPT version
				if (!RehaMove3::CheckSupportedVersion(RMP_SupportedVersionsSMPT, &this->rmpStatus.Device.StimVersion.smpt_version, this->rmpInitSettings.DebugConfig.disableVersionCheck, &printWarning, &printError)){
					char versionString[100] = {};
					if (printWarning && !printError){
						RehaMove3::printSupportedVersion(versionString, RMP_SupportedVersionsSMPT);
						// show a warning about the unsupported SMPT version
						RehaMove3::printMessage(printMSG_warning, "%s Warning: The SMPT version of the 'Stim MCU' is probably not fully supported!\n   -> Supported Versions: %s\n   -> Version of the device: %u.%u.%u\n",
								this->DeviceIDClass, versionString, this->rmpStatus.Device.StimVersion.smpt_version.major, this->rmpStatus.Device.StimVersion.smpt_version.minor, this->rmpStatus.Device.StimVersion.smpt_version.revision);
					}
					if (printError){
						RehaMove3::printSupportedVersion(versionString, RMP_SupportedVersionsSMPT);
						// show an error about the unsupported SMPT version and stop the initialisation
						RehaMove3::printMessage(printMSG_error, "%s Error: The SMPT version of the 'Stim MCU' is not supported!\n   -> Supported Versions: %s\n   -> Version of the device: %u.%u.%u\n\n   -> The initialisation will be aborted!",
								this->DeviceIDClass, versionString, this->rmpStatus.Device.StimVersion.smpt_version.major, this->rmpStatus.Device.StimVersion.smpt_version.minor, this->rmpStatus.Device.StimVersion.smpt_version.revision);
						RehaMove3::AbortDeviceInitialisation();
						return false;
					}
				}
			}
		} else {
			RehaMove3::printMessage(printMSG_error, "%s Error: Sending the command %d failed!\n", this->DeviceIDClass, Smpt_Cmd_Get_Version_Stim_Ack);
			RehaMove3::AbortDeviceInitialisation();
			return false;
		}
		// print the current status, in case something goes wrong
		RehaMove3::printMessage(printMSG_rmpDeviceInfo, "%s: The stimulator '%s' was found on interface '%s'\n   -> Versions: M[%u.%u.%u|%u.%u.%u] / S[%u.%u.%u|%u.%u.%u]\n   -> Battery Voltage: %u%% (%0.2fV)\n",
				this->DeviceIDClass, this->rmpStatus.Device.DeviceID, this->DeviceFileName,
				this->rmpStatus.Device.MainVersion.smpt_version.major, this->rmpStatus.Device.MainVersion.smpt_version.minor, this->rmpStatus.Device.MainVersion.smpt_version.revision,
				this->rmpStatus.Device.MainVersion.fw_version.major, this->rmpStatus.Device.MainVersion.fw_version.minor, this->rmpStatus.Device.MainVersion.fw_version.revision,
				this->rmpStatus.Device.StimVersion.smpt_version.major, this->rmpStatus.Device.StimVersion.smpt_version.minor, this->rmpStatus.Device.StimVersion.smpt_version.revision,
				this->rmpStatus.Device.StimVersion.fw_version.major, this->rmpStatus.Device.StimVersion.fw_version.minor, this->rmpStatus.Device.StimVersion.fw_version.revision,
				this->rmpStatus.Device.BatteryLevel, this->rmpStatus.Device.BatteryVoltage);

		/*
		 * LowLevel commands
		 */

		// LowLevle initialisation
		Smpt_ll_init ll_init = {0};
		smpt_clear_ll_init(&ll_init);

		if (this->rmpInitSettings.LowLevelConfig.UseDenervation) {
			RehaMove3::printMessage(printMSG_error, "%s Error: Stimulation of denerved muscles is not supported yet!\n", this->DeviceIDClass);
			//this->Setup.DenervationIsUsed = true;
			//ll_init.enable_denervation = 1;
			this->rmpSettings.DenervationIsUsed = false; // TODO: add denervation feature if available
			ll_init.enable_denervation = 0;
		} else {
			this->rmpSettings.DenervationIsUsed = false;
			ll_init.enable_denervation = 0;
		}

		// set voltage level
		ll_init.high_voltage_level = (Smpt_High_Voltage)this->rmpInitSettings.LowLevelConfig.HighVoltageLevel;

		// package number
		ll_init.packet_number = RehaMove3::GetPackageNumber();

		// check the configuration
		if (smpt_is_valid_ll_init(&ll_init)) {
			// init configuration is valid
			RehaMove3::printMessage(printMSG_rmpInitParam, "%s DEBUG: Initialising the LowLevel Protocol\n", this->DeviceIDClass);
			RehaMove3::printMessage(printMSG_rmpInitParam, "     -> Denervation used: %s\n", (this->rmpSettings.DenervationIsUsed ? "yes":"no"));

			// Send the ll_init command to the stimulator
			if (smpt_send_ll_init(&(this->Device), &ll_init)) {
				if (RehaMove3::GetResponse(Smpt_Cmd_Ll_Init_Ack, true, 500) != Smpt_Cmd_Ll_Init_Ack) {
					// error
					RehaMove3::printMessage(printMSG_error, "%s Error: The device could not be initialised! The LL_Init acknowledgement is missing!\n", this->DeviceIDClass);
					DoDeviceReset = true;
					continue;
				} else {
			        // update the internal status
			        this->rmpStatus.DeviceLlIsInitialised = true;
			    	// lock the sequence queue
			    	pthread_mutex_lock(&(this->SequenceQueueLock_mutex));
			    	// show the warning about a full queue only one -> reset the value to true
			    	this->SequenceQueue.DoNotReportUnclaimedSequenceResults = true;
			    	// unlock the sequence queue
			    	pthread_mutex_unlock(&(this->SequenceQueueLock_mutex));
					RehaMove3::printMessage(printMSG_rmpInitParam, "     -> SUCCESSFUL initialised\n");
				}
			} else {
				RehaMove3::printMessage(printMSG_error, "%s Error: Sending the command %d failed!\n", this->DeviceIDClass, Smpt_Cmd_Ll_Init);
				RehaMove3::AbortDeviceInitialisation();
				return false;
			}
		} else {
			// init configuration is invalid
			RehaMove3::printMessage(printMSG_error, "%s Error: The stimulator could not be initialised since the initial parameter structure of the low level protocol is invalid!\n", this->DeviceIDClass);
			RehaMove3::AbortDeviceInitialisation();
			return false;
		}

		/*
		 * Checks
		 */
		// get the current stim status -> saving the data is done in the response handlers
		if (smpt_send_get_stim_status(&(this->Device), RehaMove3::GetPackageNumber())) {
			if (RehaMove3::NewStatusUpdateReceived(200)){
				// TODO Init STIM check: low level initialised, high voltage Level
			} else {
				RehaMove3::printMessage(printMSG_error, "%s Error: The current 'STIM Status' could net be read!\n", this->DeviceIDClass);
				DoDeviceReset = true;
				continue;
			}
		} else {
			RehaMove3::printMessage(printMSG_error, "%s Error: Sending the command %d failed!\n", this->DeviceIDClass, Smpt_Cmd_Get_Stim_Status);
			RehaMove3::AbortDeviceInitialisation();
			return false;
		}

		// get the current main status -> saving the data is done in the response handlers
		if (smpt_send_get_main_status(&(this->Device), RehaMove3::GetPackageNumber())) {
			if (RehaMove3::NewStatusUpdateReceived(200)){
				// TODO Init MAIN check: DataMeasurment initialised
			} else {
				RehaMove3::printMessage(printMSG_error, "%s Error: The current 'MAIN Status' could net be read!\n", this->DeviceIDClass);
				DoDeviceReset = true;
				continue;
			}
		} else {
			RehaMove3::printMessage(printMSG_error, "%s Error: Sending the command %d failed!\n", this->DeviceIDClass, Smpt_Cmd_Get_Main_Status);
			RehaMove3::AbortDeviceInitialisation();
			return false;
		}

		// end the initialise loop
	} while (DoDeviceReset);

	// print the status
	RehaMove3::printMessage(printMSG_rmpInitInfo, "%s: Initialising %s was successful!\n", this->DeviceIDClass, this->DeviceFileName);
	if (this->rmpInitSettings.DebugConfig.printInitInfos){
		RehaMove3::GetCurrentStatus(this->rmpInitSettings.DebugConfig.printInitInfos, false, 0);
	}

	// the initialisation was not aborted, so it was successful
	this->rmpStatus.DeviceInitialised = true;
	this->rmpStatus.InitThreatRunning = false;
	this->rmpStatus.InitThreatActive  = false;

	this->rmpInitResult.finished = true;
	this->rmpInitResult.successful = true;
	memcpy(this->rmpInitResultExtern, &this->rmpInitResult, sizeof(this->rmpInitResult));
	return true;
}

void RehaMove3::AbortDeviceInitialisation()
{
	this->rmpStatus.InitThreatRunning = false;
	RehaMove3::CloseSerial();
	this->rmpInitResult.finished = true;
	this->rmpInitResult.successful = false;
	memcpy(this->rmpInitResultExtern, &this->rmpInitResult, sizeof(this->rmpInitResult));
}

bool RehaMove3::SendNewPreDefinedLowLevelSequence(SequenceConfig_t *SequenceConfig)
{
	// make sure the device is initialised
	if (!this->rmpStatus.DeviceInitialised || !this->rmpStatus.DeviceLlIsInitialised){
		return false;
	}

	bool	 OneOrMorePulsesSend = false, WasCorrected = false;
	uint8_t  NumberOfPoints = 0, iPoint = 0;
	uint16_t PulseWidth[REHAMOVE_SHAPES__NUMBER_OF_POINTS_MAX] = {}, PWStepSize = 0, tempPW = 0;
	float 	 Current[REHAMOVE_SHAPES__NUMBER_OF_POINTS_MAX] = {}, CurrentSign = 0, tempI = 0, CurrentStepSize = 0.0;
	double 	 Charge = 0.0, ChargeOverAll = 0.0;

	// Struct for Ll_channel_config command
	Smpt_ll_channel_config 	ll_channel_config;

	// Debug output
	if (this->rmpInitSettings.DebugConfig.printStimInfos){
		printf("\n%s Puls Info: time=%0.3f\n", this->DeviceIDClass, RehaMove3::GetCurrentTime());
	}

	/*
	 * Handling StimulationErrors e.g. electrode errors
	 */
	if (this->rmpStatus.DoNotStimulate){
		if ( this->rmpStatus.DoReTestTheStimError){
			this->rmpStatus.NumberOfSequencesUntilErrorRetest--;
			if (this->rmpStatus.NumberOfSequencesUntilErrorRetest != 0){
				// do not re-test for the errors yet
				this->Stats.SequencesSend++;
				this->Stats.SequencesFailed++;
				return false;
			} else {
				// do re-test for the errors ....
				this->rmpStatus.DoReTestTheStimError = false;
				this->rmpStatus.DoNotStimulate = false;
			}
		} else {
			// do not re-test for the errors ever
			this->Stats.SequencesSend++;
			this->Stats.SequencesFailed++;
			return false;
		}
	}
	/*
	 *  Loop through the pulses of one sequence
	 */
	for (uint8_t i_Puls = 0; i_Puls < SequenceConfig->NumberOfPulses; i_Puls++){
		// Clear Ll_channel_config and set the data
		smpt_clear_ll_channel_config(&ll_channel_config);

		if (SequenceConfig->PulseConfig[i_Puls].PulseWidth == 0){
			// the pulse width is 0 -> skip this pulse
			continue;
		}

		// input checks/corrections -> make sure the stimulation does not exceed the stimlation boundaries
		if (!RehaMove3::CheckChannel(SequenceConfig->PulseConfig[i_Puls].Channel)) {
			// error: channel invalid
			RehaMove3::printMessage(printMSG_rmpSequenceError, "%s Error: The requested channel id %u is invalid! (time: %0.3f; pulse: %u)\n", this->DeviceIDClass, SequenceConfig->PulseConfig[i_Puls].Channel, RehaMove3::GetCurrentTime(), i_Puls);
			continue;
		}
		WasCorrected = false;
		tempPW = SequenceConfig->PulseConfig[i_Puls].PulseWidth;
		SequenceConfig->PulseConfig[i_Puls].PulseWidth = CheckAndCorrectPulsewidth(SequenceConfig->PulseConfig[i_Puls].PulseWidth, &WasCorrected);
		if (WasCorrected){
			RehaMove3::printMessage(printMSG_rmpWarningCorrectionChargeInbalace, "%s Input Correction (time: %0.3f; pulse %u):\n   -> The pulse width was adjusted from %u to %u\n",
					this->DeviceIDClass, RehaMove3::GetCurrentTime(), i_Puls+1, tempPW, SequenceConfig->PulseConfig[i_Puls].PulseWidth);
		}
		WasCorrected = false;
		tempI = SequenceConfig->PulseConfig[i_Puls].Current;
		SequenceConfig->PulseConfig[i_Puls].Current = (float)CheckAndCorrectCurrent(SequenceConfig->PulseConfig[i_Puls].Current, &WasCorrected);
		if (WasCorrected){
			RehaMove3::printMessage(printMSG_rmpWarningCorrectionChargeInbalace, "%s Input Correction (time: %0.3f; pulse %u):\n   -> The current was adjusted from %+0.2f to %+0.2f\n",
					this->DeviceIDClass, RehaMove3::GetCurrentTime(), i_Puls+1, tempI, SequenceConfig->PulseConfig[i_Puls].Current);
		}

		// build point list
		switch (SequenceConfig->PulseConfig[i_Puls].Shape) {
		case Shape_Balanced_Symetric_Biphasic:
		case Shape_Balanced_Symetric_Biphasic_NEGATIVE:
			if (SequenceConfig->PulseConfig[i_Puls].Shape == Shape_Balanced_Symetric_Biphasic_NEGATIVE){
				// handle the negative case
				SequenceConfig->PulseConfig[i_Puls].Current = -1.0 *fabsf(SequenceConfig->PulseConfig[i_Puls].Current);
			}
			iPoint = 0;
			PulseWidth[iPoint] = SequenceConfig->PulseConfig[i_Puls].PulseWidth;    	// positive pulse
			Current[iPoint++] = SequenceConfig->PulseConfig[i_Puls].Current;
			PulseWidth[iPoint] = 100;                                				// 100us break
			Current[iPoint++] = 0.0;
			PulseWidth[iPoint] = SequenceConfig->PulseConfig[i_Puls].PulseWidth;    	// negative pulse
			Current[iPoint++] = -1.0 * SequenceConfig->PulseConfig[i_Puls].Current;
			ChargeOverAll += 0;
			NumberOfPoints = iPoint;
			break;

		case Shape_Balanced_UNsymetric_Biphasic:
		case Shape_Balanced_UNsymetric_Biphasic_NEGATIVE:
			if (SequenceConfig->PulseConfig[i_Puls].Shape == Shape_Balanced_UNsymetric_Biphasic_NEGATIVE){
				// handle the negative case
				SequenceConfig->PulseConfig[i_Puls].Current = -1.0 *fabsf(SequenceConfig->PulseConfig[i_Puls].Current);
			}
			iPoint = 0;
			PulseWidth[iPoint]  = SequenceConfig->PulseConfig[i_Puls].PulseWidth;    	// positive pulse
			Current[iPoint]     = SequenceConfig->PulseConfig[i_Puls].Current;
			Charge         		= (double)PulseWidth[iPoint] *Current[iPoint];
			CurrentSign         = (Current[iPoint] < 0) ? -1.0 : 1.0;
			iPoint++;
			PulseWidth[iPoint]  = 100;                                					// 100us break
			Current[iPoint++]   = 0.0;
			iPoint += RehaMove3::GetMinimalCurrentPulse(&PulseWidth[iPoint], &Current[iPoint], &Charge, 1); // negative pulse
			ChargeOverAll += Charge;
			NumberOfPoints = iPoint;
			break;

		case Shape_UNbalanced_UNsymetric_Monophasic:
		case Shape_UNbalanced_UNsymetric_Monophasic_NEGATIVE:
			if (SequenceConfig->PulseConfig[i_Puls].Shape == Shape_UNbalanced_UNsymetric_Monophasic_NEGATIVE){
				// handle the negative case
				SequenceConfig->PulseConfig[i_Puls].Current = -1.0 *fabsf(SequenceConfig->PulseConfig[i_Puls].Current);
			}
			iPoint = 0;
			PulseWidth[iPoint] = SequenceConfig->PulseConfig[i_Puls].PulseWidth;     // first pulse
			Current[iPoint]    = SequenceConfig->PulseConfig[i_Puls].Current;
			ChargeOverAll += PulseWidth[iPoint] * Current[iPoint];
			iPoint++;
			NumberOfPoints = iPoint;
			break;

		case Shape_UNbalanced_UNsymetric_Biphasic_FIRST:
		case Shape_Balanced_UNsymetric_Biphasic_FIRST:
		case Shape_Balanced_UNsymetric_LONG_Biphasic_FIRST:
			// 6 -> erster  Teil eines biphasischen, UNgleichmässigen und UNausgeglichenen Pulses, Polarität wird durch den Strom bestimmt
			// does the secound pulse exist?
			if ( SequenceConfig->PulseConfig[i_Puls +1].Shape == Shape_UNbalanced_UNsymetric_Biphasic_SECOUND ||
				 SequenceConfig->PulseConfig[i_Puls +1].Shape == Shape_Balanced_UNsymetric_Biphasic_SECOUND   ||
				 SequenceConfig->PulseConfig[i_Puls +1].Shape == Shape_Balanced_UNsymetric_LONG_Biphasic_SECOUND) {
				iPoint = 0;
				PulseWidth[iPoint] = SequenceConfig->PulseConfig[i_Puls].PulseWidth;   	// first pulse
				Current[iPoint]    = SequenceConfig->PulseConfig[i_Puls].Current;
				CurrentSign        = (Current[iPoint] < 0) ? -1.0 : 1.0;
				Charge = PulseWidth[iPoint] *Current[iPoint];
				iPoint++;
				PulseWidth[iPoint] = 100;                              					// 100us break
				Current[iPoint++]  = 0.0;
				PulseWidth[iPoint] = SequenceConfig->PulseConfig[i_Puls +1].PulseWidth; // second pulse
				Current[iPoint]    = -1.0 *CurrentSign	*fabsf(SequenceConfig->PulseConfig[i_Puls +1].Current);
				Charge = PulseWidth[iPoint] *Current[iPoint];
				iPoint++;
				// charge compensation
				if (SequenceConfig->PulseConfig[i_Puls].Shape == Shape_Balanced_UNsymetric_Biphasic_FIRST) {
					iPoint += RehaMove3::GetMinimalCurrentPulse(&PulseWidth[iPoint], &Current[iPoint], &Charge, 1);
				} else if (SequenceConfig->PulseConfig[i_Puls].Shape == Shape_Balanced_UNsymetric_LONG_Biphasic_FIRST) {
					iPoint += RehaMove3::GetMinimalCurrentPulse(&PulseWidth[iPoint], &Current[iPoint], &Charge, 7);
				}
				ChargeOverAll += Charge;
				NumberOfPoints = iPoint;
			} else {
				RehaMove3::printMessage(printMSG_rmpSequenceError, "%s Error: The second half of an (UN)Balanced, UNsymmetric, biphasic pulse was not defined!\n     -> Puls %u is discarded!\n!\n", this->DeviceIDClass, i_Puls);
				NumberOfPoints = 0;
				continue;

			}
			break;

		case Shape_UNbalanced_UNsymetric_Biphasic_SECOUND:
		case Shape_Balanced_UNsymetric_Biphasic_SECOUND:
		case Shape_Balanced_UNsymetric_LONG_Biphasic_SECOUND:
			// 7 -> zweiter Teil eines biphasischen, UNgleichmässigen und UNausgeglichenen Pulses, Polarität entgegengesetzt zum ersten Puls
			// the configuration was already used -> skip this SequenceConfig->PulseConfiguration
			NumberOfPoints = 0;
			continue;
			break;

		case Shape_Balanced_UNsymetric_RisingTriangle:
		case Shape_Balanced_UNsymetric_FallingTriangle:
		case Shape_UNbalanced_UNsymetric_RisingTriangle:
		case Shape_UNbalanced_UNsymetric_FallingTriangle:
			{
			// 12 -> dreieckiger UNgleichmässigen und ausgeglichenen Pulses, Polarität wird durch den Strom bestimmt; Dreieck in der ersten Flanke
			// Puls Breite
			uint8_t tempNumberOfPoints = 0;
			if (SequenceConfig->PulseConfig[i_Puls].Shape == Shape_Balanced_UNsymetric_RisingTriangle ||
				SequenceConfig->PulseConfig[i_Puls].Shape == Shape_Balanced_UNsymetric_FallingTriangle){
				tempNumberOfPoints = REHAMOVE_SHAPES__TRIAGLE_MAX_POINTS_BI;
			} else {
				tempNumberOfPoints = REHAMOVE_SHAPES__TRIAGLE_MAX_POINTS_MONO;
			}
			bool WasCorrected;
			iPoint = 0;
			Charge = 0.0;

			// NumberOfPoints für den Rechtwinkligen Teil wenn wir dir minimale Breite deines Punktes annehmen
			NumberOfPoints = (uint8_t) floor(SequenceConfig->PulseConfig[i_Puls].PulseWidth / REHAMOVE_SHAPES__PW_MIN);
			if (NumberOfPoints == 0){
				// no steps -> this pulse is no executed
				continue;
			}
			// Falls bei minimaler Breite mehr Punkte als REHAMOVE__TRIAGLE_MAX_POINTS (14?) berechnet wurden, werden nur REHAMOVE__TRIAGLE_MAX_POINTS verwendet und dafür die Pulsbreite vergrößert
			NumberOfPoints = (NumberOfPoints > tempNumberOfPoints) ? tempNumberOfPoints : NumberOfPoints;
			// Höhe der Stromstufen
			CurrentStepSize = RehaMove3::CheckAndCorrectCurrent( fabsf(SequenceConfig->PulseConfig[i_Puls].Current / NumberOfPoints), &WasCorrected);
			if (CurrentStepSize == 0.0){
				// current step is two small -> this pulse is no executed
				continue;
			}
			NumberOfPoints = (uint8_t)roundf(SequenceConfig->PulseConfig[i_Puls].Current/CurrentStepSize);
			NumberOfPoints = (NumberOfPoints > tempNumberOfPoints) ? tempNumberOfPoints : NumberOfPoints;
			// Berechne die Pulsbreite einer Stufe des Dreiecks (Abrunden und dann den ersten Punkt länger machen um genau auf PW_Soll zu kommen)
			PWStepSize = floor(SequenceConfig->PulseConfig[i_Puls].PulseWidth / NumberOfPoints);
			if (REHAMOVE_SHAPES__TRIAGLE_USE_FIXED_PW_STEP) {
				// ggf. feste Breite verwenden
				PWStepSize = (PWStepSize > REHAMOVE_SHAPES__TRIAGLE_PW_STEP) ? REHAMOVE_SHAPES__TRIAGLE_PW_STEP : PWStepSize;
			}
			PWStepSize = (PWStepSize < REHAMOVE_SHAPES__PW_MIN) ? REHAMOVE_SHAPES__PW_MIN : PWStepSize;

			CurrentSign    = (SequenceConfig->PulseConfig[i_Puls].Current < 0) ? -1.0 : 1.0;
			tempPW = 0;
			// calculate the steps of the triangle
			// point 0
			PulseWidth[0] = PWStepSize;
			tempPW += PWStepSize;
			Current[0] = (float)CurrentStepSize *CurrentSign;
			Charge += (double)PulseWidth[0] *Current[0];
			// point 1 - N
			for (iPoint = 1; iPoint < NumberOfPoints; iPoint++) {
				PulseWidth[iPoint] = PWStepSize;
				tempPW += PWStepSize;
				Current[iPoint] = Current[iPoint -1] +((float)CurrentStepSize *CurrentSign);
				Charge += (double)PulseWidth[iPoint] *Current[iPoint];
			}
			// last step of the triangle
			int tempPW2 = PulseWidth[iPoint -1] + SequenceConfig->PulseConfig[i_Puls].PulseWidth -tempPW;
			PulseWidth[iPoint -1] = (tempPW2 > 0) ? (uint16_t)tempPW2 : 0;
			Current[iPoint -1] = SequenceConfig->PulseConfig[i_Puls].Current;

			// triangle in first or second flank ?
			if (SequenceConfig->PulseConfig[i_Puls].Shape == Shape_Balanced_UNsymetric_FallingTriangle ||
				SequenceConfig->PulseConfig[i_Puls].Shape == Shape_UNbalanced_UNsymetric_FallingTriangle){
				// second flank -> reverse the order
				uint16_t PWtemp[REHAMOVE_SHAPES__NUMBER_OF_POINTS_MAX] = {};
				float 	 Itemp[REHAMOVE_SHAPES__NUMBER_OF_POINTS_MAX] = {};
				for (uint8_t iTemp = 0; iTemp < NumberOfPoints; iTemp++) {
					PWtemp[iTemp] = PulseWidth[NumberOfPoints -1 -iTemp];
					Itemp[iTemp]  = Current[NumberOfPoints -1 -iTemp];
				}
				for (uint8_t iTemp = 0; iTemp < NumberOfPoints; iTemp++) {
					PulseWidth[iTemp] = PWtemp[iTemp];
					Current[iTemp]    = Itemp[iTemp];
				}
			}

			// charge balance
			if (SequenceConfig->PulseConfig[i_Puls].Shape == Shape_Balanced_UNsymetric_RisingTriangle ||
				SequenceConfig->PulseConfig[i_Puls].Shape == Shape_Balanced_UNsymetric_FallingTriangle){
				// 100us break
				PulseWidth[iPoint] = 100;                             // 100us break
				Current[iPoint++]  = 0.0;
				// charge balance pulse
				iPoint += RehaMove3::GetMinimalCurrentPulse(&PulseWidth[iPoint], &Current[iPoint], &Charge, 1);
			}
			// done
			ChargeOverAll += Charge;
			NumberOfPoints = iPoint;
			break;}

/*
// TODO: Nur für einen Kanal ausgleichen
		case Shape_UNbalanced_Charge_Compensation:
			// 20 -> Kompensataionspuls um den Ladungsausgleich herzustellen
			if (fabs(ChargeOverAll) >= REHAMOVE__PW_MIN * REHAMOVE__I_MIN) {
				NumberOfPoints = 3;
				if (ChargeOverAll <= 0) {
					CurrentSign = 1.0;
				} else {
					CurrentSign = -1.0;
				}
				for (NumberOfPoints = 0;
						NumberOfPoints < REHAMOVE__MAX_POINTS_COMPENSATION_PULS;
						NumberOfPoints++) {
					Current[NumberOfPoints] =
							(REHAMOVE__I_STEP_COMPENSATION_PULS
									* (NumberOfPoints + 1)) * CurrentSign;
					PulseWidth[NumberOfPoints] = (int) (fabs(ChargeOverAll)
							/ (REHAMOVE__I_STEP_COMPENSATION_PULS
									* (NumberOfPoints + 1)));
					if (PulseWidth[NumberOfPoints]
							> REHAMOVE__PW_COMPENSATION_PULS) {
						// set the current to the minimum and change the pulse width accordingly
						PulseWidth[NumberOfPoints] =
								REHAMOVE__PW_COMPENSATION_PULS;
					}
					ChargeOverAll += PulseWidth[NumberOfPoints]
							* Current[NumberOfPoints];
					if (fabs(
							ChargeOverAll) <= REHAMOVE__PW_MIN*REHAMOVE__I_MIN) {
						break;
					}
				}
				if (fabs(ChargeOverAll) >= REHAMOVE__PW_MIN * REHAMOVE__I_MIN) {
					ChargeOverAll += PulseWidth[NumberOfPoints - 1]
							* Current[NumberOfPoints - 1] * -1.0;
					// set the current as needed and change the pulse width as long as possible
					PulseWidth[NumberOfPoints - 1] =
							PULSWIDTH_BALANCED_UNSYMETRIC;
					Current[NumberOfPoints - 1] = (float) (ChargeOverAll
							/ PULSWIDTH_BALANCED_UNSYMETRIC); // third pulse, very long with
				}
				ChargeOverAll += PulseWidth[NumberOfPoints - 1]
						* Current[NumberOfPoints - 1];
			} else {
				NumberOfPoints = 0;
				continue;
			}
			break;
*/

		/*
		 * Done with the pulse form generation
		 */
		default:
			// error: unknown shape
			RehaMove3::printMessage(printMSG_rmpSequenceError, "%s Error: The requested shape %u is invalid! (time: %0.3f; pulse: %u)\n", this->DeviceIDClass, SequenceConfig->PulseConfig[i_Puls].Shape, RehaMove3::GetCurrentTime(), i_Puls);
			// TODO: mark this pulse as not executed
			continue;
		}

		if (ChargeOverAll > 1.0) {
			RehaMove3::printMessage(printMSG_rmpWarningCorrectionChargeInbalace, "%s Charge Unbalance:\n   -> The remaining charge over all points is still != 0 but is %0.2f mAuS! (time: %0.3f; pulse: %u)\n", this->DeviceIDClass, ChargeOverAll, RehaMove3::GetCurrentTime(), i_Puls);
			// TODO: mark this pulse as not balanced
		}

		/*
		 * Build the channel configuration
		 */
		if (NumberOfPoints > 0){
			ll_channel_config.enable_stimulation = 1; 				// Activate the module
			ll_channel_config.channel = (Smpt_Channel) (SequenceConfig->PulseConfig[i_Puls].Channel	- 1); // Set the correct channel
			ll_channel_config.number_of_points = NumberOfPoints; 	// Set the number of points
			// Set the stimulation pulse
			for (iPoint = 0; iPoint < NumberOfPoints; iPoint++) {
				ll_channel_config.points[iPoint].control_mode = Smpt_Ll_Control_Current; 			// TODO: support other control techniques
				ll_channel_config.points[iPoint].interpolation_mode = Smpt_Ll_Interpolation_Jump;  // TODO: support other interpolation methods
				ll_channel_config.points[iPoint].time = PulseWidth[iPoint];
				ll_channel_config.points[iPoint].current = Current[iPoint];
			}
		} else {
			ll_channel_config.enable_stimulation = 0; 				// Activate the module
			ll_channel_config.number_of_points = 0; 				// Set the number of points
		}

		/*
		 * Debug output of this pulse
		 */
		if (this->rmpInitSettings.DebugConfig.printStimInfos){
			// Stimulation configuration
			RehaMove3::printMessage(printMSG_rmpPulseConfig, "  Puls %u -> Channel=%u; Shape=%u; PW=%u; I=%0.1f\n",i_Puls+1, SequenceConfig->PulseConfig[i_Puls+1].Channel, SequenceConfig->PulseConfig[i_Puls].Shape, SequenceConfig->PulseConfig[i_Puls].PulseWidth, SequenceConfig->PulseConfig[i_Puls].Current);
			if ( SequenceConfig->PulseConfig[i_Puls+1].Shape == Shape_UNbalanced_UNsymetric_Biphasic_SECOUND || SequenceConfig->PulseConfig[i_Puls+1].Shape == Shape_Balanced_UNsymetric_Biphasic_SECOUND ) {
				RehaMove3::printMessage(printMSG_rmpPulseConfig, "  Puls %u -> Channel=%u; Shape=%u; PW=%u; I=%0.1f\n", i_Puls+2, SequenceConfig->PulseConfig[i_Puls+1].Channel, SequenceConfig->PulseConfig[i_Puls+1].Shape, SequenceConfig->PulseConfig[i_Puls+1].PulseWidth, SequenceConfig->PulseConfig[i_Puls+1].Current);
			}
			for (iPoint=0; iPoint<ll_channel_config.number_of_points; iPoint++) {
				RehaMove3::printMessage(printMSG_rmpPulseConfig, "     PointConfig% 3d: Duration=% 4iµs; Current=% +7.2fmA; (Mode=%i; IM=%i)\n",
						iPoint+1, ll_channel_config.points[iPoint].time, ll_channel_config.points[iPoint].current, ll_channel_config.points[iPoint].control_mode, ll_channel_config.points[iPoint].interpolation_mode );
			}
		}
		if (this->rmpInitSettings.DebugConfig.printStimInfos){
			// Stimulation configuration
			//RehaMove3::printMessage(printMSG_rmpPulseConfig, "\n");
		}

		/*
		 * Prepare for acks and sequence statistics
		 */
		ll_channel_config.packet_number = GetPackageNumber();
		//this->Acks.LastPackageNumbers[i_Puls] = ll_channel_config.packet_number; TODO

		/*
		 * Check the configuration and send it
		 */
		if (smpt_is_valid_ll_channel_config(&ll_channel_config)) {
			// Send the Ll_channel_list command to RehaMove
			if (smpt_send_ll_channel_config(&(this->Device), &ll_channel_config)){
				OneOrMorePulsesSend = true;
				this->Stats.StimultionPulsesSend++;
				// add the expected response to the ChannelResponse queue
				PutChannelResponseExpectation(this->Stats.SequencesSend+1, ll_channel_config.channel, ll_channel_config.packet_number);
			} else {
				// error: failed to send the configuration
				RehaMove3::printMessage(printMSG_error, "%s Error: The channel configuration could not be send! (time: %0.3f; pulse: %u)\n", this->DeviceIDClass, RehaMove3::GetCurrentTime(), i_Puls);
				this->Stats.StimultionPulsesNotSend++;
				// TODO: mark this pulse as not send
			}
		} else {
			// error: channel configuration is INvalid
			RehaMove3::printMessage(printMSG_rmpSequenceError, "%s Error: The channel configuration is not valid! (time: %0.3f; pulse: %u)\n", this->DeviceIDClass, RehaMove3::GetCurrentTime(), i_Puls);
			// TODO: mark this pulse as not send
			this->Stats.StimultionPulsesNotSend++;
		}
	} // for loop

	// Debug
	if (this->rmpInitSettings.DebugConfig.printStimInfos){
		printf("\n");
	}

	// done sending the sequence configuration
	if (OneOrMorePulsesSend){
		this->Stats.SequencesSend++;
	}
	return true;
}

bool RehaMove3::GetLastSequenceResult(uint16_t *PulseErrors, bool *SequenceWasComplete)
{
	// todo gibt das letzte simulationsergebnis zurück -> return für fehler oder nicht und Argument für das eigentliche resultat
	// nur diese die Funktion enfernt die letzte sequence aus der Queue
	bool ReturnValue = false;
	// check for ACKs and process them
	RehaMove3::ReadAcksBlocking();
	// lock the  sequence queue
	pthread_mutex_lock(&(this->SequenceQueueLock_mutex));

	uint16_t tempPulseErrors = 0;
	bool tempSequenceWasComplete = false;

	// get the last sequence and get the status of that sequence
	// -> is there a sequence?
	if (this->SequenceQueue.QueueSize > 0){
		// yes, there is a sequence

		// was the sequence successful ?
		if (!this->SequenceQueue.Queue[this->SequenceQueue.QueueTail].SequenceWasSuccessful){
			// there was an error -> encode the pulses that did fail
			// encode the pulse errors
			for (uint8_t i = 0; i < this->SequenceQueue.Queue[this->SequenceQueue.QueueTail].NumberOfAcks; i++){
				if (this->SequenceQueue.Queue[this->SequenceQueue.QueueTail].StimulationPulse[i].Result != Smpt_Result_Successful){
					// encode the failed pulses as bits
					tempPulseErrors |= 0x01 << i;
				}
			}
			ReturnValue = false;
		} else {
			tempPulseErrors = 0x00;
			ReturnValue = true;
		}

		// is the sequence complete ?
		if (this->SequenceQueue.Queue[this->SequenceQueue.QueueTail].NumberOfAcks == this->SequenceQueue.Queue[this->SequenceQueue.QueueTail].NumberOfPulses){
			// yes, this sequence did receive acks for all pulses -> remove the sequence by increasing the QueueTail
			this->SequenceQueue.Queue[this->SequenceQueue.QueueTail].SequenceNumber = 0;
			this->SequenceQueue.Queue[this->SequenceQueue.QueueTail].SequenceWasSuccessful = false;
			this->SequenceQueue.Queue[this->SequenceQueue.QueueTail].NumberOfPulses = 0;
			this->SequenceQueue.Queue[this->SequenceQueue.QueueTail].NumberOfAcks = 0;
			this->SequenceQueue.QueueSize--;
			// increase the counter?
			if (this->SequenceQueue.QueueTail != this->SequenceQueue.QueueHead){
				// yes, increase the counter
				this->SequenceQueue.QueueTail++;
				if (this->SequenceQueue.QueueTail >= REHAMOVE_SEQUENCE_QUEUE_SIZE){
					this->SequenceQueue.QueueTail = 0;
				}
			}
			// optional output
			tempSequenceWasComplete = true;
		} else {
			// no, the sequence did NOT receive acks for all pulses -> wait, maybe the function was called before the stimulator could answer

			RehaMove3::printMessage(printMSG_rmpErrorUnclaimedSequence, "%s Error: The sequence acknowlegment queue contains a sequence with unacknowledged pulses!\n", this->DeviceIDClass);
			// is there another sequence?
			if (this->SequenceQueue.QueueSize > 1) {
				// did this other sequence receive a ack?
				uint8_t tempQueueElement = this->SequenceQueue.QueueTail +1;
				if (tempQueueElement >= REHAMOVE_SEQUENCE_QUEUE_SIZE){
					tempQueueElement = 0;
				}
				if ( this->SequenceQueue.Queue[tempQueueElement].NumberOfAcks > 0 ) {
					// queue end was not reached -> there is a sequence with unacknowledged pulses
					RehaMove3::printMessage(printMSG_rmpErrorUnclaimedSequence, "     -> There might be undetected errors. Please check your your usage of the RehaMove3 interface class!\n");
				} else {
					RehaMove3::printMessage(printMSG_rmpErrorUnclaimedSequence, "     -> There might be a delay in the response received by the 'GetLastSequenceResult' function.\n");
				}
			} else {
				RehaMove3::printMessage(printMSG_rmpErrorUnclaimedSequence, "     -> There might be a delay in the response received by the 'GetLastSequenceResult' function.\n");
			}

			// optional output
			tempSequenceWasComplete = false;
			// no -> return false
			ReturnValue = false;
		}
	} else {
		// no there is no sequence
		// is stimulation disabled?
		if (this->rmpStatus.DoNotStimulate){
			// yes, no stimulation -> return false and errors
			// optional output
			tempSequenceWasComplete = false;
			tempPulseErrors = 0xFFFF;
			// no -> return false
			ReturnValue = false;
		} else {
			// no -> return true because no sequence was send, so this is always successful
			tempSequenceWasComplete = true;
			tempPulseErrors = 0x00000;
			// no -> return false
			ReturnValue = true;
		}
	}

	// unlock the sequence queue
	pthread_mutex_unlock(&(this->SequenceQueueLock_mutex));

	if (PulseErrors != NULL){
		*PulseErrors = tempPulseErrors;
	}
	if (SequenceWasComplete != NULL){
		*SequenceWasComplete = tempSequenceWasComplete;
	}
	return ReturnValue;
}

bool RehaMove3::DeInitialiseDevice(bool doPrintInfos, bool doPrintStats)
{
	if (this->rmpStatus.InitThreatRunning){
		this->rmpStatus.InitThreatActive  = false;
	}
	if (this->rmpStatus.DeviceInitialised) {
		/*
		 * LowLevel
		 */
		if (this->rmpStatus.DeviceLlIsInitialised){
			// send the ll_stop command
			if (smpt_send_ll_stop(&(this->Device), RehaMove3::GetPackageNumber())) {
				if (RehaMove3::GetResponse(Smpt_Cmd_Ll_Stop_Ack, true, 500) != Smpt_Cmd_Ll_Stop_Ack) {
					RehaMove3::printMessage(printMSG_error, "%s Error: The device could not be DEinitialised! The LL_Stop acknowledgement is missing!\n", this->DeviceIDClass);
				} else {
					// response received -> deinitialise the device
					this->rmpStatus.DeviceLlIsInitialised = false;
				}
			} else {
				RehaMove3::printMessage(printMSG_error, "%s Error: Sending the command %d failed!\n", this->DeviceIDClass, Smpt_Cmd_Ll_Stop_Ack);
				RehaMove3::CloseSerial(); // todo: einfach schließen??
			}
			// done
		}

		/*
		 * General
		 */
		// get the current status
		if (smpt_send_get_battery_status(&(this->Device), RehaMove3::GetPackageNumber())) {
			if (!RehaMove3::NewStatusUpdateReceived(500)) {
				RehaMove3::printMessage(printMSG_error, "%s Error: The current device status could net be read!\n", this->DeviceIDClass);
			}
		} else {
			RehaMove3::printMessage(printMSG_error, "%s Error: Sending the command %d failed!\n", this->DeviceIDClass, Smpt_Cmd_Get_Battery_Status);
			RehaMove3::CloseSerial(); // todo
		}
	}
	//Print status / statistic information
	RehaMove3::printMessage(printMSG_rmpInitInfo, "%s: DeInitialising the device %s was successful!\n", this->DeviceIDClass, this->DeviceFileName);
	RehaMove3::GetCurrentStatus(doPrintInfos,doPrintStats, 0);

	//Close device
	RehaMove3::CloseSerial();
	// done -> resets
	this->rmpStatus.DeviceInitialised = false;
	return true;
}


RehaMove3::rmpGetStatus_t RehaMove3::GetCurrentStatus(bool DoPrintStatus, bool DoPrintStatistic, uint16_t WaitTimeout)
{
	// Check if the class was successfully created
	if (!this->ClassInstanceInitialised){
		RehaMove3::printMessage(printMSG_error, "RehaMove3 FAITAL ERROR: The class instance was not successfully initialised!\n");
		rmpGetStatus_t tempStatus = {};
		memset(&tempStatus, 0, sizeof(tempStatus));
		return tempStatus;
	}

	// update the status first?
	if (WaitTimeout > 0){
		/*
		 * Checks
		 */
		// get the current stim status -> saving the data is done in the response handlers
		if (smpt_send_get_stim_status(&(this->Device), RehaMove3::GetPackageNumber())) {
			if (!RehaMove3::NewStatusUpdateReceived(200)){
				RehaMove3::printMessage(printMSG_error, "%s Error: The current status for 'stim' could net be read!\n", this->DeviceIDClass);
			}
		} else {
			RehaMove3::printMessage(printMSG_error, "%s Error: Sending the command %d failed!\n", this->DeviceIDClass, Smpt_Cmd_Get_Stim_Status);
		}

		// get the current main status -> saving the data is done in the response handlers
		if (smpt_send_get_main_status(&(this->Device), RehaMove3::GetPackageNumber())) {
			if (!RehaMove3::NewStatusUpdateReceived(200)){
				RehaMove3::printMessage(printMSG_error, "%s Error: The current status for 'main' could net be read!\n", this->DeviceIDClass);
			}
		} else {
			RehaMove3::printMessage(printMSG_error, "%s Error: Sending the command %d failed!\n", this->DeviceIDClass, Smpt_Cmd_Get_Main_Status);
		}

		// get the current battery status -> saving the data is done in the response handlers
		if (smpt_send_get_battery_status(&(this->Device), RehaMove3::GetPackageNumber())) {
			if (!RehaMove3::NewStatusUpdateReceived(500)){
				RehaMove3::printMessage(printMSG_error, "%s Error: The current device status could net be read!\n", this->DeviceIDClass);
			}
		} else {
			RehaMove3::printMessage(printMSG_error, "%s Error: Sending the command %d failed!\n", this->DeviceIDClass, Smpt_Cmd_Get_Battery_Status);
		}
	}
	// print the status to the terminal?
	if (DoPrintStatus){
		struct timeval time;gettimeofday(&time,NULL);
		uint64_t CurrentTime = (uint64_t)(time.tv_sec*1000.0) + (uint64_t)(time.tv_usec/1000.0) + 1;
		// status
		printf("%s: Status Report\n     -> Interface: %s\n     -> Device ID: %s\n     -> Battery Voltage: %d%% (%0.2fV)\n     -> Last updated: %0.3f seconds ago\n     -> Init Threat running: %s\n     -> Receiver Threat running: %s\n",
				this->DeviceIDClass, this->DeviceFileName, this->rmpStatus.Device.DeviceID, this->rmpStatus.Device.BatteryLevel, this->rmpStatus.Device.BatteryVoltage, ((double)(CurrentTime - this->rmpStatus.StartTime_ms - this->rmpStatus.LastUpdated))/1000, this->rmpStatus.InitThreatRunning ? "yes":"no", this->rmpStatus.ReceiverThreatRunning ? "yes":"no");
		printf("     -> LowLevel:\n        -> Initialised: %s\n        -> Current/Last High Voltage: %dV\n        -> Use Denervation: %s\n        -> Abort after N stimulation errors: %d\n        -> Resume the stimulation after: %d sequences\n\n",
				this->rmpStatus.DeviceLlIsInitialised ? "yes":"no", this->rmpStatus.Device.HighVoltageVoltage, this->rmpSettings.DenervationIsUsed ? "yes":"no", this->rmpSettings.NumberOfErrorsAfterWhichToAbort, this->rmpSettings.NumberOfSequencesAfterWhichToRetestForError);
	}
	// print the statistics to the terminal?
	if (DoPrintStatistic){
		// statistic
		printf("%s: Statistic Report\n     -> Pulse SEQUENCES send: %lu (%lu pulses send; %lu pulses NOT send)\n        -> Successful: %lu (%lu pulses)\n        -> Unsuccessful: %lu (%lu pulses)\n           -> Stimulation Error: %lu\n        -> Missing: %lu\n",
						this->DeviceIDClass, this->Stats.SequencesSend, this->Stats.StimultionPulsesSend, this->Stats.StimultionPulsesNotSend, this->Stats.SequencesSuccessful, this->Stats.StimultionPulsesSuccessful, this->Stats.SequencesFailed, this->Stats.StimultionPulsesFailed, this->Stats.SequencesFailed_StimError,
						(this->Stats.SequencesSend - (this->Stats.SequencesSuccessful + this->Stats.SequencesFailed)) );
		printf("     -> Input Corrections:\n        -> Invalid Input: %lu pulses\n        -> Current correction (to high): %u pulses\n        -> Current correction (to low):  %u pulses\n        -> Pulsewidth correction (to high): %u pulses\n        -> Pulsewidth correction (to low):  %u pulses\n",
				this->Stats.InvalidInput, this->Stats.InputCorrections_CurrentOver, this->Stats.InputCorrections_CurrentUnder, this->Stats.InputCorrections_PulswidthOver, this->Stats.InputCorrections_PulswidthUnder);
	}

	rmpGetStatus_t CurrentState = {};
	CurrentState.DeviceIsOpen = this->rmpStatus.DeviceIsOpen;
	CurrentState.DeviceLlIsInitialised = this->rmpStatus.DeviceLlIsInitialised;
	CurrentState.DeviceMlIsInitialised = this->rmpStatus.DeviceMlIsInitialised;
	CurrentState.DeviceInitialised     = this->rmpStatus.DeviceInitialised;
	CurrentState.LastUpdated        = this->rmpStatus.LastUpdated;
	CurrentState.DoNotStimulate     = this->rmpStatus.DoNotStimulate;
	CurrentState.NumberOfStimErrors = this->rmpStatus.NumberOfStimErrors;
	CurrentState.BatteryLevel       = this->rmpStatus.Device.BatteryLevel;
	CurrentState.BatteryVoltage     = this->rmpStatus.Device.BatteryVoltage;
	CurrentState.HighVoltageVoltage = this->rmpStatus.Device.HighVoltageVoltage;
	// return the current status
	return CurrentState;
}


bool RehaMove3::DoDeviceReset(void)
{
	if (this->rmpStatus.DeviceIsOpen){
		// close and open the device
		RehaMove3::CloseSerial();
		RehaMove3::OpenSerial();
	}

	// execute the reset
	RehaMove3::printMessage(printMSG_warning, "\n%s: Executing a device reset ... ", this->DeviceIDClass);
	if (smpt_send_reset(&(this->Device), RehaMove3::GetPackageNumber())) {
		if (RehaMove3::GetResponse(Smpt_Cmd_Reset_Ack, true, 5500) != Smpt_Cmd_Reset_Ack) {
			RehaMove3::printMessage(printMSG_warning, "should be done. The reset acknowledgement is missing!\n");
		} else {
			RehaMove3::printMessage(printMSG_warning, "done.\n");
		}
	} else {
		RehaMove3::printMessage(printMSG_error, "%s Error: Sending the command %d failed!\n", this->DeviceIDClass, Smpt_Cmd_Reset_Ack);
		RehaMove3::CloseSerial();
		return false;
	}
	// General commands
	// get the device id
	if (smpt_send_get_device_id(&(this->Device), RehaMove3::GetPackageNumber())) {
		if (RehaMove3::GetResponse(Smpt_Cmd_Get_Device_Id_Ack, true, 200) != Smpt_Cmd_Get_Device_Id_Ack) {
			// error
			RehaMove3::printMessage(printMSG_error, "%s Error: The device ID could not be read!\n", this->DeviceIDClass);
			return false;
		}
	} else {
		RehaMove3::printMessage(printMSG_error, "%s Error: Sending the command %d failed!\n", this->DeviceIDClass, Smpt_Cmd_Get_Device_Id_Ack);
		RehaMove3::CloseSerial();
		return false;
	}
	return true;
}

bool RehaMove3::OpenSerial()
{
	if (access(this->DeviceFileName, R_OK | W_OK) != -1) {
		// serial interface exists
		if (smpt_open_serial_port(&(this->Device), this->DeviceFileName)) {
			// opening successful
			RehaMove3::printMessage(printMSG_rmpDeviceInfo, "RehaMove3 DEBUG: Device %s opened successfully.\n", this->DeviceFileName);
			this->rmpStatus.DeviceIsOpen = true;
			/*
			 *  Start the receiver threat
			 */
			if (this->rmpSettings.UseThreadForAcks){
				this->rmpStatus.ReceiverThreatActive = true;
				if (pthread_create(&(this->ReceiverThread), NULL, ReceiverThreadFunc, (void *)this) != 0) {
					RehaMove3::printMessage(printMSG_error, "%s Error: The receiver threat could net be started:\n     -> %s (%d)\n", this->DeviceIDClass, strerror(errno), errno);
					RehaMove3::CloseSerial();
					return false;
				}
				RehaMove3::printMessage(printMSG_rmpDeviceInfo, "RehaMove3 DEBUG: Starting the receiver threat was successfully.\n");
			}
			// done
			return true;
		} else {
			RehaMove3::printMessage(printMSG_rmpDeviceInfo, "RehaMove3 DEBUG: Device %s opening failed.\n", this->DeviceFileName);
			// opening failed
			return false;
		}
	} else {
		// serial interface doesn't exist
		return false;
	}
}

bool RehaMove3::CloseSerial()
{
	if (this->rmpStatus.DeviceIsOpen) {
		if (this->rmpStatus.InitThreatRunning) {
			this->rmpStatus.InitThreatActive = false;
		}
		if (this->rmpStatus.ReceiverThreatRunning) {
			this->rmpStatus.ReceiverThreatActive = false;
		}
		if (this->rmpStatus.InitThreatRunning) {
			//what for the init threat to stop
			do {
				usleep(500);
			} while (this->rmpStatus.InitThreatRunning);
		}
		if (this->rmpStatus.ReceiverThreatRunning) {
			//what for the receiver threat to stop
			do {
				usleep(500);
			} while (this->rmpStatus.ReceiverThreatRunning);
		}

		if (smpt_close_serial_port(&(this->Device))) {
			this->rmpStatus.DeviceIsOpen = false;
			return true;
		} else {
			return false;
		}
	} else {
		this->rmpStatus.DeviceIsOpen = false;
		return true;
	}
}


bool RehaMove3::CheckSupportedVersion(const uint8_t SupportedVersions[][3], Smpt_version *DeviceVersion, bool disablePedanticVersionCheck, bool *printWarning, bool *printError)
{
	*printWarning = false;
	*printError = false;

	uint8_t NumberOfSupportedVersions = 0;
	if (SupportedVersions == RMP_SupportedVersionsMain){
		NumberOfSupportedVersions = RMP_NumberOfSupportedVersionsMain;
	}
	if (SupportedVersions == RMP_SupportedVersionsStim){
		NumberOfSupportedVersions = RMP_NumberOfSupportedVersionsStim;
	}
	if (SupportedVersions == RMP_SupportedVersionsSMPT){
		NumberOfSupportedVersions = RMP_NumberOfSupportedVersionsSMPT;
	}

	uint8_t  iErrorSmallest = 255;
	for (uint8_t i = 0; i< NumberOfSupportedVersions; i++){
		// check them version numbers
		if (DeviceVersion->major == SupportedVersions[i][0] && DeviceVersion->minor == SupportedVersions[i][1] && DeviceVersion->revision == SupportedVersions[i][2]){
			// the version numbers match -> return true
			return true;
		} else {
			// the major or minor version number does not match
			if (DeviceVersion->major == SupportedVersions[i][0]) {
				if (DeviceVersion->minor == SupportedVersions[i][1]) {
					// the major and minor version number matches -> the revision does not match -> warning
					iErrorSmallest = 0;
				} else {
					// the major version number matches -> the minor does not match -> find the minimal difference
					if (abs(int32_t(DeviceVersion->minor - SupportedVersions[i][1])) < iErrorSmallest){
						iErrorSmallest = abs(int32_t(DeviceVersion->minor - SupportedVersions[i][1]));
					}
				}
			}
		}
	}

	// error or warning?
	if (iErrorSmallest > RMP_ErrorWarningBand){
		*printError = true;
	} else {
		*printWarning = true;
	}

	// disable the pedantic version check?
	if (disablePedanticVersionCheck){
		if (*printError){
			*printError = false;
			*printWarning = true;
		}
	}
	// done
	return false;
}

bool RehaMove3::CheckChannel(uint8_t Channel_IN) {
	if ((Channel_IN < 1) || (Channel_IN > 4)) {
		return false;
	}
	return true;
}

uint16_t RehaMove3::CheckAndCorrectPulsewidth(int PulseWidthIN, bool *Corrected) {
	if (PulseWidthIN > this->rmpSettings.MaxPulseWidth) {
		this->Stats.InputCorrections_PulswidthOver++;
		*Corrected = true;
		return this->rmpSettings.MaxPulseWidth;
	} else if (PulseWidthIN < 0) {
		this->Stats.InputCorrections_PulswidthUnder++;
		*Corrected = true;
		return 0;
	} else {
		return (uint16_t) PulseWidthIN;
	}
}

float RehaMove3::CheckAndCorrectCurrent(float CurrentIN, bool *Corrected)
{
	if (CurrentIN > this->rmpSettings.MaxCurrent) {
		this->Stats.InputCorrections_CurrentOver++;
		*Corrected = true;
		return this->rmpSettings.MaxCurrent;
	} else if (CurrentIN < -1*this->rmpSettings.MaxCurrent) {
		this->Stats.InputCorrections_CurrentUnder++;
		*Corrected = true;
		return -1*this->rmpSettings.MaxCurrent;
	} else {
		float temp = CurrentIN -floorf(CurrentIN);
		// check if the current is x.0 or x.5
		if ( !((temp == 0) || (temp == 0.5)) ){
			float tempI = modff(CurrentIN, &CurrentIN);
			if (tempI > 0.75) {
				// is x.0 -> ceil
				CurrentIN = CurrentIN +1.0;
			} else if (tempI > 0.25) {
				// is x.5
				CurrentIN = CurrentIN +0.5;
			} else {
				// is x.0 -> floor
				//CurrentIN = CurrentIN +0.0;
			}
		}
		return CurrentIN;
	}
}

uint8_t RehaMove3::GetMinimalCurrentPulse(uint16_t *PulseWidth, float *Current, double *Charge, uint8_t MaxNumberOfPoints)
{
	// get the current for the second pulse, very long with almost no current to achieve charge balance
	Current[0] = fabs(*Charge) / (REHAMOVE_SHAPES__PULSWIDTH_BALANCED_UNSYMETRIC *MaxNumberOfPoints);
	// make sure the current is valid (x.0 or x.5)
	if ( (Current[0] -floorf(Current[0])) <= 0.5 ){
		Current[0] = floorf(Current[0]) +0.5;
	} else {
		Current[0] = ceilf(Current[0]);
	}
	if (Current[0] < REHAMOVE_SHAPES__CURRENT_MIN_BALANCED_UNSYMETRIC ){
		Current[0] = REHAMOVE_SHAPES__CURRENT_MIN_BALANCED_UNSYMETRIC;
	}
	PulseWidth[0] = (uint16_t)round((fabs(*Charge) / (double)Current[0]));
	Current[0] *= *Charge < 0 ? +1.0 : -1.0;
	*Charge += PulseWidth[0] * Current[0];
	// split the pulse width if larger as REHAMOVE_SHAPES_PULSWIDTH_BALANCED_UNSYMETRIC
	uint8_t iPoint = 1;
	while (PulseWidth[0] > REHAMOVE_SHAPES__PULSWIDTH_BALANCED_UNSYMETRIC){
		PulseWidth[iPoint] = REHAMOVE_SHAPES__PULSWIDTH_BALANCED_UNSYMETRIC;
		PulseWidth[0] -= REHAMOVE_SHAPES__PULSWIDTH_BALANCED_UNSYMETRIC;
		Current[iPoint] = Current[0];
		iPoint++;
	}
	return iPoint;
}

inline void RehaMove3::ReadAcksBlocking(void)
{
	if (!this->rmpSettings.UseThreadForAcks){
		RehaMove3::ReadAcks();
	}
}

bool RehaMove3::ReadAcks(void)
{
    int retValue = 0;
    // look this function
    if ( (retValue = pthread_mutex_trylock(&(this->ReadPackage_mutex))) == EBUSY) {
    	RehaMove3::printMessage(printMSG_error, "%s Error: The ReadAcks function is looked: %s (%d)\n", this->DeviceIDClass, strerror(retValue), retValue);
        return false;
    }
    this->rmpStatus.ReceiverThreatRunning = true;

	Smpt_get_main_status_ack 	GeneralMainStatusAck;
	Smpt_get_stim_status_ack 	GeneralStimStatusAck;
	Smpt_get_battery_status_ack GeneralBatteryStatusAck;
	Smpt_ll_channel_config_ack 	ChannelAck;

	struct timeval 				time;
    SingleResponse_t 			Response;
    bool PackageReceived = false;

	do {
		/*
		 * Look if a response was received
		 */
		if (smpt_new_packet_received(&(this->Device))) {
			PackageReceived = true;
			/*
			 * Get the Response
			 */
			smpt_clear_ack(&(Response.Ack));
			smpt_last_ack(&(this->Device), &(Response.Ack));
			// debug output
			RehaMove3::printMessage(printMSG_rmpReceiveACK, "%s DEBUG: Response received\n   -> Ack: %i; Result: %i; Package Number: %i\n", this->DeviceIDClass, Response.Ack.command_number, Response.Ack.result, Response.Ack.packet_number);

			/*
			 * Error Handler 1, handle the error code as soon as the error comes in, otherwise handle the error in the response queue
			 */
			Response.Error = true; 	// default
			// check the result
			switch (Response.Ack.result) {
			case Smpt_Result_Successful: // No error, command execution is started
				// default -> this is ok
				Response.Error = false;
				break;
			default:
				//dummy code to silence compiler warnings about missing case statements
				break;
			}

			/*
			 * Response Handler 1, handle the response as soon as the response comes in, otherwise handle it in the response queue
			 */
			switch (Response.Ack.command_number) {
			/*
			 *  General commands
			 */
			case Smpt_Cmd_Get_Device_Id_Ack:
				// lock the acks struct
				pthread_mutex_lock(&(this->AcksLock_mutex));
				// Get the device id response
				smpt_clear_get_device_id_ack(&(this->Acks.G_device_id_ack));
				// Writes the received data into ack struct
				smpt_get_get_device_id_ack(&(this->Device), &(this->Acks.G_device_id_ack));
		        // unlock the acks struct
		        pthread_mutex_unlock(&(this->AcksLock_mutex));
				break;

			case Smpt_Cmd_Get_Version_Main_Ack:
				// lock the acks struct
				pthread_mutex_lock(&(this->AcksLock_mutex));
				/* Get the version response */
				smpt_clear_get_version_ack(&(this->Acks.G_version_ack));
				/* Writes the received data into ack struct */
				smpt_get_get_version_main_ack(&(this->Device), &(this->Acks.G_version_ack));
				// unlock the acks struct
				pthread_mutex_unlock(&(this->AcksLock_mutex));
				break;

			case Smpt_Cmd_Get_Version_Stim_Ack:
				// lock the acks struct
				pthread_mutex_lock(&(this->AcksLock_mutex));
				/* Get the version response */
				smpt_clear_get_version_ack(&(this->Acks.G_version_ack));
				/* Writes the received data into ack struct */
				smpt_get_get_version_stim_ack(&(this->Device), &(this->Acks.G_version_ack));
				// unlock the acks struct
				pthread_mutex_unlock(&(this->AcksLock_mutex));
				break;

			case Smpt_Cmd_Get_Battery_Status_Ack: // PC <- stimulator smpt_get_battery_status_ack()
				/* Get the status response */
				smpt_clear_get_battery_status_ack(&GeneralBatteryStatusAck);
				/* Writes the received data into battery_status_ack */
				smpt_get_get_battery_status_ack(&(this->Device), &GeneralBatteryStatusAck);
				// save the current status
				this->rmpStatus.Device.BatteryLevel = GeneralBatteryStatusAck.battery_level;
				this->rmpStatus.Device.BatteryVoltage = ((float)GeneralBatteryStatusAck.battery_voltage) / 1000.0;
				// save the current time
				gettimeofday(&time,NULL);
				this->rmpStatus.LastUpdated = ((uint64_t)(time.tv_sec*1000.0) + (uint64_t)(time.tv_usec/1000.0)) - this->rmpStatus.StartTime_ms;
				// the response is handled -> do not add this response to the response queue
				continue;
				break;

			case Smpt_Cmd_Get_Main_Status_Ack: // PC <- stimulator smpt_get_main_status_ack()
				/* Get the status response */
				smpt_clear_get_main_status_ack(&GeneralMainStatusAck);
				/* Writes the received data into main_status_ack */
				smpt_get_get_main_status_ack(&(this->Device), &GeneralMainStatusAck);
				// save the current status
				this->rmpStatus.Device.MainStatus = GeneralMainStatusAck.main_status;
				// save the current time
				gettimeofday(&time,NULL);
				this->rmpStatus.LastUpdated = ((uint64_t)(time.tv_sec*1000.0) + (uint64_t)(time.tv_usec/1000.0)) - this->rmpStatus.StartTime_ms;
				// the response is handled -> do not add this response to the response queue
				continue;
				break;

			case Smpt_Cmd_Get_Stim_Status_Ack: // PC <- stimulator smpt_get_stim_status_ack()
				/* Get the status response */
				smpt_clear_get_stim_status_ack(&GeneralStimStatusAck);
				/* Writes the received data into stim_status_ack */
				smpt_get_get_stim_status_ack(&(this->Device), &GeneralStimStatusAck);
				// save the current status
				this->rmpStatus.Device.StimStatus = GeneralStimStatusAck.stim_status;
				this->rmpStatus.Device.HighVoltageLevel = GeneralStimStatusAck.high_voltage_level;
				switch (this->rmpStatus.Device.HighVoltageLevel){
				case Smpt_High_Voltage_Default:
				case Smpt_High_Voltage_150V:
					this->rmpStatus.Device.HighVoltageVoltage = 150;
					break;
				case Smpt_High_Voltage_120V:
					this->rmpStatus.Device.HighVoltageVoltage = 120;
					break;
				case Smpt_High_Voltage_90V:
					this->rmpStatus.Device.HighVoltageVoltage = 90;
					break;
				case Smpt_High_Voltage_60V:
					this->rmpStatus.Device.HighVoltageVoltage = 60;
					break;
				case Smpt_High_Voltage_30V:
					this->rmpStatus.Device.HighVoltageVoltage = 30;
					break;
				case Smpt_High_Voltage_Off:
					this->rmpStatus.Device.HighVoltageVoltage = 0;
					break;
				default:
					RehaMove3::printMessage(printMSG_error, "%s Error: An invalid option for the 'high voltage' setting was returned: %d!\n     -> Please check library version and this implementation!\n",
							this->DeviceIDClass, this->rmpStatus.Device.HighVoltageLevel);
				}
				// save the current time
				gettimeofday(&time,NULL);
				this->rmpStatus.LastUpdated = ((uint64_t)(time.tv_sec*1000.0) + (uint64_t)(time.tv_usec/1000.0)) - this->rmpStatus.StartTime_ms;
				// the response is handled -> do not add this response to the response queue
				continue;
				break;


			/*
			 * LowLevel commands
			 */
			case Smpt_Cmd_Ll_Init_Ack: // PC <- stimulator smpt_get_ll_init_ack()
				// lock the acks struct
				pthread_mutex_lock(&(this->AcksLock_mutex));
				/* Get the init response */
				smpt_clear_ll_init_ack(&(this->Acks.G_ll_init_ack));
				/* Writes the received data into ll_init_ack */
				smpt_get_ll_init_ack(&(this->Device), &(this->Acks.G_ll_init_ack));
				// unlock the acks struct
				pthread_mutex_unlock(&(this->AcksLock_mutex));
				break;

			case Smpt_Cmd_Ll_Channel_Config_Ack: // PC <- stimulator smpt_get_ll_ch_config_ack()
				// Get the channel configuration response
				smpt_clear_ll_channel_config_ack(&ChannelAck);
				// Writes the received data into ll_channel_config_ack
				smpt_get_ll_channel_config_ack(&(this->Device), &ChannelAck);
				// push the result into the sequence queue
				RehaMove3::PutChannelResponse(ChannelAck.packet_number, ChannelAck.result, ChannelAck.electrode_error);
				// the response is handled -> do not add this response to the response queue
				continue;
				break;

			case Smpt_Cmd_Ll_Stop_Ack:       // PC <- stimulator smpt_last_ack()
				// update the internal status
				// this->rmpStatus.IsInitialised_LowLevel = false; -> done by the deinitalise function
				break;


			// Unknown error
			case Smpt_Cmd_Unknown_Cmd: // PC <- stimulator: The stimulator sends this cmd, if the received cmd can not be processed / is unknown.
				RehaMove3::printMessage(printMSG_error, "%s Error: An unknown command was send to the stimulator!\n", this->DeviceIDClass);
				break;

			default:
				// Error: unknown return ID
				RehaMove3::printMessage(printMSG_error, "%s Error: UNKNOWN acknowledgement received! (ack id = %i)\n", this->DeviceIDClass, (int)Response.Ack.command_number);
			}
			/*
			 * Add the response to the queue
			 */
			RehaMove3::PutResponse(&Response);

		} else {
			// no new responses available -> sleep
			PackageReceived = false;
			if (this->rmpSettings.UseThreadForAcks){
				usleep(REHAMOVE_ACK_THREAD_DELAY_US);
			}
		}
	} while (PackageReceived || this->rmpStatus.ReceiverThreatActive); // do loop

	// done
	this->rmpStatus.ReceiverThreatRunning = false;
	// UnLock the function
    pthread_mutex_unlock(&(this->ReadPackage_mutex));
	return true;
}


void RehaMove3::PutResponse(SingleResponse_t *Response)
{
	/*
	 * Check queue / Add response to queue
	 */
	bool wasRequested = false;
	// lock the queue
	pthread_mutex_lock(&(this->ResponseQueueLock_mutex));

	// check if there is request for this response in the queue
	bool SearchResponse = true;
	uint16_t QueueEntry = this->ResponseQueue.QueueTail;
	while (SearchResponse){
		// check if the response was requested
		if (Response->Ack.command_number == this->ResponseQueue.Queue[QueueEntry].Request){
			wasRequested = true;
			break;
		}
		// abort if the queue was searched
		if (QueueEntry == this->ResponseQueue.QueueHead) {
			SearchResponse = false;
			break;
		}
		// go to the next entry
		QueueEntry++;
		// make sure to stay inside of the queue
		if (QueueEntry >= REHAMOVE_RESPONSE_QUEUE_SIZE){
			QueueEntry = 0;
		}
	}

	// Process the response
	if (wasRequested){
		// the response was expected -> add the response to the queue
		memcpy(&(this->ResponseQueue.Queue[QueueEntry].Ack), &(Response->Ack), sizeof(Response->Ack));
		this->ResponseQueue.Queue[QueueEntry].Error = false;
		if (Response->Error){ // TODO
			this->ResponseQueue.Queue[QueueEntry].Error = true;
			memcpy(&(this->ResponseQueue.Queue[QueueEntry].ErrorDescription), &(Response->ErrorDescription), sizeof(Response->ErrorDescription));
		}
		// unlock the queue
		pthread_mutex_unlock(&(this->ResponseQueueLock_mutex));
		this->ResponseQueue.Queue[QueueEntry].ResponseReceived = true;
		if (this->ResponseQueue.Queue[QueueEntry].WaitForResponce){
			// the response is waited for
			if (this->ResponseQueue.Queue[QueueEntry].WaitTimedOut){
				// the response was waited for -> the timeout was triggered and the GetResponse function returned
				// -> call the GetResponse function now and let the response handler decide
				RehaMove3::GetResponse(this->ResponseQueue.Queue[QueueEntry].Request, false, 0);
			}
		} else {
			// the response is not waited for -> so no one will look at the result
			// -> call the GetResponse function now and let the response handler decide
			RehaMove3::GetResponse(this->ResponseQueue.Queue[QueueEntry].Request, false, 0);
		}
	} else {
		// the response was not requested or expected, so we do not safe the result in the queue but discard the ack
		// unlock the queue
		pthread_mutex_unlock(&(this->ResponseQueueLock_mutex));
		// print an error message
		RehaMove3::printMessage(printMSG_error, "%s Error: An UNEXPECTED acknowledgement was received!\n     -> Ack id = %u; Result = %u\n", this->DeviceIDClass, Response->Ack.command_number, Response->Ack.result);

	}
    // successfully finished
}

// TODO:  Rückgabewerte -> noch akutelles TODO?
int RehaMove3::GetResponse(Smpt_Cmd ExpectedCommand, bool AddExpectedResponse, int MilliSecondsToWait)
{
	struct timeval TStart = {}, TStop = {};
	double StartTime = 0, EndTime = 0;

	if (this->rmpInitSettings.DebugConfig.printSendCmdInfos){
		gettimeofday(&TStart,NULL);
		StartTime = (double)(TStart.tv_sec*1000.0) + (double)(TStart.tv_usec/1000.0);
		RehaMove3::printMessage(printMSG_rmpSendCMD, "%s DEBUG: Command send for ACK %u\n", this->DeviceIDClass, ExpectedCommand);
	}

	// input checks
	bool DoWait = false;
	if (MilliSecondsToWait <= 0){
		MilliSecondsToWait = 0;
	} else {
		DoWait = true;
	}

	/*
	 * Add a response to the queue
	 */
	if (AddExpectedResponse){
		// lock the queue
		pthread_mutex_lock(&(this->ResponseQueueLock_mutex));
		this->ResponseQueue.QueueHead++;
		if (this->ResponseQueue.QueueHead >= REHAMOVE_RESPONSE_QUEUE_SIZE){
			this->ResponseQueue.QueueHead = 0;
		}
		if (this->ResponseQueue.QueueHead == this->ResponseQueue.QueueTail) {
			// queue end was overrun -> we lose data
			RehaMove3::printMessage(printMSG_error, "%s Error: The request for acknowledgement = %u could not be added to the response queue!\n     -> The oldest acknowledgement will be discarded.\n", this->DeviceIDClass, ExpectedCommand);
			// IMPROVEMENT: wait until the queue is free again?
			this->ResponseQueue.QueueTail++;
			if (this->ResponseQueue.QueueTail >= REHAMOVE_RESPONSE_QUEUE_SIZE){
				this->ResponseQueue.QueueTail = 0;
			}
		}
		// write the response expectation
		this->ResponseQueue.Queue[this->ResponseQueue.QueueHead].Request = ExpectedCommand;
		this->ResponseQueue.Queue[this->ResponseQueue.QueueHead].ResponseReceived = false;
		if (MilliSecondsToWait > 0){
			this->ResponseQueue.Queue[this->ResponseQueue.QueueHead].WaitForResponce = true;
		} else {
			this->ResponseQueue.Queue[this->ResponseQueue.QueueHead].WaitForResponce = false;
		}
		this->ResponseQueue.Queue[this->ResponseQueue.QueueHead].WaitTimedOut = false;
		this->ResponseQueue.Queue[this->ResponseQueue.QueueHead].Error = false;
		// unlock the queue
		pthread_mutex_unlock(&(this->ResponseQueueLock_mutex));
	}

	/*
	 * Wait for the expected response to arrive
	 */
	uint16_t QueueEntry = this->ResponseQueue.QueueTail;
	struct timeval time;
	uint64_t TimeStart = 0, TimeNow = 0;
	int TimeToWait = MilliSecondsToWait;
	// search the request inside of the queue -> start at the tail, so old request are processed first
	while (QueueEntry != this->ResponseQueue.QueueHead ){
		// check if the response was requested
		if (ExpectedCommand == this->ResponseQueue.Queue[QueueEntry].Request){
			// this is the requested response -> abort the loop; the location is available in QueueEntry
			break;
		}
		// go to the next entry
		QueueEntry++;
		// make sure to stay inside of the queue
		if (QueueEntry >= REHAMOVE_RESPONSE_QUEUE_SIZE){
			QueueEntry = 0;
		}
	}
	// Prepare for waiting
	if (TimeToWait > 0){
		gettimeofday(&time,NULL);
		TimeStart = (uint64_t)(time.tv_sec*1000.0) + (uint64_t)(time.tv_usec/1000.0);
	}
	do {
		// check for ACKs and process them
		RehaMove3::ReadAcksBlocking();
		// check if the response was received
		if (this->ResponseQueue.Queue[QueueEntry].ResponseReceived){
			// the response was received -> abort the loop and deal with the response
			break;
		}
		if (MilliSecondsToWait > 0){
			usleep(500);
			gettimeofday(&time,NULL);
			TimeNow = (uint64_t)(time.tv_sec*1000.0) + (uint64_t)(time.tv_usec/1000.0);
			TimeToWait = MilliSecondsToWait - (int)(TimeNow - TimeStart);
		}
		if (this->rmpStatus.InitThreatRunning && !this->rmpStatus.InitThreatActive){
			TimeToWait = 0;
		}
	} while (TimeToWait > 0);

	// check for a timeout
	if (!this->ResponseQueue.Queue[QueueEntry].ResponseReceived){
		if (DoWait) {
			// the response was not received -> timeout occurred
			// lock the queue
			pthread_mutex_lock(&(this->ResponseQueueLock_mutex));
			this->ResponseQueue.Queue[QueueEntry].WaitTimedOut = true;
			// unlock the queue
			pthread_mutex_unlock(&(this->ResponseQueueLock_mutex));
			// return a -1 because of the timeout and nothing was received
			return -1;
		} else {
			// do not wait -> the response can not be there yet
			return (int) ExpectedCommand;
		}
	}

	/*
	 * Process the received response
	 */
	SingleResponse_t Response;
	// lock the queue
	pthread_mutex_lock(&(this->ResponseQueueLock_mutex));
	memcpy(&Response, &(this->ResponseQueue.Queue[QueueEntry]), sizeof(Response));
	// remove response from the queue
	this->ResponseQueue.QueueTail++;
	if (this->ResponseQueue.QueueTail >= REHAMOVE_RESPONSE_QUEUE_SIZE){
		this->ResponseQueue.QueueTail = 0;
	}
	// unlock the queue
	pthread_mutex_unlock(&(this->ResponseQueueLock_mutex));

	/*
	 * Error Handler 2
	 */
	if (Response.Error){
		switch (Response.Ack.result) {
		case Smpt_Result_Successful: 			// No error, command execution is started
		case Smpt_Result_Electrode_Error: 		// Electrode error happened during stimulation.
			// already handle in the Error Handler 1 -> should not occur here
			break;

		case Smpt_Result_Transfer_Error:      	// Checksum or length mismatch
			sprintf(Response.ErrorDescription, "Checksum or length mismatch!");
			break;
		case Smpt_Result_Parameter_Error: 		// At least one parameter value is wrong or missing
			sprintf(Response.ErrorDescription, "At least one parameter value is wrong or missing!");
			break;
		case Smpt_Result_Protocol_Error:  		//  The protocol version is not supported
			sprintf(Response.ErrorDescription, "The protocol version is not supported!");
			break;
		case Smpt_Result_Invalid_Cmd_Error: 	// Stimulation device can not process command.
			sprintf(Response.ErrorDescription, "Stimulation device can not process command!");
			break;
		case Smpt_Result_Uc_Stim_Timeout_Error:	// There was an internal time out. This might be an hardware error
			sprintf(Response.ErrorDescription, "There was an internal time out. This might be an hardware error!");
			break;
		case Smpt_Result_Not_Initialized_Error: // The stimulation device was not initialized using Ll_init
			sprintf(Response.ErrorDescription, "The stimulation device was not initialised using LL_init!");
			break;
		case Smpt_Result_Fuel_Gauge_Error: 		// The fuel gauge is not responding.
			sprintf(Response.ErrorDescription, "The fuel gauge is not responding!");
			break;
		case Smpt_Result_Hv_Error: 				// The HV was not setup
			sprintf(Response.ErrorDescription, "The stimulation voltage could not be set up!");
			break;

			// Error IDs 5-6, 9, 12-16, 18-20, 22-26 are not implemented
		default:
			// unknown return result
			RehaMove3::printMessage(printMSG_error, "%s Error: UNKNOWN acknowledgement result received! (ack result id = %i)\n", this->DeviceIDClass, (int)Response.Ack.result);
		}

		// Print a error message
		RehaMove3::printMessage(printMSG_error, "%s Error: The stimulator reported an error for the command: %u!\n     -> Error code: %u  -> %s\n", this->DeviceIDClass, Response.Ack.command_number, Response.Ack.result, Response.ErrorDescription);
		return ((int)Response.Ack.result) * -1;
	}

	/*
	 * Response Handler 2
	 * -> react on specific acknowledgements and/or errors if the response is not handled in the response handler 1
	 */
	switch (Response.Ack.command_number) {
	/*
	 * General commands
	 */

	/*
	 * LowLevel commands
	 */

	/*
	 * Data Measurement commands
	 */

	}

	// done
	if (this->rmpInitSettings.DebugConfig.printSendCmdInfos){
		gettimeofday(&TStop,NULL);
		EndTime = (double)(TStop.tv_sec*1000.0) + (double)(TStop.tv_usec/1000.0);
		RehaMove3::printMessage(printMSG_rmpSendCMD, "   -> ACK %u needed %f ms to finish\n", Response.Request, (EndTime - StartTime));
	}
	return (int) ExpectedCommand;
}


void RehaMove3::PutChannelResponseExpectation(uint64_t SequenceNumber, Smpt_Channel Channel, uint8_t PackageNumber)
{
	/*
	 * Add the expected stimulation response to the queue
	 */
	// lock the queue
	pthread_mutex_lock(&(this->SequenceQueueLock_mutex));

	// add a new sequence or add a pulse to the last one?
	if (this->SequenceQueue.Queue[this->SequenceQueue.QueueHead].SequenceNumber != SequenceNumber){
		// yes, this a new sequence
		// increase the counter?
		if (this->SequenceQueue.QueueSize != 0){
			this->SequenceQueue.QueueHead++;
			if (this->SequenceQueue.QueueHead >= REHAMOVE_SEQUENCE_QUEUE_SIZE){
				this->SequenceQueue.QueueHead = 0;
			}
			if (this->SequenceQueue.QueueHead == this->SequenceQueue.QueueTail) {
				// queue end was overrun -> we lose data
				if (!this->SequenceQueue.DoNotReportUnclaimedSequenceResults){
					RehaMove3::printMessage(printMSG_error, "%s Error: The sequence acknowlegement queue did overflow!\n     -> The oldest sequence will be discarded.\n     -> This might happen, because the sequence acknowlegements are not pulled!\n     -> This is the ONLY message about the error!\n", this->DeviceIDClass);
					// show this warning only once
					this->SequenceQueue.DoNotReportUnclaimedSequenceResults = true;
				}
				this->SequenceQueue.QueueTail++;
				if (this->SequenceQueue.QueueTail >= REHAMOVE_SEQUENCE_QUEUE_SIZE){
					this->SequenceQueue.QueueTail = 0;
				}
			}
		}
		// clear the sequence and set the defaults
		this->SequenceQueue.QueueSize++;
		this->SequenceQueue.Queue[this->SequenceQueue.QueueHead].NumberOfPulses = 0;
		this->SequenceQueue.Queue[this->SequenceQueue.QueueHead].NumberOfAcks = 0;
		this->SequenceQueue.Queue[this->SequenceQueue.QueueHead].SequenceNumber = SequenceNumber;
		this->SequenceQueue.Queue[this->SequenceQueue.QueueHead].SequenceWasSuccessful = false;
	}

	// add the response expectation to the queue
	this->SequenceQueue.Queue[this->SequenceQueue.QueueHead].StimulationPulse[this->SequenceQueue.Queue[this->SequenceQueue.QueueHead].NumberOfPulses].Channel = Channel;
	this->SequenceQueue.Queue[this->SequenceQueue.QueueHead].StimulationPulse[this->SequenceQueue.Queue[this->SequenceQueue.QueueHead].NumberOfPulses].PackageNumber = PackageNumber;
	this->SequenceQueue.Queue[this->SequenceQueue.QueueHead].NumberOfPulses++;

	// unlock the queue
	pthread_mutex_unlock(&(this->SequenceQueueLock_mutex));
}

void RehaMove3::PutChannelResponse(uint8_t PackageNumber, Smpt_Result Result, Smpt_Channel ChannelError)
{
	/*
	 * Update the expected stimulation response with the response
	 */
	// lock the queue
	pthread_mutex_lock(&(this->SequenceQueueLock_mutex));

	// search backwards for the last stimulation pulse with no ack ( and were the package number match )
	bool DoSearch = true, PulseFound = false;
	int16_t SequenceQueuePointer = this->SequenceQueue.QueueHead;
	uint8_t PulsePointer = 0;

	while(DoSearch){
		// is this sequence complete?
		/*
		if (this->SequenceQueue.Queue[SequenceQueuePointer].NumberOfAcks != this->SequenceQueue.Queue[SequenceQueuePointer].NumberOfPulses) {
			// no -> so this ACK must be for the first pulse without an ack, which is the pulse NumberOfAcks because of the 0 index
			PulsePointer = this->SequenceQueue.Queue[SequenceQueuePointer].NumberOfAcks;
			DoSearch = false;
			PulseFound = true;
		}
		*/
		for (uint8_t i = 0; i < this->SequenceQueue.Queue[SequenceQueuePointer].NumberOfPulses; i++){
			if ( this->SequenceQueue.Queue[SequenceQueuePointer].StimulationPulse[i].PackageNumber == PackageNumber){
				// the package number match -> this is the pulse we got a response for
				PulsePointer = i;
				DoSearch = false;
				PulseFound = true;
				break;
			}
		}

		if (DoSearch){
			// the pulse was not found in the last sequence -> look in the sequence before
			SequenceQueuePointer--;
			if (SequenceQueuePointer < 0){
				SequenceQueuePointer = REHAMOVE_SEQUENCE_QUEUE_SIZE;
			}
			if (SequenceQueuePointer < this->SequenceQueue.QueueTail) {
				// queue end is reached and no pulse was found -> this should not happen ...
				RehaMove3::printMessage(printMSG_rmpErrorUnclaimedSequence, "%s Error: The received acknowledgement for a stimulation pulse could not be found in the sequence queue!\n     -> Package Number: %d\n     -> Result: %s\n     -> Channel Error: %d\n",
					this->DeviceIDClass, PackageNumber, RehaMove3::GetResultString(Result), ChannelError);
				DoSearch = false;
			}
		}
	}

	// update the found pulse
	if (PulseFound){
		this->SequenceQueue.Queue[(uint8_t)SequenceQueuePointer].NumberOfAcks++;
		this->SequenceQueue.Queue[(uint8_t)SequenceQueuePointer].StimulationPulse[PulsePointer].Result = (uint8_t)Result;
		// check the result
		switch (Result){
		case Smpt_Result_Successful:
			// stimulation pulse was successful -> end now
			this->Stats.StimultionPulsesSuccessful++;
			break;
		case Smpt_Result_Electrode_Error:
			// a stimulation error was detected
			this->Stats.StimultionPulsesFailed++;
			this->Stats.StimultionPulsesFailed_StimError++;
			break;
		default:
			RehaMove3::printMessage(printMSG_error,"%s Error: the result code %d is not handled in function 'PutChannelResponse'\n", this->DeviceIDClass, Result);
		}

		// was this the last pulse of this sequence?
		if (this->SequenceQueue.Queue[(uint8_t)SequenceQueuePointer].NumberOfAcks == this->SequenceQueue.Queue[(uint8_t)SequenceQueuePointer].NumberOfPulses){
			bool SequenceWasSuccessful = true, StimErrorsOccurred = false;
			char ErrorString[1000], tempString[150];
			memset(ErrorString, 0, sizeof(ErrorString));

			// make sure every pulse within the sequence was successful
			for (uint8_t i = 0; i < this->SequenceQueue.Queue[(uint8_t)SequenceQueuePointer].NumberOfAcks; i++) {

				switch (this->SequenceQueue.Queue[(uint8_t)SequenceQueuePointer].StimulationPulse[i].Result){
				case Smpt_Result_Successful:
					// stimulation pulse was successful -> end now
					//this->Stats.StimultionPulsesSuccessful++;
					break;
				case Smpt_Result_Electrode_Error:
					// a stimulation error was detected
					SequenceWasSuccessful = false;
					StimErrorsOccurred = true;
					//this->Stats.StimultionPulsesFailed++;
					//this->Stats.StimultionPulsesFailed_StimError++;
					snprintf(tempString, sizeof(tempString), "   -> Electrode Error => Pulse Number: %d; Channel: %d (%s)\n",
							(i+1), ((int8_t)this->SequenceQueue.Queue[(uint8_t)SequenceQueuePointer].StimulationPulse[i].Channel+1), RehaMove3::GetChannelNameString((Smpt_Channel)this->SequenceQueue.Queue[(uint8_t)SequenceQueuePointer].StimulationPulse[i].Channel));
					strcat(ErrorString, tempString);
					break;

				default:
					SequenceWasSuccessful = false;
					snprintf(ErrorString, sizeof(ErrorString), "%s     -> UNDEFINED Error => Pulse Number: %d; Channel: %d (%s) Result: %d\n",
							ErrorString, (PulsePointer+1), ((int8_t)ChannelError+1), RehaMove3::GetChannelNameString(ChannelError), Result);
					RehaMove3::printMessage(printMSG_error,"%s Error: the result code %d is not handled in function 'PutChannelResponse'\n", this->DeviceIDClass, Result);
				}

			} // end for loop

			if (SequenceWasSuccessful){
				// no error occurred
				this->Stats.SequencesSuccessful++;
				this->SequenceQueue.Queue[(uint8_t)SequenceQueuePointer].SequenceWasSuccessful = true;
			} else {
				// an error occurred
				RehaMove3::printMessage(printMSG_rmpSequenceError, "\n%s Sequence Error: Sequence %ld FAILED! (time=%fs)\n%s",
					this->DeviceIDClass, this->SequenceQueue.Queue[(uint8_t)SequenceQueuePointer].SequenceNumber, RehaMove3::GetCurrentTime(true), ErrorString );
				this->Stats.SequencesFailed++;

				/*
				 * Handle Stimulation Errors e.g. electrode errors and check if the stimulation should be continued
				 */
				this->rmpStatus.NumberOfStimErrors++;
				if ((this->rmpStatus.NumberOfStimErrors >= this->rmpSettings.NumberOfErrorsAfterWhichToAbort) && (this->rmpSettings.NumberOfErrorsAfterWhichToAbort != 0)){
					this->rmpStatus.DoNotStimulate = true;
					if (this->rmpSettings.NumberOfSequencesAfterWhichToRetestForError != 0){
						this->rmpStatus.NumberOfSequencesUntilErrorRetest = this->rmpSettings.NumberOfSequencesAfterWhichToRetestForError;
						this->rmpStatus.DoReTestTheStimError = true;
						snprintf(ErrorString, sizeof(ErrorString), "The stimulation will be resumed after %0.2f sec!", (float)( (float)this->rmpSettings.NumberOfSequencesAfterWhichToRetestForError/(float)this->rmpSettings.StimFrequency));
					} else {
						this->rmpStatus.DoReTestTheStimError = false;
						this->rmpStatus.NumberOfSequencesUntilErrorRetest = 0xFFFF;
						snprintf(ErrorString, sizeof(ErrorString), "The stimulation will never be resumed!");
					}
					RehaMove3::printMessage(printMSG_rmpSequenceError, "%s Stimulation Errors: %u Sequence FAILED!\n   -> The stimulation will be DISABLED!\n   -> PLEASE CHECK THE SETUP AND THE SETTINGS!\n\n   -> RE-TEST: %s\n",
										this->DeviceIDClass, this->rmpStatus.NumberOfStimErrors, ErrorString );
				}

				if (StimErrorsOccurred) {
					this->Stats.SequencesFailed_StimError++;
				}
			}
		}
	}

	// unlock the queue
	pthread_mutex_unlock(&(this->SequenceQueueLock_mutex));
}


bool RehaMove3::NewStatusUpdateReceived(uint32_t MilliSecondsToWait)
{
	/*
	 * Wait for the expected response to arrive
	 */
	struct timeval time;
	uint64_t TimeStart = 0, TimeNow = 0;
	int TimeToWait = MilliSecondsToWait;
	// Prepare for waiting
	if (TimeToWait > 0){
		gettimeofday(&time,NULL);
		TimeStart = (uint64_t)(time.tv_sec*1000.0) + (uint64_t)(time.tv_usec/1000.0);
	}
	uint64_t OldStatusUpdateTime = this->rmpStatus.LastUpdated;
	do {
		// check for ACKs and process them
		RehaMove3::ReadAcksBlocking();
		// check if the time of the last update changed
		if (OldStatusUpdateTime != this->rmpStatus.LastUpdated){
			// something changed -> abort the loop
			return true;
			break;
		}
		if (MilliSecondsToWait > 0){
			usleep(500);
			gettimeofday(&time,NULL);
			TimeNow = (uint64_t)(time.tv_sec*1000.0) + (uint64_t)(time.tv_usec/1000.0);
			TimeToWait = MilliSecondsToWait - (int)(TimeNow - TimeStart);
		}
	} while (TimeToWait > 0);

	// timeout occurred
	return false;
}


double RehaMove3::GetCurrentTime(void){
	return RehaMove3::GetCurrentTime(true);
}
double RehaMove3::GetCurrentTime(bool DoUpdate)
{
	if (DoUpdate){
		struct timeval time;
		gettimeofday(&time,NULL);
		uint64_t Time_ms = (uint64_t)(time.tv_sec*1000.0) + (uint64_t)(time.tv_usec/1000.0);
		this->rmpStatus.CurrentTime_ms = Time_ms - this->rmpStatus.StartTime_ms;
	}
	return ((double)this->rmpStatus.CurrentTime_ms) / 1000.0;
}

uint8_t RehaMove3::GetPackageNumber() {
	uint8_t PackageNumber_old = this->rmpStatus.LocalPackageNumber;
	this->rmpStatus.LocalPackageNumber++;
	if (this->rmpStatus.LocalPackageNumber > 63) {
		this->rmpStatus.LocalPackageNumber = 0;
	}
	return PackageNumber_old;
}

const char* RehaMove3::GetResultString(Smpt_Result Result)
{
	switch (Result){
	case Smpt_Result_Successful:					return "was SUCCESSFUL"; break;
	case Smpt_Result_Transfer_Error:         		return "ERROR: Transfer_Error"; break;
	case Smpt_Result_Parameter_Error:        		return "ERROR: Parameter_Error"; break;
	case Smpt_Result_Protocol_Error:         		return "ERROR: Protocol_Error"; break;
	case Smpt_Result_Uc_Stim_Timeout_Error:  		return "ERROR: Uc_Stim_Timeout_Error"; break;
	case Smpt_Result_Not_Initialized_Error:  		return "ERROR: Not_Initialized_Error"; break;
	case Smpt_Result_Hv_Error:               		return "ERROR: Hv_Error"; break;
	case Smpt_Result_Electrode_Error:        		return "ERROR: Electrode_Error"; break;
	case Smpt_Result_Invalid_Cmd_Error:      		return "ERROR: Invalid_Cmd_Error"; break;
	case Smpt_Result_Pulse_Timeout_Error:         	return "ERROR: Pulse_Timeout_Error"; break;
	case Smpt_Result_Fuel_Gauge_Error:            	return "ERROR: Fuel_Gauge_Error"; break;
	case Smpt_Result_Live_Signal_Error:           	return "ERROR: Live_Signal_Error"; break;
	case Smpt_Result_File_Transmission_Timeout:   	return "ERROR: File_Transmission_Timeout"; break;
	case Smpt_Result_File_Not_Found:              	return "ERROR: File_Not_Found"; break;
	case Smpt_Result_Busy:                        	return "ERROR: Busy"; break;
	case Smpt_Result_File_Error:                  	return "ERROR: File_Error"; break;
	case Smpt_Result_Flash_Erase_Error:          	return "ERROR: Flash_Erase_Error"; break;
	case Smpt_Result_Flash_Write_Error:          	return "ERROR: Flash_Write_Error"; break;
	case Smpt_Result_Unknown_Controller_Error:   	return "ERROR: Unknown_Controller_Error"; break;
	case Smpt_Result_Firmware_Too_Large_Error:   	return "ERROR: Firmware_Too_Large_Error"; break;
	default:
		RehaMove3::printMessage(printMSG_error, "%s Error: for the result %d is no string present -> fix 'GetResultString'", this->DeviceIDClass, Result);
		return "NOT Defined";
	}
}

const char* RehaMove3::GetChannelNameString(Smpt_Channel Channel)
{
	switch (Channel){
	case Smpt_Channel_Red:			return "red"; break;
	case Smpt_Channel_Blue:			return "blue"; break;
	case Smpt_Channel_Black:		return "black"; break;
	case Smpt_Channel_White:		return "white"; break;
	case Smpt_Channel_Undefined:	return "undefined"; break;
	default:
		RehaMove3::printMessage(printMSG_error,"%s ERROR: for the channel %d is no name present -> fix 'GetChannelName'", this->DeviceIDClass, Channel);
		return "NOT Defined";
	}
}

void RehaMove3::printMessage(printMessageType_t type, const char *format, ... )
{
	va_list args;
	va_start( args, format );
	switch (type){
	case printMSG_general:
		vprintf(format, args );
		break;
	case printMSG_warning:
		if (this->rmpInitSettings.DebugConfig.useColors){
			// color codes: http://misc.flogisoft.com/bash/tip_colors_and_formatting
			printf("\033[1m\033[93m"); // bold and light yellow
		}
		vprintf(format, args );
		if (this->rmpInitSettings.DebugConfig.useColors){
			printf("\033[0m");
		}
		break;
	case printMSG_error:
		if (this->rmpInitSettings.DebugConfig.useColors){
			printf("\n\033[1m\033[91m"); // bold and light red
		}
		vprintf(format, args );
		if (this->rmpInitSettings.DebugConfig.useColors){
			printf("\033[0m\n");
		}
		break;
	case printMSG_rmpDeviceInfo:
		if (this->rmpInitSettings.DebugConfig.printDeviceInfos){
			vprintf(format, args );
		}
		break;
	case printMSG_rmpInitInfo:
		if (this->rmpInitSettings.DebugConfig.printInitInfos){
			vprintf(format, args );
		}
		break;
	case printMSG_rmpInitParam:
		if (this->rmpInitSettings.DebugConfig.printInitSettings){
			vprintf(format, args );
		}
		break;
	case printMSG_rmpSendCMD:
		if (this->rmpInitSettings.DebugConfig.printSendCmdInfos){
			vprintf(format, args );
		}
		break;
	case printMSG_rmpReceiveACK:
		if (this->rmpInitSettings.DebugConfig.printReceivedAckInfos){
			vprintf(format, args );
		}
		break;
	case printMSG_rmpPulseConfig:
		if (this->rmpInitSettings.DebugConfig.printStimInfos){
			vprintf(format, args );
		}
		break;
	case printMSG_rmpErrorUnclaimedSequence:
		if (this->rmpInitSettings.DebugConfig.printErrorsSequence){
			if (this->rmpInitSettings.DebugConfig.useColors){
				printf("\033[1m\033[93m"); // bold and light yellow
			}
			vprintf(format, args );
			if (this->rmpInitSettings.DebugConfig.useColors){
				printf("\033[0m");
			}
		}
		break;
	case printMSG_rmpWarningCorrectionChargeInbalace:
		if (this->rmpInitSettings.DebugConfig.printCorrectionChargeWarnings){
			if (this->rmpInitSettings.DebugConfig.useColors){
				printf("\033[1m\033[93m"); // bold and light yellow
			}
			vprintf(format, args );
			if (this->rmpInitSettings.DebugConfig.useColors){
				printf("\033[0m");
			}
		}
		break;
	case printMSG_rmpSequenceError:
		if (this->rmpInitSettings.DebugConfig.printErrorsSequence){
			if (this->rmpInitSettings.DebugConfig.useColors){
				printf("\n\033[1m\033[91m"); // bold and light red
			}
			vprintf(format, args );
			if (this->rmpInitSettings.DebugConfig.useColors){
				printf("\033[0m\n");
			}
		}
		break;
	case printMSG_rmpStats:
		if (this->rmpInitSettings.DebugConfig.printStats){
			vprintf(format, args );
		}
		break;
	default:
		printf("\nRMP ERROR: unsupported printf type: %u....\n\n", (uint16_t)type);
	}
	va_end( args );
}


void RehaMove3::printBits(char *msgString, void const * const ptr, size_t const size)
{
	unsigned char *b = (unsigned char*) ptr;
	unsigned char byte = 0x00;
    uint16_t i_msg = 0;
    sprintf(&msgString[i_msg++], "|");
	for (uint16_t i = 0; i < (uint16_t) size; i++) {
		for (int8_t j = 7; j >= 0; j--) {
			byte = b[i] & (1 << j);
			byte >>= j;
			sprintf(&msgString[i_msg++], "%u", byte);
		}
		sprintf(&msgString[i_msg++], "|");
	}
}

void RehaMove3::printSupportedVersion(char *msgString, const uint8_t SupportedVersions[][3])
{
	uint8_t iString = 0;
	uint8_t NumberOfSupportedVersions = 0;
	if (SupportedVersions == RMP_SupportedVersionsMain){
		NumberOfSupportedVersions = RMP_NumberOfSupportedVersionsMain;
	}
	if (SupportedVersions == RMP_SupportedVersionsStim){
		NumberOfSupportedVersions = RMP_NumberOfSupportedVersionsStim;
	}
	if (SupportedVersions == RMP_SupportedVersionsSMPT){
		NumberOfSupportedVersions = RMP_NumberOfSupportedVersionsSMPT;
	}

	for (uint8_t i = 0; i< NumberOfSupportedVersions; i++){
		sprintf(&msgString[iString], "%u.%u.%u,    ", SupportedVersions[i][0], SupportedVersions[i][1], SupportedVersions[i][2]);
		iString += 8;
	}
	msgString[iString -3] = 0x00;
}

} // namespace

