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

#include "costage-runner.h"

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

void co_main( int argc, char* argv[] ) {
    bool _normal_return = false;
    std::string _module_file;
    std::set< std::vector< std::string > > _taggroup;
    std::string _root;
    std::string _filter;
    std::string _report_template;
    std::string _report_path;
    std::string _begin_stage;
    std::string _final_stage;
    std::string _report_stage;

    utils::argparser::set_parser("module_file", "m", _module_file);
    utils::argparser::set_parser("tag", "t", [&_taggroup](std::string&& value) {
        auto _r = utils::split(value, ",");
        _taggroup.insert(std::move(_r));
    });
    utils::argparser::set_parser("root", "r", _root);
    utils::argparser::set_parser("filter", "f", _filter);
    utils::argparser::set_parser("report-path", "rp", _report_path);
    utils::argparser::set_parser("report-template", "rt", _report_template);
    utils::argparser::set_parser("begin-stage", "bs", _begin_stage);
    utils::argparser::set_parser("final-stage", "fs", _final_stage);
    utils::argparser::set_parser("report-stage", "rs", _report_stage);

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
            << "  -t, --tag                 Run stages match the given tag" << std::endl
            << "  -r, --root                Initialize the root node according to the file" << std::endl
            << "  -f, --filter              Filte the stages according to the JSON filter" << std::endl
            << "  -rp, --report-path        Report output folder, will copy the " << std::endl 
            << "                              report.json to that path, and " << std::endl 
            << "                              create a report.html" << std::endl
            << "  -rt, --report-template    Customized report HTML template" << std::endl
            << "  -bs, --begin-stage        Override the _begin.costage file" << std::endl
            << "  -fs, --final-stage        Override the _final.costage file" << std::endl
            << "  -rs, --report-stage       Override the _report.costage file" << std::endl
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

    Json::Value _rootValue = (Json::objectValue);
    if ( _root.size() > 0 && utils::is_file_existed(_root) ) {
        Json::Reader _jr;
        std::ifstream _rf(_root);
        if ( !_jr.parse(_rf, _rootValue, false) ) {
            std::cerr << "failed to load root node" << std::endl;
            g_return = 2;
            return;
        }
    }

    auto _input = utils::argparser::individual_args();
    if ( _input.size() == 0 ) {
        // Begin Input from STDIN
        if ( _begin_stage.size() > 0 && utils::is_file_existed(_begin_stage) ) {
            coas::costage _bs(_rootValue);
            auto _r = coas::run_stage_file(_begin_stage, _bs);
            if ( !_r ) {
                std::cerr << _bs.err_string() << std::endl;
                g_return = 3;
                return;
            }
            // Copy the root value
            _rootValue = _bs.rootValue();
        }

        coas::costage _stage(_rootValue);
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

        // Run Final if any
        if ( _final_stage.size() > 0 && utils::is_file_existed(_final_stage) ) {
            coas::costage _fs(_stage.rootValue());
            auto _r = coas::run_stage_file(_final_stage, _fs);
            g_return = (_r ? 3 : 0);
            return;
        }
    } else {
        // We get a file or folder as input
        std::set< std::string > _stages;
        for ( auto& _arg : _input ) {
            if ( utils::is_file_existed(_arg) ) {
                _stages.insert(_arg);
            } else {
                utils::rek_scan_dir(
                    _arg, 
                    [&_stages, &_begin_stage, &_final_stage, &_report_stage](
                        const std::string& name, bool is_folder
                    ) {
                        if ( is_folder ) return true;
                        // if ( !is_folder ) _stages.insert(name);
                        if ( utils::extension(name) != "costage" ) return true;
                        if ( utils::filename(name) == "_begin" ) {
                            if ( _begin_stage.size() == 0 ) {
                                _begin_stage = name;
                            }
                            return true;
                        }
                        if ( utils::filename(name) == "_final" ) {
                            if ( _final_stage.size() == 0 ) {
                                _final_stage = name;
                            }
                            return true;
                        }
                        if ( utils::filename(name) == "_report" ) {
                            if ( _report_stage.size() == 0 ) {
                                _report_stage = name;
                            }
                            return true;
                        }
                        _stages.insert(name);
                        return true;
                    }
                );
            }
        }
        this_task::begin_tick();
        Json::Value _root(_rootValue);
        // We do have a begin stage
        if ( _begin_stage.size() > 0 ) {
            costage _bstage(_rootValue);
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
            loop::main.do_job([_sig, stage_file, _final_stage, &_root, &_result]() {
                this_task::begin_tick();
                costage _stage(_root);
                bool _ret = run_stage_file(stage_file, _stage);
                Json::Value _rj(Json::objectValue);
                _rj["name"] = _stage.name();
                _rj["description"] = _stage.description();
                _rj["state"] = _ret;
                _rj["time"] = this_task::tick();
                Json::Value _stack(Json::arrayValue);
                for ( auto&s : _stage.call_stack() ) {
                    _stack.append(s);
                }
                _rj["stack"] = _stack;

                // If we have final stage defined
                if ( _final_stage.size() > 0 ) {
                    costage _fstage(_stage.rootValue());
                    _ret = run_stage_file(_final_stage, _fstage);
                    _rj["root"] = _fstage.rootValue();
                } else {
                    _rj["root"] = _stage.rootValue();
                }

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
                std::cerr << "[" << CLI_GREEN << " PASS " << CLI_NOR << "]: ";
                _passed += 1;
            } else {                
                std::cerr << "[" << CLI_RED << "FAILED" << CLI_NOR << "]: ";
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
            std::cerr << i.key().asString() << ", " << __time_format(_stime) << std::endl;
        }
        std::cerr << 
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

        // Create the result json
        Json::Value _resultValue(Json::objectValue);
        _resultValue["result"] = _result;
        _resultValue["passed"] = _passed;
        _resultValue["failed"] = _failed;
        _resultValue["all"] = (_passed + _failed);
        _resultValue["time"] = _time;
        Json::Value _jmax(Json::objectValue);
        _jmax["stage"] = _max_time_stage;
        _jmax["time"] = _max_time;
        _resultValue["max"] = _jmax;
        Json::Value _jmin(Json::objectValue);
        _jmin["stage"] = _min_time_stage;
        _jmin["time"] = _min_time;
        _resultValue["min"] = _jmin;

        if ( _report_stage.size() > 0 ) {
            costage _rstage(_resultValue);
            bool _ret = run_stage_file(_report_stage, _rstage);
            if ( !_ret ) {
                g_return = 102; return;
            }
        }

        // Output the Report
        if ( _report_path.size() > 0 ) {
            std::ofstream _rfs(_report_path);
            _rfs << _resultValue;
        } else {
            std::ofstream _rfs("./report.json");
            _rfs << _resultValue;
        }

        // TODO: Output the report html template

        // Return Failed Count
        g_return = _failed;
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