/*
 * Reader.h
 *
 *  Created on: 10.11.2016
 *      Author: Tony
 */

#ifndef READERH
#define READERH

class Reader {
public:
    Reader();
    virtual ~Reader();
    void CharReader();
    void values();
    int value;
private:
    char userInput[80];
};


#endif / READERH */
