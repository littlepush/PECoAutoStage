/*
    costage-runner.cpp
    2019-12-16
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

#include "costage-runner.h"

namespace coas {
    // Fetch only the header part of a stage file
    bool fetch_stage_info(const std::string& stage_file, stage_info_t& info) {
        std::ifstream _fs(stage_file);
        if ( !_fs ) return false;

        std::string _description;
        std::string _code;
        bool _is_in_description = false;
        bool _is_in_function = false;

        while ( std::getline(_fs, _code) ) {
            utils::trim(_code);
            utils::code_filter_comment(_code);
            if ( _code.size() == 0 ) continue;
            if ( _code[0] == '.' && _is_in_description ) {
                // End Description
                info.description = _description;
                _is_in_description = false;
            }
            if ( _code == ".description" ) {
                if ( _is_in_description ) return false;
                _is_in_function = false;
                _is_in_description = true;
                continue;
            }
            if ( _code == ".stage" ) return true;
            if ( _code[0] == '.' ) {
                _is_in_function = false;
                auto _kv = utils::split(_code, " \t");
                if ( _kv.size() == 1 ) return false;
                if ( _kv[0] == ".tag" ) info.tags.insert(_kv[1]);
                else if ( _kv[0] == ".name" ) {
                    if ( info.name.size() != 0 ) return false;
                    info.name = _kv[2];
                }
                else if ( _kv[0] == ".func" ) {
                    _is_in_function = true;
                }
            } else {
                if ( _is_in_description ) {
                    _description += " " + _code;
                } else {
                    if ( !_is_in_function ) return true;
                }
            }
        }

        return false;
    }

    bool parse_stage_file( 
        const std::string& stage_file,
        std::ifstream& fstage,
        costage& stage,
        size_t& lineno,
        bool module_file
    ) {
        if ( !fstage ) return false;
        bool _is_in_function = false;
        bool _already_in_stage = false;
        bool _is_description = false;

        std::string _description;
        std::string _code;
        while ( std::getline(fstage, _code) ) {
            utils::trim(_code);
            utils::code_filter_comment(_code);
            ++lineno;
            if ( _code.size() == 0 ) continue;
            if ( _code[0] == '.' && _already_in_stage ) {
                std::cerr << stage_file << ": " << lineno <<
                    ": section cannot be in the middle of stage" <<
                    std::endl;
                return false;
            }
            if ( utils::is_string_start(_code, ".description") ) {
                if ( _code != ".description" ) {
                    std::cerr << stage_file << ": " << lineno <<
                        ": description should start from a new line" << 
                        std::endl;
                    return false;
                }
                if ( _description.size() > 0 ) {
                    std::cerr << stage_file << ": " << lineno <<
                        ": a stage file can only have one description" << 
                        std::endl;
                    return false;
                }
                _is_description = true;
                continue;
            }
            if ( _code[0] == '.' && _is_description ) {
                _is_description = false;
                stage.set_description(utils::trim(_description));
            }
            if ( _is_description ) {
                _description += " " + _code;
                continue;
            }
            if ( utils::is_string_start(_code, ".tag") ) {
                auto _p = utils::split(_code, std::vector<std::string>{" ", "\t"});
                if ( _p.size() != 2 || _p[0] != ".tag" ) {
                    std::cerr << stage_file << ": " << lineno << 
                        ": invalidate tag definition" << std::endl;
                    return false;
                }
                // Add a new tag
                stage.add_tag(_p[1]);
                continue;
            }
            if ( utils::is_string_start(_code, ".name") ) {
                auto _p = utils::split(_code, std::vector<std::string>{" ", "\t"});
                if ( _p.size() != 2 || _p[0] != ".name" ) {
                    std::cerr << stage_file << ": " << lineno << 
                        ": invalidate name definition" << std::endl;
                    return false;
                }
                if ( stage.name().size() > 0 ) {
                    std::cerr << stage_file << ": " << lineno <<
                        ": a stage file can only have one name" << 
                        std::endl;
                    return false;
                }
                stage.set_name(_p[1]);
                continue;
            }
            if ( utils::is_string_start(_code, ".func") ) {
                auto _p = utils::split(_code, std::vector<std::string>{" ", "\t"});
                if ( _p.size() != 2 || _p[0] != ".func" ) {
                    std::cerr << stage_file << ": " << lineno <<
                        ": invalidate function definition" << std::endl;
                    return false;
                }
                // End last function
                stage.end_function();
                // And start a new one
                stage.create_function(_p[1]);
                _is_in_function = true;
                continue;
            } else if ( utils::is_string_start(_code, ".include") ) {
                auto _p = utils::split(_code, std::vector<std::string>{" ", "\t"});
                if ( _p.size() != 2 || _p[0] != ".include" ) {
                    std::cerr << stage_file << ": " << lineno <<
                        ": assert failed: invalidate include" << 
                        std::endl;
                    return false;
                }
                stage.end_function();
                std::string _fs = utils::dirname(stage_file) + _p[1];
                std::ifstream _ifs(_fs);
                if ( !_ifs ) {
                    std::cerr << stage_file << ": " << lineno <<
                        ": include file " << _fs << " not found" <<
                        std::endl;
                    return false;
                }
                size_t _lines = 0;
                if ( !parse_stage_file(_fs, _ifs, stage, _lines, true) ) return false;
                continue;
            } else if ( _code == ".stage" ) {
                stage.end_function();
                _already_in_stage = true;
                _is_in_function = false;
                continue;
            }

            if ( module_file && (!_is_in_function) ) {
                std::cerr << stage_file << ": " << lineno << 
                    ": code out of function in a module file" <<
                    std::endl;
                return false;
            }
            auto _flag = stage.code_parser( std::move(_code) );
            if ( _flag == coas::I_SYNTAX ) {
                std::cerr << stage_file << ":" << lineno << ": syntax error: " 
                    << stage.err_string() << std::endl;
                return false;
            }
            this_task::yield();
        }
        return true;
    }

    // Simply to parse a file
    bool parse_stage_file(
        const std::string& stage_file,
        costage& stage
    ) {
        std::ifstream _fs(stage_file);
        size_t _lineno = 0;
        return parse_stage_file(stage_file, _fs, stage, _lineno, false);
    }

    bool run_stage_file( const std::string& stage_file, costage& stage ) {
        auto _ret = stage.code_run();
        if ( _ret == coas::E_ASSERT ) {
            std::cerr << stage_file << ": assert failed: " << stage.err_string() << std::endl;
            return false;
        }
        if ( _ret == coas::E_RETURN ) {
            if ( stage.resultItem().type == rpn::IT_PATH ) {
                Json::Value* _pret = stage.get_node(stage.returnValue());
                if ( _pret == NULL ) {
                    std::cerr << stage_file << ": the return path not existed" << std::endl;
                    return false;
                }
                if ( _pret->isNull() ) {
                    std::cerr << stage_file << ": WARNING: " <<
                        "return a null object, should use $.void instead" << 
                        std::endl;
                    return true;
                }
                if ( _pret->isBool() ) {
                    return _pret->asBool();
                }
            } else if ( stage.resultItem().type == rpn::IT_VOID ) {
                std::cerr << stage_file << ": WARNING: " <<
                    "return a void object, should use $.void instead" << 
                    std::endl;
                return true;
            } else if ( stage.resultItem().type == rpn::IT_BOOL ) {
                return stage.returnValue().asBool();
            } else {
                std::cerr << stage_file << ": a stage can only return void or bool" << std::endl;
                return false;
            }
        }
        return true;
    }
}

// Push Chen
//
