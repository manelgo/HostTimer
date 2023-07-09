#ifndef _COMMON_GLOBALS_WEBTIMER_H
#define _COMMON_GLOBALS_WEBTIMER_H

/////////////////////////////////////////////////////////////////////////////
/**
 *  @file   CommonGlobalsWebTimer.h
 *  @author Manel Gonzalez Farrera
 *  @date   March 2018
 *  @brief  Definition of Common Globals WebTimer
 */
/////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
// GLOBAL CONSTANTS
///////////////////////////////////////////////////////////////////////////////////////////////////

const char         PROGRAM_INFO_FILE_NAME[20] = "Program.info";
const char          PROGRAM_SET_FILE_NAME[20] = "Program.set";
const char             CHANNELS_FILE_NAME[20] = "Program.channels";
const char               GUARDS_FILE_NAME[20] = "Program.guards";
const char  PROGRAM_UPDATE_LIST_FILE_NAME[20] = "Program.update.list";
const char   PROGRAM_UPDATE_TAR_FILE_NAME[20] = "Program.update.tar";
const char        WIFI_SETTINGS_FILE_NAME[20] = "WiFi.settings";
const char PROGRAM_UPDATED_FLAG_FILE_NAME[20] = "Program.updated.txt";
const uint8_t                    NUM_CHANNELS = 20u;
const unsigned int PROGRAM_FILE_SIZE_IN_BYTES = 10080u;


#endif // _COMMON_GLOBALS_WEBTIMER_H

