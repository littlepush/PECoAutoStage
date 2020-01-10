/*
    rpn.h
    2019-12-12
    PECoAutoStage
    Push Chen
*/

/*
MIT License

Copyright (c) 2019 Push Chen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#ifndef PECO_AUTO_STAGE_RPN_H__
#define PECO_AUTO_STAGE_RPN_H__

#include <peco/peco.h>
using namespace pe;

namespace coas { namespace rpn {

    enum item_type {
        IT_NUMBER                   = 0,
        IT_STRING,
        IT_OBJECT,
        IT_ARRAY,
        IT_BOOL,
        IT_PATH,
        IT_NULL,
        IT_STACK,                   // Sub stack
        IT_BOA,                     // Begin of Arguments
        IT_EOA,                     // End of Arguments

        // Operators
        IT_SET                      = 100,
        IT_PLUS,
        IT_MINUS,
        IT_TIMES,
        IT_DIVID,
        IT_MOD,
        IT_POW,

        IT_EQUAL,
        IT_NOT_EQUAL,
        IT_LESS_THAN,
        IT_LESS_EQUAL,
        IT_GREAT_THAN,
        IT_GREAT_EQUAL,
        IT_NOT,

        IT_EXEC,
        IT_NODE,

        // Sub
        IT_LEFT_BRACES,             // {
        IT_RIGHT_BRACES,            // }
        IT_LEFT_PARENTHESES,        // (
        IT_RIGHT_PARENTHESES,       // )
        IT_LEFT_BRACKETS,           // [
        IT_RIGHT_BRACKETS,          // ]

        IT_COMMA,

        // Types
        IT_VOID                     = 10000,
        IT_ERROR,
        IT_TEMP
    };

    // Each word in the code will be convert to an item_t
    typedef struct {
        item_type               type;
        Json::Value             value;
    } item_t;

    // The Memory Stack
    typedef std::stack< item_t >        stack_type;

    // Stack used in parser
    typedef struct {
        // Operator 
        stack_type                      op;
        // Item
        stack_type                      item;
        // Node Level
        std::map< int, bool >           nodelv;
        // Argument Level
        std::map< int, bool >           arglv;
        // Parentheses Level
        int                             plv;
    } parser_stack_type;
}}

#endif 

// Push Chen
//