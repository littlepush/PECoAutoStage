/*
    costage.h
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

#include <peutils.h>
using namespace pe;

#include "rpn.h"

namespace coas {

    // Input State
    typedef enum {
        // Input is ok
        I_OK,
        // Input unfinished
        I_UNFINISHED,
        // Input has syntax error
        I_SYNTAX
    } I_STATE;

    // Executing State
    typedef enum {
        // Execute OK
        E_OK,
        // Assert Failed
        E_ASSERT,
        // Break and return
        E_RETURN
    } E_STATE;

    // Class: Stage 
    // a stage will process a costage file and read each line and execute it.
    class costage {
    public: 
        // Types
        typedef std::shared_ptr< rpn::stack_type >              ptr_stack_type;
        typedef std::shared_ptr< rpn::runtime_stack_type >      ptr_runtime_type;
        typedef std::shared_ptr< rpn::parser_stack_type >       ptr_parser_type;
        typedef rpn::stack_type *                               pstack_t;
    protected: 

        // Environments
        // A stage's root node
        Json::Value                         root_;
        // The error message
        std::string                         err_;
        // The final result of the stage
        rpn::item_t                         result_;

        // Code Cache for unfinished line
        std::string                         code_;

        // Code Map, contains all sub stacks
        std::map< std::string, ptr_stack_type >         code_map_;
        // Parser Queue
        std::queue< ptr_parser_type >                   parser_queue_;
        // Runtime Queue
        std::queue< ptr_runtime_type >                  runtime_queue_;

        // Current Stack
        ptr_runtime_type                    exec_;
        ptr_parser_type                     parser_;

    protected: 
        // Internal navigation

        // Create a new sub stack and return the name
        // Will create both parser item and code part
        std::string stack_create_();

        // Jump to a stack
        // Switch current runtime envorinment
        void stack_jump_(const std::string& name);

        // Exit a stack
        // Return from a current stack and switch to last stack
        // the return value of current stack will be push to last stack's data
        void stack_return_( );

        // Get a node by path
        // If the path is not in module list, then create a new nullObject
        // If match any module, then run the module and put the result as the node.
        Json::Value* node_by_path_(const Json::Value& path_value);
    };

}

#endif 

// Push Chen
//