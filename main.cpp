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
Json::Value g_rootValue;
std::set< std::vector< std::string > > g_tagGroup;
std::string g_moduleFile;
std::string g_execPath;
std::string g_workPath;
std::string g_filterPath;
std::string g_binary;
std::map< std::string, bool > g_filterMap;
std::shared_ptr< condition > g_condition;

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

void load_modules( const std::string& module_file ) {
    if ( !utils::is_file_existed(module_file) ) return;
    // Load Modules
    std::string _ml;
    std::ifstream _mf(module_file);
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

void print_call_stack(const std::list< std::string >& callstack) {
    std::string _space;
    for ( auto& s: callstack ) {
        std::cerr << "- " << _space << s << std::endl;
        _space += "  ";
    }
}
void run_playground(Json::Value& root) {
    // Begin Input from STDIN
    coas::costage _stage(root);
    std::string _code;
    int _lno = 0;
    std::cout << "coas(" << _lno << "): ";
    while ( std::getline(std::cin, _code) ) {
        if ( _code == ".exit" ) break;
        auto _flag = _stage.code_parser(std::move(_code));
        if ( _flag == coas::I_SYNTAX ) {
            std::cerr << "Syntax Error: " << _stage.err_string() << std::endl;
            g_return = -99;
            break;
        } else if ( _flag == coas::I_UNFINISHED ) {
            continue;
        }
        // Now we get a full code, run it
        auto _ret = _stage.code_run();
        if ( _ret == coas::E_ASSERT ) {
            std::cerr << "Assert Failed: " << _stage.err_string() << std::endl;
            print_call_stack(_stage.call_stack());
            g_return = -98;
            break;
        } 
        _stage.code_clear();
        // Run OK, then continue to get next code
        ++_lno;
        std::cout << "coas(" << _lno << "): ";
    }
}

bool is_match_tags( const std::set< std::string >& tags ) {
    if ( g_tagGroup.size() == 0 ) return true;
    if ( tags.size() == 0 ) return false;

    for ( const auto& tg: g_tagGroup ) {
        bool _has_unmatch_group = false;
        for ( const auto& t : tg ) {
            bool _contains = false;
            for ( const auto& tt : tags ) {
                if ( t != tt ) continue;
                _contains = true;
                break;
            }
            if ( _contains ) continue;
            _has_unmatch_group = true;
            break;
        }
        if ( _has_unmatch_group ) continue;
        return true;
    }
    return false;
}

struct stage_group_t {
    std::string                                     name;
    std::string                                     begin_stage;
    std::string                                     end_stage;
    std::list< std::string >                        stage_list;
    std::list< std::shared_ptr<stage_group_t> >     children;
};

enum {
    SYNTAX_IN_BEGIN     = 1,
    ERR_IN_BEGIN        = 2,
    SYNTAX_IN_STAGE     = 3,
    ERR_IN_STAGE        = 4,
    SYNTAX_IN_END       = 5,
    ERR_IN_END          = 6
};

bool is_available_stage( const std::string& fname ) {
    coas::stage_info_t _sinfo;
    bool _r = coas::fetch_stage_info(fname, _sinfo);
    if ( _r == false ) return false;

    if ( g_filterMap.size() > 0 ) {
        if ( _sinfo.name.size() == 0 ) return false;
        if ( g_filterMap.find(_sinfo.name) == g_filterMap.end() ) return false;
    }

    return is_match_tags( _sinfo.tags );
}

void rek_scan_stages( 
    std::shared_ptr< stage_group_t > pgroup, 
    const std::string& target 
) {
    std::string _ftarget = target;
    if ( _ftarget.back() == '/' ) _ftarget.pop_back();
    std::string _path = _ftarget;
    _ftarget = utils::full_filename(_ftarget);
    if ( utils::is_file_existed(_path) ) {
        // Ignore default costage
        if ( _ftarget == "_begin.costage" ) {
            pgroup->begin_stage = _path;
            return;
        }
        if ( _ftarget == "_end.costage" ) {
            pgroup->end_stage = _path;
            return;
        }
        if ( _ftarget == "_report.costage" ) return;
        // Ignore non-costage
        if ( utils::extension(_ftarget) != "costage" ) return;

        // Check if the target is validate
        if ( is_available_stage(_path) ) {
            pgroup->stage_list.push_back(_path);
        }
    } else if ( utils::is_folder_existed(_path) ) {
        std::shared_ptr< stage_group_t > _subgroup(new stage_group_t);
        _subgroup->name = _ftarget;
        utils::rek_scan_dir(_path, 
            [&_subgroup](const std::string& item, bool is_folder) {
                rek_scan_stages(_subgroup, item);
                return true;
            }
        );
        if ( 
            _subgroup->stage_list.size() == 0 && 
            _subgroup->children.size() == 0 
        ) {
            return;
        } 
        pgroup->children.push_back(_subgroup);
    }
}

size_t stage_count_of_group( std::shared_ptr< stage_group_t > pgroup ) {
    size_t _subsize = 0;
    for ( auto& _sub : pgroup->children ) {
        _subsize += stage_count_of_group(_sub);
    }
    return _subsize + pgroup->stage_list.size();
}

void init_report_value(std::shared_ptr< stage_group_t > pgroup, Json::Value& subReportNode ) {
    subReportNode["workspace"] = pgroup->name;
    subReportNode["begin"] = pgroup->begin_stage;
    subReportNode["end"] = pgroup->end_stage;
    subReportNode["state"] = "unknown";
    subReportNode["stages"] = Json::Value(Json::objectValue);
    Json::Value& _jstages = subReportNode["stages"];
    for ( const auto& sf : pgroup->stage_list ) {
        coas::stage_info_t _sinfo;
        coas::fetch_stage_info(sf, _sinfo);

        Json::Value _node(Json::objectValue);
        _node["file"] = sf;
        _node["state"] = "unknown";
        _node["time"] = 0.f;
        _node["name"] = _sinfo.name;
        _node["description"] = _sinfo.description;
        _node["tags"] = Json::Value(Json::arrayValue);
        Json::Value& _jtag = _node["tags"];
        for ( auto& t: _sinfo.tags ) {
            _jtag.append(t);
        }
        _jstages[sf] = _node;
    }
    subReportNode["children"] = Json::Value(Json::objectValue);
    Json::Value& _jchildren = subReportNode["children"];
    for ( const auto& sg : pgroup->children ) {
        _jchildren[sg->name] = Json::Value(Json::objectValue);
        Json::Value& _jsgnode = _jchildren[sg->name];
        init_report_value(sg, _jsgnode);
    }
}

void run_single_stage(
    const Json::Value& stage_root, 
    const std::string& stage_file,
    bool print_result = false
) {
    std::string _dir = utils::dirname(stage_file);
    std::string _fname = utils::filename(stage_file);
    std::string _sbegin = _dir + _fname + ".begin";
    std::string _send = _dir + _fname + ".end";

    std::shared_ptr< coas::costage > _pstage(nullptr);
    Json::Value _output(Json::objectValue);
    _output["exec_log"] = Json::Value(Json::arrayValue);
    Json::Value& _jexec = _output["exec_log"];

    do {
        if ( utils::is_file_existed(_sbegin) ) {
            coas::costage _bstage(stage_root);
            if ( !coas::parse_stage_file(_sbegin, _bstage) ) {
                g_return = SYNTAX_IN_BEGIN;
                _output["error"] = _bstage.err_string();
                _output["root"] = _bstage.rootValue();
                _output["return"] = g_return;
                break;
            }
            if ( !coas::run_stage_file(_sbegin, _bstage) ) {
                g_return = ERR_IN_BEGIN;
                _output["error"] = _bstage.err_string();
                _output["root"] = _bstage.rootValue();
                _output["return"] = g_return;
                _bstage.stack_toJson(_output);
                break;
            }
            _bstage.dump_exec_log(_jexec);
            _pstage = std::make_shared<coas::costage>(_bstage.rootValue());
        } else {
            _pstage = std::make_shared<coas::costage>(stage_root);
        }

        if ( !coas::parse_stage_file(stage_file, *_pstage) ) {
            g_return = SYNTAX_IN_STAGE;
            _output["error"] = _pstage->err_string();
            _output["root"] = _pstage->rootValue();
            _output["return"] = g_return;
            break;
        }
        if ( !coas::run_stage_file(stage_file, *_pstage) ) {
            g_return = ERR_IN_STAGE;
            _output["error"] = _pstage->err_string();
            _pstage->stack_toJson(_output);
            _output["root"] = _pstage->rootValue();
            _output["return"] = g_return;
            break;
        }
        _pstage->dump_exec_log(_jexec);

        if ( utils::is_file_existed(_send) ) {
            coas::costage _estage(_pstage->rootValue());
            if ( !coas::parse_stage_file(_send, _estage) ) {
                g_return = SYNTAX_IN_END;
                _output["error"] = _estage.err_string();
                _output["root"] = _estage.rootValue();
                _output["return"] = g_return;
                break;
            }
            if ( !coas::run_stage_file(_send, _estage) ) {
                g_return = ERR_IN_END;
                _output["error"] = _estage.err_string();
                _estage.stack_toJson(_output);
                _output["root"] = _estage.rootValue();
                _output["return"] = g_return;
                break;
            }
            _output["root"] = _estage.rootValue();
            _output["return"] = 0;
            _estage.dump_exec_log(_jexec);
        } else {
            _output["root"] = _pstage->rootValue();
            _output["return"] = 0;
        }
    } while ( false );
    if ( print_result == false ) return;
    std::cout << "COAS_FORK_CHILD_RESULT_OUTPUT_BOUNDARY_BEGIN" << std::endl;
    std::cout << _output << std::endl;
    std::cout << "COAS_FORK_CHILD_RESULT_OUTPUT_BOUNDARY_END" << std::endl;
    std::cout << std::flush;
    this_task::sleep(std::chrono::milliseconds(1));
}

void run_single_stage( 
    const Json::Value& stage_root, 
    Json::Value& report_node,
    const std::string& stage_file
) {
    loop::main.do_job([stage_root, &report_node, stage_file]() {
        process _fchild(g_binary);
        _fchild << 
            "--no-report" << 
            "--is-fork-child" << 
            "--module_file=" + g_moduleFile << 
            "--root=-" <<
            stage_file;

        std::stringstream _oss;
        report_node["console"] = Json::Value(Json::arrayValue);
        Json::Value& _jconsole = report_node["console"];
        bool _eol = true;
        _fchild.stdout = [&_oss](std::string&& data) {
            _oss << data;
        };
        _fchild.stderr = [&_jconsole, &_eol](std::string&& data) {
            auto _lines = utils::split(data, "\n");
            if ( _lines.size() == 0 ) {
                _eol = true;
                return;
            }
            if ( _eol ) _jconsole.append(_lines[0]);
            else {
                Json::Value& _last = _jconsole[_jconsole.size() - 1];
                _last = (_last.asString() + _lines[0]);
            }
            for ( size_t i = 1; i < _lines.size(); ++i ) {
                _jconsole.append(_lines[i]);
            }
            _eol = (data.back() == '\n');
        };
        int _ret = 99;
        this_task::begin_tick();
        loop::main.do_job([&_fchild, &_ret]() {
            parent_task::guard _pg;
            _ret = _fchild.run();
        });
        Json::FastWriter _jw;
        _fchild.input(_jw.write(stage_root));
        _fchild.send_eof();
        // Wait for the task to quit
        this_task::holding();
        report_node["time"] = this_task::tick();
        g_condition->notify();

        std::string _childoutput(_oss.str());
        size_t _bboundary = _childoutput.find("COAS_FORK_CHILD_RESULT_OUTPUT_BOUNDARY_BEGIN");
        size_t _eboundary = _childoutput.find("COAS_FORK_CHILD_RESULT_OUTPUT_BOUNDARY_END");
        if ( _bboundary != std::string::npos ) {
            _bboundary += strlen("COAS_FORK_CHILD_RESULT_OUTPUT_BOUNDARY_BEGIN");
            size_t _outputlen = _eboundary - _bboundary;
            std::string _outstr = _childoutput.substr(_bboundary, _outputlen);

            Json::Reader _er;
            Json::Value _enode;
            _er.parse(_outstr, _enode);
            if ( _enode.isMember("error") ) {
                report_node["error"] = _enode["error"];
            }
            if ( _enode.isMember("callstack") ) {
                report_node["callstack"] = _enode["callstack"];
            }
            if ( _enode.isMember("root") ) {
                report_node["root"] = _enode["root"];
            }
            if ( _enode.isMember("return") ) {
                _ret = _enode["return"].asInt();
            }
            if ( _enode.isMember("exec_log") ) {
                report_node["exec_log"] = _enode["exec_log"];
            }
        } else {
            // Crashed
            _ret = 99;
        }
        if ( _ret == 0 ) {
            report_node["state"] = "passed";
            return;
        } 
        if ( _ret == SYNTAX_IN_BEGIN ) {
            report_node["state"] = "bsyntax";
        } else if ( _ret == ERR_IN_BEGIN ) {
            report_node["state"] = "bfailed";
        } else if ( _ret == SYNTAX_IN_STAGE ) {
            report_node["state"] = "syntax";
        } else if ( _ret == ERR_IN_STAGE ) {
            report_node["state"] = "failed";
        } else if ( _ret == SYNTAX_IN_END ) {
            report_node["state"] = "esyntax";
        } else if ( _ret == ERR_IN_END ) {
            report_node["state"] = "efailed";
        } else {
            report_node["state"] = "crashed";
        }
        g_return += 1;
    });
}

void rek_skip_stages(
    std::shared_ptr< stage_group_t > pgroup
) {
    for ( auto i = pgroup->stage_list.begin(); i != pgroup->stage_list.end(); ++i ) {
        g_condition->notify();
    }
    for ( auto& sg : pgroup->children ) {
        rek_skip_stages( sg );
    }
}

void rek_run_stage( 
    const Json::Value& sub_root, 
    Json::Value& report_node, 
    std::shared_ptr< stage_group_t > pgroup 
) {
    Json::Value _root(sub_root);
    if ( pgroup->begin_stage.size() > 0 ) {
        coas::costage _beginStage(_root);
        if ( !coas::parse_stage_file(pgroup->begin_stage, _beginStage) ) {
            report_node["state"] = "bsyntax";
            report_node["error"] = _beginStage.err_string();
            rek_skip_stages(pgroup);
            return;
        }
        if ( !coas::run_stage_file(pgroup->begin_stage, _beginStage) ) {
            report_node["stage"] = "bfailed";
            report_node["error"] = _beginStage.err_string();
            _beginStage.stack_toJson(report_node);
            rek_skip_stages(pgroup);
            return;
        }
        _root =_beginStage.rootValue();
    }
    report_node["state"] = "processed";
    report_node["begin_root"] = _root;
    for ( const auto& sf : pgroup->stage_list ) {
        run_single_stage( _root, report_node["stages"][sf], sf);
    }
    for ( auto& sg : pgroup->children ) {
        rek_run_stage( _root, report_node["children"][sg->name], sg );
    }
}

void rek_end_stage(
    Json::Value& report_node, 
    std::shared_ptr< stage_group_t > pgroup 
) {
    for ( const auto& sg : pgroup->children ) {
        rek_end_stage( report_node["children"][sg->name], sg );
    }
    // Begin root not success
    if ( !report_node.isMember("begin_root") ) return;
    // No End stage
    if ( pgroup->end_stage.size() == 0 ) return;
    coas::costage _endStage(report_node["begin_root"]);
    if ( !coas::parse_stage_file(pgroup->end_stage, _endStage) ) {
        report_node["state"] = "esyntax";
        report_node["error"] = _endStage.err_string();
        return;
    }
    if ( !coas::run_stage_file(pgroup->end_stage, _endStage) ) {
        report_node["stage"] = "efailed";
        report_node["error"] = _endStage.err_string();
        _endStage.stack_toJson(report_node);
        return;
    }
}

void rek_dump_report(Json::Value& report_node, int& p, int& f, int& u) {
    if ( report_node.isMember("stages") ) {
        Json::Value& _jstages = report_node["stages"];
        for ( auto i = _jstages.begin(); i != _jstages.end(); ++i ) {
            std::string _state = (*i)["state"].asString();
            if ( _state == "passed" ) {
                std::cout << "[" << CLI_GREEN << "PASSED" << CLI_NOR << "]: " 
                    << i.key().asString() << " ";
                ++p;
            }
            else if ( _state == "unknown" ) {
                std::cout << "[" << CLI_YELLOW << "UNKNOWN" << CLI_NOR << "]: " 
                    << i.key().asString() << " ";
                ++u;
            }
            else {
                std::cout << "[" << CLI_RED << "FAILED" << CLI_NOR << "]: " 
                    << i.key().asString() << " ";
                ++f;
            }
            if ( i->isMember("time") ) {
                std::cout << "(+" << __time_format((*i)["time"].asDouble()) << ")";
            }
            std::cout << std::endl;
        }
    }
    if ( report_node.isMember("children") ) {
        Json::Value& _jchildren = report_node["children"];
        for ( auto i = _jchildren.begin(); i != _jchildren.end(); ++i ) {
            rek_dump_report((*i), p, f, u);
        }
    }
}

void co_main( int argc, char* argv[] ) {
    g_binary = argv[0];
    bool _normal_return = false;
    g_moduleFile = std::string(getenv("HOME")) + ".coas.modules";
    std::string _root;
    std::string _report_template;
    std::string _report_path;
    std::string _work_dir;
    bool _playground = false;
    bool _no_report = false;
    bool _print_report = false;
    bool _is_fork_child = false;
    bool _quiet = false;

    utils::argparser::set_parser("module_file", "m", g_moduleFile);
    utils::argparser::set_parser("tag", "t", [](std::string&& value) {
        auto _r = utils::split(value, ",");
        g_tagGroup.insert(std::move(_r));
    });
    utils::argparser::set_parser("root", "r", _root);
    utils::argparser::set_parser("filter", "f", g_filterPath);
    utils::argparser::set_parser("report-path", "rp", _report_path);
    utils::argparser::set_parser("report-template", "rt", _report_template);
    utils::argparser::set_parser("work-dir", "w", _work_dir);
    utils::argparser::set_parser("playground", "p", [&_playground](std::string&&) {
        _playground = true;
    });
    utils::argparser::set_parser("no-report", "np", [&_no_report](std::string&&) {
        _no_report = true;
    });
    utils::argparser::set_parser("print-report", "pr", [&_print_report](std::string&&) {
        _print_report = true;
    });
    utils::argparser::set_parser("is-fork-child", "fc", [&_is_fork_child](std::string&&) {
        _is_fork_child = true;
    });
    utils::argparser::set_parser("quiet", "q", [&_quiet](std::string&&) {
        _quiet = true;
    });

    ON_DEBUG(
        utils::argparser::set_parser("enable-conet-trace", [](std::string&&) {
            net::enable_conet_trace();
        });
        utils::argparser::set_parser("enable-cotask-trace", [](std::string&&) {
            enable_cotask_trace();
        });
    )

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
            << "  -p, --playground          Start coas in playground mode" << std::endl
            << "  -np, --no-report          Run cases without report stage" << std::endl
            << "  -q, --quiet               No output" << std::endl
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
        g_return = -1;
        return;
    }
    // On output info command
    if ( _normal_return ) return;
    coas::module_manager::init_default_modules();

    do {
        char _path[1024];
        g_execPath = getcwd(_path, 1024);
    } while ( false );
    if ( *g_execPath.rbegin() != '/' ) g_execPath += '/';

    if ( g_filterPath.size() != 0 ) {
        do {
            if ( g_filterPath[0] == '/' ) break;
            if ( g_filterPath[0] == '~' ) break;
            g_filterPath = g_execPath + g_filterPath;
        } while ( false );
        if ( !utils::is_file_existed(g_filterPath) ) {
            std::cerr << "filter " << g_filterPath << " not existed" << std::endl;
            g_return = -2;
            return;
        }
    }

    Json::Value _filterList;
    if ( g_filterPath.size() > 0 ) {
        std::ifstream _filterfs(g_filterPath);
        Json::Reader _fjr;
        if ( !_fjr.parse(_filterfs, _filterList) ) {
            std::cerr << "filter " << g_filterPath << " is not validate: "
                << _fjr.getFormattedErrorMessages() << std::endl;
            g_return = -2;
            return;
        }
        if ( !_filterList.isArray() ) {
            std::cerr << "filter must be an JSON array" << std::endl;
            g_return = -2;
            return;
        }
    }
    for ( Json::ArrayIndex i = 0; i < _filterList.size(); ++i ) {
        g_filterMap[_filterList[i].asString()] = true;
    }

    // Load modules from file
    load_modules(g_moduleFile);

    // Load Root Json File or from STDIN
    if ( _root.size() > 0 ) {
        Json::Reader _jr;
        if ( _root == "-" ) {
            if ( _playground ) {
                std::cerr << "cannot get root from stdin when in playground mode" 
                    << std::endl;
                g_return = -4;
                return;
            }
            // Get input from stdin
            std::stringstream _rootss;
            std::string _input;
            while ( std::getline(std::cin, _input) ) {
                _rootss << _input;
            }
            if ( !_jr.parse(_rootss, g_rootValue, false) ) {
                std::cerr << "failed to load root node: " << 
                    _jr.getFormattedErrorMessages() << std::endl;
                g_return = -4;
                return;
            }
        } else {
            if ( utils::is_file_existed(_root) ) {
                std::ifstream _rf(_root);
                if ( !_jr.parse(_rf, g_rootValue, false) ) {
                    std::cerr << "failed to load root node: " << 
                        _jr.getFormattedErrorMessages() << std::endl;
                    g_return = -4;
                    return;
                }
            } else {
                std::cerr << "root file doesn't existed" << std::endl;
                g_return = -4;
                return;
            }
        }
    }

    // Change work dir
    if ( _work_dir.size() > 0 ) {
        if ( !utils::is_folder_existed(_work_dir) ) {
            std::cerr << "work dir not existed" << std::endl;
            g_return = -3;
            return;
        }
        chdir(_work_dir.c_str());
    }
    // The work path is always "./"
    g_workPath = "./";

    // Run Playground Mode
    if ( _playground ) {
        return run_playground(g_rootValue);
    }

    auto _input = utils::argparser::individual_args();
    g_condition = std::make_shared<condition>();
    if ( _is_fork_child ) {
        if ( _input.size() != 1 ) {
            std::cerr << "invalidate fork process, missing target to run" << std::endl;
            g_return = -4;
            return;
        }
        if ( !utils::is_file_existed(_input[0]) ) {
            std::cerr << "invalidate for process, input need to be a stage file" << std::endl;
            g_return = -4;
            return;
        }
        // TODO: Run a single file
        run_single_stage(g_rootValue, _input[0], true);
    } else {
        // Scan to create stage group list
        if ( _input.size() == 0 ) _input.push_back("./");
        std::shared_ptr< stage_group_t > _pStageGroup(new stage_group_t);
        _pStageGroup->name = "./";
        for ( auto& s : _input ) {
            rek_scan_stages(_pStageGroup, s);
        }
        if ( 
            _pStageGroup->stage_list.size() == 0 &&
            _pStageGroup->children.size() == 0
        ) {
            std::cerr << "missing target, with empty stage group" << std::endl;
            g_return = -5;
            return;
        }

        Json::Value _reportValue(Json::objectValue);
        _reportValue["time"] = 0.f;
        _reportValue["date"] = (long long)time(NULL);
        _reportValue["workpath"] = g_workPath;
        _reportValue["filter"] = Json::Value(Json::arrayValue);
        Json::Value& _jfilter = _reportValue["filter"];
        for ( auto& kv : g_filterMap ) {
            _jfilter.append(kv.first);
        }
        _reportValue["tags"] = Json::Value(Json::arrayValue);
        Json::Value& _jtags = _reportValue["tags"];
        for ( auto& tl : g_tagGroup ) {
            Json::Value _jtg(Json::arrayValue);
            for ( auto& t : tl ) {
                _jtg.append(t);
            }
            _jtags.append(_jtg);
        }

        _reportValue["result"] = Json::Value(Json::objectValue);
        Json::Value& _rootGroup = _reportValue["result"];
        init_report_value(_pStageGroup, _rootGroup);
        size_t _all_stage_count = stage_count_of_group(_pStageGroup);
        rek_run_stage(g_rootValue, _rootGroup, _pStageGroup);
        size_t _finished = 0;
        this_task::begin_tick();
        if ( !_quiet ) {
            std::cout << "\r" << "Stages: (" << _finished << "/" << _all_stage_count << ")";
            std::cout << std::flush;
        }
        while ( _finished != _all_stage_count ) {
            g_condition->wait(nullptr);
            ++_finished;
            if ( !_quiet ) {
                std::cout << "\r" << "Stages: (" << _finished << "/" << _all_stage_count << ")";
                std::cout << std::flush;
            }
            // this_task::sleep(std::chrono::seconds(1));
        }
        std::cout << std::endl;
        rek_end_stage(_rootGroup, _pStageGroup);
        double _running_time = this_task::tick();
        _reportValue["time"] = _running_time;

        if ( _print_report ) {
            std::cout << _reportValue;
        }
        if ( !_no_report ) {
            std::string _rp = (_report_path.size() > 0 ? 
                _report_path : g_execPath + "/report.json");
            std::ofstream _reportfs(_rp);
            _reportfs << _reportValue;
            _reportfs.close();

            if ( utils::is_file_existed("./_report.costage") ) {
                costage _rs(_reportValue);
                if ( ! coas::parse_stage_file("./_report.costage", _rs) ) {
                    std::cerr << "Parse _report.costage failed, will not send report" << 
                        ", please read report.json under working dir" << std::endl;
                } else {
                    coas::run_stage_file("./_report.costage", _rs);
                }
            }
        }
        if ( !_quiet ) {
            // Dump basic stage report
            int _passed = 0, _failed = 0, _unknow = 0;
            rek_dump_report(_rootGroup, _passed, _failed, _unknow);
            std::cout << "Time Used: " << __time_format(_running_time) << std::endl;
            std::cout << "All Stage: " << _all_stage_count << std::endl;
            std::cout << "Passed: " << _passed << std::endl;
            std::cout << "Failed: " << _failed << std::endl;
            std::cout << "Unknown: " << _unknow << std::endl;
        }
    }

    return;
}

int main( int argc, char* argv[] ) {
    loop::main.do_job(std::bind(&co_main, argc, argv));
    loop::main.run();
    for ( void *_m : g_modules ) {
        dlclose(_m);
    }
    return g_return;
}