#include "Command.h"
#include <stdlib.h>

typedef struct{
	char szCommand[10];
	int CommandId;
	bool bParameter;
}COMMAND_INFO;

COMMAND_INFO gCommandList[] = {
		{"AT+MOI\n", Command::CMD_MOI, true},
		{"AT+TMP\n", Command::CMD_TMP, true},
		{"AT+HUM\n", Command::CMD_HUM, true},
		{"AT+LIT\n", Command::CMD_LIT, true},
		{"AT+CO2\n", Command::CMD_CO2, true},
		{"AT+CAM\n", Command::CMD_CAM, true},
		{"AT+LON\n", Command::CMD_LED_ON, false},
		{"AT+LOF\n", Command::CMD_LED_OFF, false},
		{"AT+WON\n", Command::CMD_WTR_ON, false},
		{"AT+WOF\n", Command::CMD_WTR_OFF, false},
		{"AT+FON\n", Command::CMD_FAN_ON, false},
		{"AT+FOF\n", Command::CMD_FAN_OFF, false},
};

Command::Command(){

}