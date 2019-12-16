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

#include <float.h>
#include <dlfcn.h>

std::list< void * > g_modules;
int g_return = 0;

std::string __time_format(double t) {
    ostringstream _os;
    _os << std::setprecision(3);
    if ( t <= 1000.f ) {
        _os << t << "ms";
    } else if ( t <= 1000000.f ) {
        _os << (t / 1000) << "s";
    } else if ( t <= 60000000.f ) {
        double _seconds = t / 1000000.f;
        _os << (_seconds / 60.f) << "m" << ((size_t)_seconds % 60u) << "s";
    }
    return _os.str();
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
    std::string _code;
    while ( std::getline(fstage, _code) ) {
        utils::trim(_code);
        utils::code_filter_comment(_code);
        ++lineno;
        if ( _code.size() == 0 ) continue;
        if ( utils::is_string_start(_code, ".func") ) {
            if ( _already_in_stage ) {
                std::cerr << stage_file << ": " << lineno <<
                    ": func cannot be in the middle of stage" <<
                    std::endl;
                return false;
            }
            auto _p = utils::split(_code, std::vector<std::string>{" ", "\t"});
            if ( _p.size() != 2 && _p[0] != ".func" ) {
                std::cerr << stage_file << ": " << lineno <<
                    ": assert failed: invalidate function definition" << 
                    std::endl;
                return false;
            }
            // End last function
            stage.end_function();
            // And start a new one
            stage.create_function(_p[1]);
            _is_in_function = true;
            continue;
        } else if ( utils::is_string_start(_code, ".include") ) {
            if ( _already_in_stage ) {
                std::cerr << stage_file << ": " << lineno <<
                    ": include cannot be in the middle of stage" <<
                    std::endl;
                return false;
            }
            auto _p = utils::split(_code, std::vector<std::string>{" ", "\t"});
            if ( _p.size() != 2 && _p[0] != ".include" ) {
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
            if ( _already_in_stage ) {
                std::cerr << stage_file << ": " << lineno <<
                    ": You can only have one stage in a single stage file" <<
                    std::endl;
                return false;
            }
            stage.end_function();
            _is_in_function = false;
            continue;
        }

        if ( module_file && (!_is_in_function) ) {
            std::cerr << stage_file << ": " << lineno << 
                ": code out of function in a module file" <<
                std::endl;
            return false;
        }
        if ( !_is_in_function ) _already_in_stage = true;
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


bool run_stage_file( const std::string& stage_file, costage& stage ) {
    std::ifstream _stagef(stage_file);
    if ( !_stagef ) return false;
    size_t _line = 0;
    if ( !parse_stage_file(stage_file, _stagef, stage, _line, false) ) return false;
    auto _ret = stage.code_run();
    ON_DEBUG(
        std::cout << stage_file << ", result: " << std::endl << stage.rootValue().toStyledString();
    )
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

void co_main( int argc, char* argv[] ) {
    bool _normal_return = false;
    std::string _module_file;
    utils::argparser::set_parser("module_file", "m", _module_file);
    utils::argparser::set_parser("help", "h", [&_normal_return](std::string&&) {
        std::cout
            << "Usage: coas [OPTION] <stage_folder>" << std::endl
            << "Usage: coas [OPTION] <stage_file>" << std::endl
            << "Usage: coas [OPTION]" << std::endl
            << "coas is a test case running tool. each case should be writen in" << std::endl
            << "a stage file. The basic format in a stage file is `left` = `right`" << std::endl
            << "use `$.assert = ...` or `$.return = bool` to tell if a stage success." << std::endl
            << std::endl
            << "  -m, --module_file         Load thirdparty syntax module" << std::endl
            << "  -h, --help                Display this message" << std::endl
            << "  -v, --version             Display version number" << std::endl
            << "  --enable-conet-trace      In debug version only, display net log" << std::endl
            << "  --enable-cotask-trace     In debug version only, display task log" << std::endl
            << std::endl
            << "Visit https://github.com/littlepush/PECoAutoStage to see detail documentation." 
            << std::endl
            << "Powered By Push Chen <littlepush@gmail.com>, as a sub project of PECo framework." 
            << std::endl;
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

    if ( _module_file.size() > 0 ) {
        if ( utils::is_file_existed(_module_file) ) {
            // Load Modules
            std::string _ml;
            std::ifstream _mf(_module_file);
            std::set< std::string > _mn;
            while ( std::getline(_mf, _ml) ) {
                utils::trim(_ml);
                utils::code_filter_comment(_ml);
                if ( _ml.size() == 0 ) continue;
                if ( _ml[0] == '#' ) continue;
                _mn.insert(_ml);
            }
            _mf.close();

            for ( auto& m_name : _mn ) {
                if ( !utils::is_file_existed(m_name) ) continue;
                void *_hm = dlopen(m_name.c_str(), RTLD_LAZY | RTLD_GLOBAL);
                if ( !_hm ) {
                    std::cerr << "WARNING: cannot load module " << m_name << std::endl;
                    continue;
                }
                module_nf _nf = (module_nf)dlsym(_hm, "module_name");
                if ( !_nf ) {
                    std::cerr << "WARNING: " << dlerror() << " in " << m_name << std::endl;
                    dlclose(_hm);
                    continue;
                }
                module_f _jf = (module_f)dlsym(_hm, "module_action");
                if ( !_jf ) {
                    std::cerr << "WARNING: " << dlerror() << " in " << m_name << std::endl;
                    dlclose(_hm);
                    continue;
                }
                module_mf _mf = (module_mf)dlsym(_hm, "module_match");
                if ( !_mf ) {
                    std::cerr << "WARNING: " << dlerror() << " in " << m_name << std::endl;
                    dlclose(_hm);
                    continue;
                }
                // Register the module
                module_manager::register_module(module_type{
                    _nf(), _mf, _jf
                });

                g_modules.push_back(_hm);
            }

        }
    }

    auto _input = utils::argparser::individual_args();
    if ( _input.size() == 0 ) {
        // Begin Input from STDIN
        coas::costage _stage;
        std::string _code;
        int _lno = 0;
        std::cout << "coas(" << _lno << "): ";
        while ( std::getline(std::cin, _code) ) {
            if ( _code == ".exit" ) break;
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
        std::string _begin_stage;
        std::string _final_stage;
        for ( auto& _arg : _input ) {
            if ( utils::is_file_existed(_arg) ) {
                _stages.insert(_arg);
            } else {
                utils::rek_scan_dir(
                    _arg, 
                    [&_stages, &_begin_stage, &_final_stage](const std::string& name, bool is_folder) {
                        if ( is_folder ) return true;
                        // if ( !is_folder ) _stages.insert(name);
                        if ( utils::extension(name) != "costage" ) return true;
                        if ( utils::filename(name) == "_begin" ) {
                            _begin_stage = name;
                            return true;
                        }
                        if ( utils::filename(name) == "_final" ) {
                            _final_stage = name;
                            return true;
                        }
                        _stages.insert(name);
                        return true;
                    }
                );
            }
        }
        this_task::begin_tick();
        Json::Value _root(Json::nullValue);
        // We do have a begin stage
        if ( _begin_stage.size() > 0 ) {
            costage _bstage;
            bool _ret = run_stage_file(_begin_stage, _bstage);
            if ( !_ret ) {
                g_return = 101; return;
            }
            // Copy the root value
            _root = _bstage.rootValue();
        }
        Json::Value _result(Json::objectValue);
        std::shared_ptr< pe::co::condition > _sig(new pe::co::condition);
        for ( auto stage_file : _stages ) {
            loop::main.do_job([_sig, stage_file, &_root, &_result]() {
                this_task::begin_tick();
                costage _stage(_root);
                bool _ret = run_stage_file(stage_file, _stage);
                Json::Value _rj(Json::objectValue);
                _rj["state"] = _ret;
                _rj["time"] = this_task::tick();
                _result[stage_file] = _rj;
                _sig->notify();
            });
        }
        // Wait all stage done
        while ( _result.size() != _stages.size() ) {
            _sig->wait(nullptr);
        }
        int _passed = 0, _failed = 0;
        double _time = 0;
        double _max_time = 0;
        double _min_time = FLT_MAX;
        std::string _max_time_stage;
        std::string _min_time_stage;
        for ( auto i = _result.begin(); i != _result.end(); ++i ) {
            bool _state = (*i)["state"].asBool();
            if ( _state == true ) {
                std::cout << "[" << CLI_GREEN << " PASS " << CLI_NOR << "]: ";
                _passed += 1;
            } else {                
                std::cout << "[" << CLI_RED << "FAILED" << CLI_NOR << "]: ";
                _failed += 1;
            }
            double _stime = (*i)["time"].asDouble();
            _time += _stime;
            if ( _stime > _max_time ) {
                _max_time = _stime;
                _max_time_stage = i.key().asString();
            }
            if ( _stime < _min_time ) {
                _min_time = _stime;
                _min_time_stage = i.key().asString();
            }
            std::cout << i.key().asString() << ", " << __time_format(_stime) << std::endl;
        }
        std::cout << 
            "All Stage: " << _stages.size() << std::endl <<
            "Passed: " << _passed << std::endl <<
            "Failed: " << _failed << std::endl <<
            "Available: " << std::setprecision(3) << 
                ((double)_passed / (double)_stages.size()) * 100.f << "%" << 
                std::endl <<
            "Running time: " << __time_format(this_task::tick()) << std::endl <<
            "All stage time: " << __time_format(_time) << std::endl <<
            "Max stage time: [" << _max_time_stage << "]: " << 
                std::setprecision(3) << __time_format(_max_time) << std::endl <<
            "Min stage time: [" << _min_time_stage << "]: " << 
                std::setprecision(3) << __time_format(_min_time) << std::endl;
        if ( _final_stage.size() > 0 ) {
            if ( _root.isNull() ) {
                _root = Json::Value(Json::objectValue);
            }
            _root["result"] = _result;
            costage _fstage(_root);
            bool _ret = run_stage_file(_final_stage, _fstage);
            if ( !_ret ) {
                g_return = 102; return;
            }
        }
    }
}

int main( int argc, char* argv[] ) {
    loop::main.do_job(std::bind(&co_main, argc, argv));
    loop::main.run();
    for ( void *_m : g_modules ) {
        dlclose(_m);
    }
    return g_return;
}