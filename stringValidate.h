#ifndef STRINGVALIDATE_H_
#define STRINGVALIDATE_H_

#include <iostream>
#include <string.h>
#include <sstream>

bool isNumber(char character) {
    if (character >= 48 && character <= 57) {
        return true;
    } else {
        return false;
    }
}

bool isSpace(char character) {
    if (character == 32) {
        return true;
    } else {
        return false;
    }
}

__int128_t atoint128_t(std::string const & in)
{
    __int128_t res = 0;
    size_t i = 0;
    bool sign = false;
    
    if (in[i] == '-') {
        ++i;
        sign = true;
    }
    
    if (in[i] == '+') {
        ++i;
    }
    
    for (; i < in.size(); ++i) {
        const char c = in[i];
        if (not std::isdigit(c))
            throw std::runtime_error(std::string("Non-numeric character: ") + c);
            res *= 10;
        res += c - '0';
    }
    
    if (sign) {
        res *= -1;
    }
    
    return res;
}

std::ostream&
operator<<( std::ostream& dest, __int128_t value )
{
    std::ostream::sentry s( dest );
    if ( s ) {
        __uint128_t tmp = value < 0 ? -value : value;
        char buffer[ 128 ];
        char* d = std::end( buffer );
        do
        {
            -- d;
            *d = "0123456789"[ tmp % 10 ];
            tmp /= 10;
        } while ( tmp != 0 );
        if ( value < 0 ) {
            -- d;
            *d = '-';
        }
        int len = std::end( buffer ) - d;
        if ( dest.rdbuf()->sputn( d, len ) != len ) {
            dest.setstate( std::ios_base::badbit );
        }
    }
    return dest;
}

char* changeNumberInString(char* content, char* newNumber) {
    int size = strlen(content);
    char previous = '0';
    int digitsInRow = 0;
    std::string numberFirstTwoDigits;
    std::string numberWithoutFirstTwoDigits;
    std::string numberWithSpaces;
    std::string numberWithoutSpaces;
    bool isFound = false;
    int numberOfSpaces = 0;
    int startOfNumber;
    
    char currentChar = '0';
    char previousChar = '0';
    int afterHeader = 0;
    
    for(int i=0; i<size; ++i) {
        currentChar = content[i];
        if (currentChar == '\n' && previousChar=='\n') {
            afterHeader = i;
            break;
        }
        previousChar = content[i];
    }
    
    for(int i=afterHeader; i<size; ++i) {
        if (isNumber(content[i]) && digitsInRow==0) {
            digitsInRow++;
        } else if (isNumber(content[i]) && (isNumber(previous) == true || isSpace(previous)) && digitsInRow!=0) {
            digitsInRow++;
        } else if(!isNumber(content[i]) && !isSpace(content[i])){
            digitsInRow = 0;
        } else if (isSpace(content[i]) && digitsInRow != 0) {
            numberOfSpaces++;
        } else if (isSpace(content[i]) && digitsInRow == 0) {
            numberOfSpaces = 0;
        }
        previous = content[i];
        if (digitsInRow == 26){
            startOfNumber = i-(25+numberOfSpaces);
            for (int j=startOfNumber; j<=i; j++) {
                numberWithSpaces += content[j];
            }
            isFound = true;
            break;
        }
    }
    
    //std::cout << "\n-received message-\n" << content << "\n--\n";
    
    for(int k=0; k<numberWithSpaces.length(); ++k) {
        if (numberWithSpaces[k] >= 48 && numberWithSpaces[k] <= 57) {
            numberWithoutSpaces += numberWithSpaces[k];
        }
    }
    
    numberFirstTwoDigits += numberWithoutSpaces[0]; numberFirstTwoDigits += numberWithoutSpaces[1];
    
    for(int k=2; k<numberWithoutSpaces.length(); ++k) {
        numberWithoutFirstTwoDigits += numberWithoutSpaces[k];
    }
    
    __int128_t numberWithoutFirstTwoDigitsInt = atoint128_t(numberWithoutFirstTwoDigits.c_str());
    __int128_t numberFirstTwoDigitsInt = atoint128_t(numberFirstTwoDigits.c_str());
    
    __int128 validateNumber = numberWithoutFirstTwoDigitsInt;
    validateNumber *= 1000000;
    validateNumber += 252100;
    validateNumber += numberFirstTwoDigitsInt;
    validateNumber = validateNumber % 97;
    
    // jeśli reszta jest równa 1 podmianiamy ciąg znaków na nowy, zapisany przez nas
    //std::cout << "\n-send message-\n";
    if (validateNumber == 1) {
        int it = 0;
        for (int i=startOfNumber; i<startOfNumber+numberOfSpaces+26; ++i) {
            if (content[i] >= 48 && content[i] <= 57) {
                content[i] = newNumber[it];
                it++;
            }
        }
    }
    
    //std::cout << content << "\n-";
    return content;
}

#endif