///////////////////////////////////////////////////////////////////////////////
/// \file Terminal.c
///
///	\author Ronald Sousa @Opticalworm
///////////////////////////////////////////////////////////////////////////////
#include "common.h"
#include "MCU/led.h"
#include "MCU/usart2.h"
#include "MCU/tick.h"

///////////////////////////////////////////////////////////////////////////////
/// \brief Defines our terminal buffer size which in turn set the longest command
///////////////////////////////////////////////////////////////////////////////
#define TERMINAL_BUFFER_SIZE 25

///////////////////////////////////////////////////////////////////////////////
/// \brief Keep track of the number of bytes we received from the computer
///////////////////////////////////////////////////////////////////////////////
static uint32_t NumberOfByteReceived;

///////////////////////////////////////////////////////////////////////////////
/// \brief The terminal data buffer
///////////////////////////////////////////////////////////////////////////////
static uint8_t Buffer[TERMINAL_BUFFER_SIZE];


///////////////////////////////////////////////////////////////////////////////
/// \brief this message has the system information data that we want to display
/// on the terminal
///////////////////////////////////////////////////////////////////////////////
static const uint8_t SystemMessageString[] = "-----------------------------------\r\n"
											"Firm : " FIRMWARE_VERSION "\r\n"
											"Hard : " HARDWARE_VERSION "\r\n"
											COMPILED_DATA_TIME "\r\n"
											"-----------------------------------\r\n";

///////////////////////////////////////////////////////////////////////////////
/// \brief Defines the parameter data type
///////////////////////////////////////////////////////////////////////////////
typedef struct {
	uint8_t Type;		///< Parameter data type. S = command, U = integer
	uint32_t Value;		///< parameter data
} ParamStructureType;

///////////////////////////////////////////////////////////////////////////////
/// \brief Defines the list of parameter data structure
///////////////////////////////////////////////////////////////////////////////
typedef struct {

	uint32_t NumberOfParameter;		///< used to keep track of the number of parameter we haver
	ParamStructureType List[10];	///< hold the parameter
} ListOfParameterStructureType;

///////////////////////////////////////////////////////////////////////////////
/// \brief holds our current list of parameter extracted by ProcessData
///
///	\sa ProcessData
///////////////////////////////////////////////////////////////////////////////
static ListOfParameterStructureType ParameterList;


///////////////////////////////////////////////////////////////////////////////
/// \brief Send the system information to the computer. This first clear the
///	computer terminal screen.
///////////////////////////////////////////////////////////////////////////////
static void DisplaySystemInformation(void)
{
	SerialPort2.SendByte(0x0C); // clear terminal
    SerialPort2.SendString(&SystemMessageString[0]);
}

///////////////////////////////////////////////////////////////////////////////
/// \brief Init the terminal program
///////////////////////////////////////////////////////////////////////////////
void Terminal_Init(void)
{
    Led_Init();
    Tick_init();
    SerialPort2.Open(115200);

    NumberOfByteReceived = 0;
    DisplaySystemInformation();
}

///////////////////////////////////////////////////////////////////////////////
/// \brief process the buffer data and extract the commands
///
///	\return TRUE success
///
///	\todo update the document return state value. need to implement
///////////////////////////////////////////////////////////////////////////////
static int_fast8_t ProcessData(void)
{

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
/// \brief run the terminal command
///
///	\return TRUE success
///
///	\todo update the document return state value. need to implement
///////////////////////////////////////////////////////////////////////////////
static int_fast8_t RunCommand(void)
{

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
/// \brief process the buffer data and extract the commands
///
///	\return TRUE success. FALSE no error
///
///	\todo update the document return state value. need to implement
///////////////////////////////////////////////////////////////////////////////
int_fast8_t Terminal_Process(void)
{
	uint8_t SerialTempData = 0; // hold the new byte from the serial fifo
	int_fast8_t Result = FALSE;

	Result = SerialPort2.GetByte(&SerialTempData);

	if ( TRUE != Result )
	{
		return Result;
	}

	// echo the user command
	SerialPort2.SendByte(SerialTempData);

	if ('\r' == SerialTempData)
	{
		if (NumberOfByteReceived)
		{
			/// \todo call the process data
			if ( TRUE == ProcessData() )
			{
				if(  TRUE == RunCommand() )
				{
					Result =  TRUE;
				}

			}
		}
		else
		{
			Result =  FALSE;
		}
	}
	else if ( (SerialTempData >= '0' && SerialTempData <= '9') ||
			(SerialTempData >= 'A' && SerialTempData <= 'Z') ||
			(SerialTempData >= 'a' && SerialTempData <= 'z') )

	{
		if ( NumberOfByteReceived < TERMINAL_BUFFER_SIZE )
		{
			Buffer[NumberOfByteReceived] = SerialTempData;
			NumberOfByteReceived++;
			return FALSE;
		}

	}

	// reset buffer by reseting the counter
	NumberOfByteReceived = 0;
	return Result;


}









