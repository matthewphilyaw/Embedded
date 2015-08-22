///////////////////////////////////////////////////////////////////////////////
/// \file SerialInterface.h
/// \author Ronald Sousa
/// \website www.HashDefineElectronics.com
/// \company Hash Define Electronics Ltd
///
/// \brief  this is the serial terminal interface used to debug and test the CPU
///////////////////////////////////////////////////////////////////////////////
#include "common.h"

#ifdef EN_DEBUG_INTERFACE

#include "MCU/tick.h"
#include "MCU/SeriaStructure.h"
#include "Terminal/DebugInterface.h"
#include "Terminal/Debug_MainMenu.h"

///////////////////////////////////////////////////////////////////////////////
/// \brief Holds our serial instance
///////////////////////////////////////////////////////////////////////////////
static SerialInterface *COMPort;

///////////////////////////////////////////////////////////////////////////////
/// \brief Maximum serial buffer
///////////////////////////////////////////////////////////////////////////////
#define READ_BUFFER_LENGTH 256

///////////////////////////////////////////////////////////////////////////////
/// \brief set the process update rate to 100ms
///////////////////////////////////////////////////////////////////////////////
static TickType ProcessUpdateRate = { 0 , 500} ;

///////////////////////////////////////////////////////////////////////////////
/// \brief Define the read buffer which all data is store while we wait
/// for the end of line transmission
///////////////////////////////////////////////////////////////////////////////
uint8_t ReadBuffer[READ_BUFFER_LENGTH];

///////////////////////////////////////////////////////////////////////////////
/// \brief ReadBuffer pointer
///////////////////////////////////////////////////////////////////////////////
uint8_t * ReadBufferPointer = ReadBuffer;

///////////////////////////////////////////////////////////////////////////////
/// \brief how many bytes we have in the read buffer
///////////////////////////////////////////////////////////////////////////////
uint32_t ReadBufferLength = 0;

///////////////////////////////////////////////////////////////////////////////
/// \brief this is the number of TRANS_END_BYTE byte in a row we need before we can trigger
/// a system reset.
/// \sa ResetCounter TRANS_END_BYTE
///////////////////////////////////////////////////////////////////////////////
#define RESET_COUNTER_TRIGGER 3

///////////////////////////////////////////////////////////////////////////////
/// \brief Define the number of reset command we received
/// \sa RESET_COUNTER_TRIGGER
///////////////////////////////////////////////////////////////////////////////
static uint32_t ResetCounter = 0;

///////////////////////////////////////////////////////////////////////////////
/// \brief Define the end of transmission byte
///////////////////////////////////////////////////////////////////////////////
#define TRANS_END_BYTE 0x0D

///////////////////////////////////////////////////////////////////////////////
/// \brief the escape character will reset the current propmt and redraw the menu
///////////////////////////////////////////////////////////////////////////////
#define DRAW_MENU_KEY 0x1B

///////////////////////////////////////////////////////////////////////////////
/// \brief Holds the current menu pointer
///////////////////////////////////////////////////////////////////////////////
SDMenuStruture  *ActiveMenuPointer = &DefaultMenu;

///////////////////////////////////////////////////////////////////////////////
/// \brief Holds the process data packet
///////////////////////////////////////////////////////////////////////////////
static SDebugType SDebugSystem;

///////////////////////////////////////////////////////////////////////////////
/// \brief Portal layer for serial send byte
///
/// \param source byte to write
///////////////////////////////////////////////////////////////////////////////
void SDebug_WriteByte(uint8_t source)
{
    if ( COMPort ) // make sure that we can actual send data
    {
        COMPort->SendByte(source);
    }
}

///////////////////////////////////////////////////////////////////////////////
/// \brief Write serial string
///
/// \param source pointer to the string to write. must end with null
///////////////////////////////////////////////////////////////////////////////
void SDebug_WriteString(uint8_t *source)
{
    if ( COMPort ) // make sure that we can actual send data
    {
        COMPort->SendString(source);
    }
}

