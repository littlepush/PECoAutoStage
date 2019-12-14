/*
    main.cpp
    2019-12-10
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

#include <peutils.h>
using namespace pe;

#include <cotask.h>
#include <conet.h>
using namespace pe::co;

#include "costage.h"
using namespace coas;

int g_return = 0;

void co_main( int argc, char* argv[] ) {
    bool _normal_return = false;
    utils::argparser::set_parser("help", "h", [&_normal_return](std::string&&) {
        _normal_return = true;
    });
    utils::argparser::set_parser("version", "v", [&_normal_return](std::string&&) {
        std::cout << "coas, ";
        #ifdef DEBUG
            std::cout << "Debug Version, ";
        #else
            std::cout << "Release Version, ";
        #endif
        std::cout << "v" << VERSION << std::endl;
        std::cout 
            << "Powered By Push Chen <littlepush@gmail.com>, as a sub project of PECo framework." 
            << std::endl;
        _normal_return = true;
    });

    // Do the parse
    if ( !utils::argparser::parse(argc, argv) ) {
        g_return = 1;
        return;
    }
    // On output info command
    if ( _normal_return ) return;
    coas::module_manager::init_default_modules();

    auto _input = utils::argparser::individual_args();
    if ( _input.size() == 0 ) {
        // Begin Input from STDIN
        coas::costage _stage;
        std::string _code;
        int _lno = 0;
        std::cout << "coas(" << _lno << "): ";
        while ( std::getline(std::cin, _code) ) {
            auto _flag = _stage.code_parser(std::move(_code));
            if ( _flag == coas::I_SYNTAX ) {
                std::cerr << "Syntax Error: " << _stage.err_string() << std::endl;
                g_return = 99;
                break;
            } else if ( _flag == coas::I_UNFINISHED ) {
                continue;
            }
            // Now we get a full code, run it
            auto _ret = _stage.code_run();
            if ( _ret == coas::E_ASSERT ) {
                std::cerr << "Assert Failed: " << _stage.err_string() << std::endl;
                g_return = 98;
                break;
            } 
            _stage.code_clear();
            // Run OK, then continue to get next code
            ++_lno;
            std::cout << "coas(" << _lno << "): ";
        }
    } else {
        // We get a file or folder as input
        std::set< std::string > _stages;
        for ( auto& _arg : _input ) {
            if ( utils::is_file_existed(_arg) ) {
                _stages.insert(_arg);
            } else {
                utils::rek_scan_dir(_arg, [&_stages](const std::string& name, bool is_folder) {
                    if ( !is_folder ) _stages.insert(name);
                    return true;
                });
            }
        }
        for ( auto stage_file : _stages ) {
            loop::main.do_job([stage_file]() {
                std::ifstream _stagef(stage_file);
                if ( !_stagef ) return;
                std::string _code;
                coas::costage _stage;
                size_t _line = 1;
                while ( std::getline(_stagef, _code) ) {
                    auto _flag = _stage.code_parser( std::move(_code) );
                    if ( _flag == coas::I_SYNTAX ) {
                        std::cerr << stage_file << ":" << _line << ": syntax error: " 
                            << _stage.err_string() << std::endl;
                        return;
                    }
                    this_task::yield();
                    ++_line;
                }
                auto _ret = _stage.code_run();
                if ( _ret == coas::E_ASSERT ) {
                    std::cerr << stage_file << ": assert failed: " << _stage.err_string() << std::endl;
                }
            });
        }
    }
}

int main( int argc, char* argv[] ) {
    loop::main.do_job(std::bind(&co_main, argc, argv));
    loop::main.run();
    return g_return;
}