/*
 * UI.h
 *
 *  Created on: 9.1.2017
 *      Author: Sami
 */

#ifndef UI_H_
#define UI_H_

#include "Reader.h"
#include "board.h"

class UI {
public:
	UI();
	virtual ~UI();

	int userInput();
	void print(char buffer[150]);

	int fanSpeed, pasCal;
private:
	char buffer[150];
	int k;
	Reader reader;
};

#endif /* UI_H_ */
