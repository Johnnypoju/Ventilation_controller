/*
===============================================================================
 Name        : main.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#if defined (__USE_LPCOPEN)
#if defined(NO_BOARD_LIB)
#include "chip.h"
#else
#include "board.h"
#endif
#endif

#include <cr_section_macros.h>
#include "ModbusMaster.h"
#include "I2C.h"
#include "UI.h"

// TODO: insert other include files here

// TODO: insert other definitions and declarations here

#define TICKRATE_HZ1 (1000)

static volatile int counter;
static volatile int counter2;
static volatile int counter3 = 30000;
static volatile uint32_t systicks;

volatile int compPress;
volatile int autoPress = 5;
volatile int lowestP;
volatile int highestP;
volatile int change;
volatile int freQlvl = 2000;

int c;
int h;
bool autoManual = true;
bool itIsDone = true;
bool weCantMakeIt = false;

char buffer2[150];
uint8_t pressureData[3];
uint8_t readPressureCmd = 0xF1;
int16_t pressure = 0;
ModbusMaster *nodeptr;

bool setFrequency(ModbusMaster& node, uint16_t freq) {
	uint8_t result;
	int ctr;
	bool atSetpoint;
	const int delay = 500;

	node.writeSingleRegister(1, freq); // set motor frequency

	printf("Set freq = %d\n", freq/40); // for debugging

	// wait until we reach set point or timeout occurs
	ctr = 0;
	atSetpoint = false;
	do {
		// read status word
		result = node.readHoldingRegisters(3, 1);
		// check if we are at setpoint
		if (result == node.ku8MBSuccess) {
			if(node.getResponseBuffer(0) & 0x0100) atSetpoint = true;
		}
		ctr++;
	} while(ctr < 20 && !atSetpoint);

	printf("Elapsed: %d\n", ctr * delay); // for debugging

	return atSetpoint;
}

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief	Handle interrupt from SysTick timer
 * @return	Nothing
 */
void SysTick_Handler(void)
{
	//Check and read for user input

	h = Board_UARTGetChar();
	if (h != EOF){
		c = h;
	}

	systicks++;

	//Adjust the frequency and remind that the component is still alive

	if (counter2 >= 1000 && nodeptr != NULL) {
		setFrequency(*nodeptr, freQlvl);
		counter2 = 0;
	}

	//Counter 3 checks if the desired values can be reached in a reasonable time

	if (counter3 > 0) {
		counter3--;
	}
	if (counter3 == 0 && itIsDone == false) {
		weCantMakeIt = true;
	}
	counter2++;
	if(counter > 0) counter--;
}
#ifdef __cplusplus
}
#endif

void Sleep(int ms)
{
	counter = ms;
	while(counter > 0) {
		__WFI();
	}
}

uint32_t millis() {
	return systicks;
}

void printRegister(ModbusMaster& node, uint16_t reg) {
	uint8_t result;
	// slave: read 16-bit registers starting at reg to RX buffer
	result = node.readHoldingRegisters(reg, 1);

	// do something with data if read is successful
	if (result == node.ku8MBSuccess)
	{
		printf("R%d=%04X\n", reg, node.getResponseBuffer(0));
	}
	else {
		printf("R%d=???\n", reg);
	}
}

int main(void) {

#if defined (__USE_LPCOPEN)
    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();
#if !defined(NO_BOARD_LIB)
    // Set up and initialize all required blocks and
    // functions related to the board hardware
    Board_Init();
    // Set the LED to the state of "On"
    Board_LED_Set(0, true);
#endif
#endif

    // TODO: insert code here

    uint32_t sysTickRate;
    SystemCoreClockUpdate();
	Chip_Clock_SetSysTickClockDiv(1);
	sysTickRate = Chip_Clock_GetSysTickClockRate();
	SysTick_Config(sysTickRate / TICKRATE_HZ1);

	UI ui;

    I2C i2c(0, 100000);
    ModbusMaster node(2); // Create modbus object that connects to slave id 2

    nodeptr = &node;

    /* Set up SWO to PIO1_2 */
    Chip_SWM_MovablePortPinAssign(SWM_SWO_O, 1, 2); // Needed for SWO printf

	node.begin(9600); // set transmission rate - other parameters are set inside the object and can't be changed here

	node.writeSingleRegister(0, 0x047F); // set drive to start mode

	Sleep(1000); // give converter some time to set up
	// note: we should have a startup state machine that check converter status and acts per current status
	//       but we take the easy way out and just wait a while and hope that everything goes well

	while (1) {

		Sleep(1000);
		//c = Board_UARTGetChar();

		// Check if the program is running in Manual or Auto and act according to it

		if (autoManual) {
			if (i2c.transaction(0x40, &readPressureCmd, 1, pressureData, 3)) {
				/* Output temperature. */
				pressure = (pressureData[0] << 8) | pressureData[1];
				snprintf(buffer2, 150,
						"Pressure read over I2C is %.1f Pa Curr FreQ: %d\r\n",
						pressure / 240.0, freQlvl);

			} else {
				snprintf(buffer2, 150, "Error reading pressure.\r\n");
			}
			Sleep(1000);

			compPress = pressure / 240.0;
			lowestP = autoPress - 0.5;
			highestP = autoPress + 0.5;

			if (((autoPress - compPress) > 3)
					|| ((compPress - autoPress) > 3)) {
				change = 1000;
			} else if (((autoPress - compPress) > 1)
					|| ((compPress - autoPress) > 1)) {
				change = 200;
			} else {
				change = 100;
			}
			if (compPress < lowestP) {
				freQlvl += change;
				itIsDone = false;
			} else if (compPress > highestP) {
				freQlvl -= change;
				itIsDone = false;
			} else {
				freQlvl = freQlvl;
				itIsDone = true;
			}

			if (freQlvl > 20000) {
				freQlvl = 20000;
			}
			ui.print(buffer2);

			if (weCantMakeIt) {
				snprintf(buffer2, 150, "Still calibrating.\r\n");
				ui.print(buffer2);
				weCantMakeIt = false;
				counter3 = 30000;
			}

			//c = Board_UARTGetChar();
			if (c == '\r') {
				autoManual = false;
			}
		} else {
			if (i2c.transaction(0x40, &readPressureCmd, 1, pressureData, 3)) {
				/* Output temperature. */
				pressure = (pressureData[0] << 8) | pressureData[1];
				snprintf(buffer2, 150,
						"Pressure read over I2C is %.1f Pa Curr FreQ: %d\r\n",
						pressure / 240.0, freQlvl);

			} else {
				snprintf(buffer2, 150, "Error reading pressure.\r\n");
			}
			Sleep(1000);
			//c = Board_UARTGetChar();
			if (c == '\r') {
				int y = ui.userInput();
				if (y == 1) {
					freQlvl = ui.fanSpeed;
					Sleep(1000);
				}
				else if (y == 2) {
					autoPress = ui.pasCal;
					autoManual = true;
					Sleep(1000);
					counter3 = 30000;
				}
				else if (y == 0) {
					autoManual = false;
				}
				else {
					autoManual = true;
				}
				c = -1;
			} else {
				ui.print(buffer2);
			}
		}
	}
	// Force the counter to be placed into memory
	volatile static int i = 0;
	// Enter an infinite loop, just incrementing a counter
	while (1) {
		i++;
	}
	return 0;
}