///////////////////////////////////////////////////////////////////////////////
/// \brief Portal layer for reading byte
///
/// \param source byte to write
///////////////////////////////////////////////////////////////////////////////
static inline uint8_t SDebug_ReadByte(void)
{
    uint8_t Result = 0;
    if ( COMPort ) // make sure that we can actual send data
    {
        if ( COMPort->GetByte(&Result) )
        {
            return Result;
        }

        Result = 0; // error return 0
    }

    return Result;
}

///////////////////////////////////////////////////////////////////////////////
/// \brief Portal layer for checking is we have data available to read
///
/// \return TRUE when there is no data to read
///////////////////////////////////////////////////////////////////////////////
static inline uint8_t SDebug_IsSerialBufferEmpty(void)
{
    if ( COMPort ) // make sure that we can actual send data
    {
        if ( COMPort->DoesReceiveBufferHaveData() )
        {
            return FALSE;
        }
    }

    // fail with a TRUE.
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
/// \brief This function handles the drawing of the command header
/// ie. the micro robot and hash define electronics detail
///////////////////////////////////////////////////////////////////////////////
static void SDbug_DrawHeader(void)
{
    SDebug_WriteString( "\t [@.@]\n\r"
                        "\t/|___|\\\n\r"
                        "\t d   b\n\r");
    SDebug_WriteString("Ronald Sousa\n\r");
    SDebug_WriteString("HashDefineElectronics.com\n\r\n\r");
}

///////////////////////////////////////////////////////////////////////////////
/// \brief Draws the system prompt
///////////////////////////////////////////////////////////////////////////////
static void SDebug_DrawPrompt(void)
{
    SDebug_WriteString("\n\r[");
    if(ActiveMenuPointer && ActiveMenuPointer->MenuTitle)
    {
        SDebug_WriteString(ActiveMenuPointer->MenuTitle);
    }
    SDebug_WriteString("] > ");
}

///////////////////////////////////////////////////////////////////////////////
/// \brief This function is used to send error code back to the user
///
/// \param ack if TRUE then success message is send else the error code
/// \param code ack = TRUE then, the received command  [SDebugType.Command]
///     ack = FALSE, then the error code.
///
/// \param code ack = TRUE then, the number of parameter detected [SDebugType.ParameterLength]
///     ack = FALSE, ignored
///
///////////////////////////////////////////////////////////////////////////////
void SDebug_SendAcknowledgement(uint_fast8_t ack, uint32_t code, uint32_t parameterLength)
{
    uint8_t Buffer[20];

    if( ack )
    {
        snprintf(&Buffer[0], 20, "\n\r%c%u %c%u\n\r",SDEnum_S, code, SDEnum_U, parameterLength);
    }
    else
    {
        snprintf(&Buffer[0], 20, "\n\r%c%u\n\r",SDEnum_E, code);
    }

    SDebug_WriteString(&Buffer[0]);

    if( !ack )
    {
        SDebug_DrawPrompt();
    }
}

///////////////////////////////////////////////////////////////////////////////
/// \brief Extract system parameter
///
/// \return 0 = success; 1 = source or SystemData pointer error; 2 = too many parameter
/// 3 = no parameter detected, 4 = invalid parameter type, 5= parameter data is too long
///////////////////////////////////////////////////////////////////////////////
uint_fast8_t SDebug_ExtractParameters(uint8_t *source, SDebugType *SystemData)
{
    uint32_t CharacterIndex = 0;

    uint8_t *StringPointerTemp; // used to hold the working parameter string reference for easier data manipulation
    uint32_t ParameterIndex;

    if(!source || !SystemData)
    {
        return 1; // one or all pointers are blank
    }
    else if ( !*source ) // in the event that we might have no data to process
    {
        SDebug_SendAcknowledgement(FALSE,SDErrorEnum_E2, 0);
        return 3; // no parameter detected
    }



    // reset parameter
    SystemData->ParameterLength = 1;
    StringPointerTemp = &SystemData->Parameter[0].String[0];
    *StringPointerTemp = 0;

    // interact through all the data until we have found what we need
    for( ; *source ; source++ )
    {
        *StringPointerTemp = 0;

        if ( ' ' == *source )
        {
            if(SystemData->ParameterLength < SDEBUG_MAX_PARAMETER_SUPPORT)
            {
                // new parameter
                StringPointerTemp = &SystemData->Parameter[SystemData->ParameterLength].String[0];
                SystemData->ParameterLength++;
                CharacterIndex = 0;
            }
            else
            {
                // Too many parameters
                SDebug_SendAcknowledgement(FALSE,SDErrorEnum_E3, 0);
                return 2;
            }
        }
        else
        {
            if ( CharacterIndex < (SDEBUG_STRING_LENGTH -2) )
            {
                *StringPointerTemp = *source;
                StringPointerTemp++;
                *StringPointerTemp = 0;
                CharacterIndex++;
            }
            else
            {
                // parameter data is too large
                SDebug_SendAcknowledgement(FALSE,SDErrorEnum_E5, 0);
                return 5;
            }
        }
    }


    for(ParameterIndex = 0; ParameterIndex <  SystemData->ParameterLength; ParameterIndex++)
    {
        switch(SystemData->Parameter[ParameterIndex].Type)
        {
            /// unsigned integer 32 bit
            case  SDEnum_S:
            case SDEnum_s:

                if( ParameterIndex )
                {
                    SDebug_SendAcknowledgement(FALSE,SDErrorEnum_E4, 0);
                    return 4;
                }

                // fallthrough
            case  SDEnum_U:
            case SDEnum_u:
                SystemData->Parameter[ParameterIndex].Data.ui32_t[0] = atoll(&SystemData->Parameter[ParameterIndex].String[1]);
                break;

            // singed integer 32 bit
            case  SDEnum_I:
            case SDEnum_i:
                SystemData->Parameter[ParameterIndex].Data.i32_t[0] = atoi(&SystemData->Parameter[ParameterIndex].String[1]);
                break;

            // float 32 bit
            case  SDEnum_F:
            case SDEnum_f:
                SystemData->Parameter[ParameterIndex].Data.f32_t[0] = atof(&SystemData->Parameter[ParameterIndex].String[1]);
                break;

            // boolean FALSE
            case  SDEnum_L:
            case SDEnum_l:
                SystemData->Parameter[ParameterIndex].Data.ui32_t[0] = 0x00000000;
                break;

            // boolean TRUE
            case  SDEnum_H:
            case SDEnum_h:
                SystemData->Parameter[ParameterIndex].Data.ui32_t[0] = 0xFFFFFFFF;
                break;
            case  SDEnum_T:
            case SDEnum_t:
                // Text doesn't need processing
                break;

            default: // error invalid parameter
                SDebug_SendAcknowledgement(FALSE,SDErrorEnum_E4, 0);
                return 4;
                break;
        }
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////
/// \brief This function monitor the serial line and process the incoming data
///
/// return TRUE = we have data; FALSE = no data
///////////////////////////////////////////////////////////////////////////////
uint_fast8_t SDebug_Monitor(void)
{
    struct SDMenuStruture  *MenuTemp;

 //  while(!SDebug_IsSerialBufferEmpty() )
    {
        if ( !SDebug_IsSerialBufferEmpty() ) // do we have data
        {
            *ReadBufferPointer = SDebug_ReadByte();

            if ( TRANS_END_BYTE == *ReadBufferPointer) // we detected end of transmission.
            {
                *ReadBufferPointer = 0; // replace the termination character to a null for better string support

                // for a valid command, we need at lease 2 bytes
                if(ReadBufferLength > 1)
                {
                    ResetCounter = 0; // clear the reset counter

                    if (!SDebug_ExtractParameters(&ReadBuffer[0] , &SDebugSystem) )
                    {
                        // The first parameter should be of type U or u for unsigned integer
                        if ( SDEnum_S == SDebugSystem.Parameter[0].Type
                            || SDEnum_s == SDebugSystem.Parameter[0].Type )
                        {
                            SDebug_SendAcknowledgement(TRUE,SDebugSystem.Parameter[0].Data.ui32_t[0], SDebugSystem.ParameterLength);
                            if (ActiveMenuPointer)
                            {
                                MenuTemp = ActiveMenuPointer->MenuPointer(&SDebugSystem);

                                // have we changed screen?
                                if( MenuTemp )
                                {
                                    ActiveMenuPointer = MenuTemp;
                                    CLEAR_TERMINAL_SCREEN();
                                    SDbug_DrawHeader();
                                    ActiveMenuPointer->DrawOptions();
                                }

                            }
                            SDebug_DrawPrompt();
                        }
                        else
                        {
                            SDebug_SendAcknowledgement(FALSE,SDErrorEnum_E0, 0);
                        }
                    }
                }
                else
                {
                    if( ResetCounter < (RESET_COUNTER_TRIGGER -1 ) )
                    {
                        ResetCounter++;
                    }
                    else
                    {
                        SDebug_Init(NULL);
                        ResetCounter = 0;
                        ReadBufferLength = 0;
                        ReadBufferPointer = &ReadBuffer[0];
                        return TRUE;
                    }
                }

                // reset read buffer
                ReadBufferPointer = &ReadBuffer[0];
                ReadBufferLength = 0;
            }
            else if( DRAW_MENU_KEY == *ReadBufferPointer && ActiveMenuPointer)
            {
                CLEAR_TERMINAL_SCREEN();
                SDbug_DrawHeader();
                ActiveMenuPointer->DrawOptions();
                SDebug_DrawPrompt();

                ResetCounter = 0;
                ReadBufferLength = 0;
                ReadBufferPointer = &ReadBuffer[0];
                return TRUE;
            }
            else if( *ReadBufferPointer >= ' ' && *ReadBufferPointer <= '~')
            {
                SDebug_WriteByte(*ReadBufferPointer);

                if (ActiveMenuPointer && ActiveMenuPointer->Process)
                {
                    ResetCounter = 0; // clear the reset counter
                    // pass the screen data
                    ActiveMenuPointer->Process(ReadBufferPointer);
                }

                if( ReadBufferLength < (READ_BUFFER_LENGTH -1) )
                {
                    ReadBufferLength++;
                    ReadBufferPointer++;
                }
                else
                {
                    // reset read buffer
                    ReadBufferPointer = &ReadBuffer[0];
                    ReadBufferLength = 0;
                    ResetCounter = 0;
                    SDebug_SendAcknowledgement(FALSE, SDErrorEnum_E1, 0);
                }
            }
        }
        else
        {
            if (ActiveMenuPointer && ActiveMenuPointer->Process)
            {
                // check if we need to update the screen
                if ( Tick_DelayMs_NonBlocking(FALSE, &ProcessUpdateRate) )
                {
                    // reset the tick counter
                    Tick_DelayMs_NonBlocking(TRUE, &ProcessUpdateRate);
                    ActiveMenuPointer->Process(NULL);
                }
            }
            else
            {
                Tick_DelayMs_NonBlocking(TRUE, &ProcessUpdateRate);
            }
        }
    }

    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \brief Initialize the serial interface
///
/// \param uartInterface pointer to the serial interface to use. if null is pass then
/// the serial port will not be open therefore it till be assumed that
/// it already open.
///////////////////////////////////////////////////////////////////////////////
void SDebug_Init(SerialInterface *uartInterface)
{
    if(uartInterface) // if the pointer is not null
    {
        COMPort = uartInterface; // update our copy of the serial port

        COMPort->Open(9600);//1500000);
    }

    // reset pointer and length
    ReadBufferPointer = &ReadBuffer[0];
    ReadBufferLength = 0;

    CLEAR_TERMINAL_SCREEN();
    SDbug_DrawHeader();
    // draw the system detail
    SDebug_WriteString( "COMPILED       "  COMPILED_DATA_TIME  "\n\r"
                        "FIRMWARE       "  FIRMWARE_VERSION  "\n\r"
                        "Press ESC to show current menu\n\r"
                        "Press enter three times to show the program information\n\r");
    SDebug_DrawPrompt();

    Tick_DelayMs_NonBlocking(TRUE, &ProcessUpdateRate);
}

#endif // EN_DEBUG_INTERFACE
