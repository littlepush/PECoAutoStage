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

#include <iomanip>

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
        auto _flag = _stage.code_parser(_code, "playground", (uint32_t)_lno);
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
    std::string _sbegin = _dir + _fname + ".cobegin";
    std::string _send = _dir + _fname + ".coend";

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
                _bstage.dump_exec_log(_jexec);
                break;
            }
            if ( !coas::run_stage_file(_sbegin, _bstage) ) {
                g_return = ERR_IN_BEGIN;
                _output["error"] = _bstage.err_string();
                _output["root"] = _bstage.rootValue();
                _output["return"] = g_return;
                _bstage.stack_toJson(_output);
                _bstage.dump_exec_log(_jexec);
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
            _pstage->dump_exec_log(_jexec);
            break;
        }
        if ( !coas::run_stage_file(stage_file, *_pstage) ) {
            g_return = ERR_IN_STAGE;
            _output["error"] = _pstage->err_string();
            _pstage->stack_toJson(_output);
            _output["root"] = _pstage->rootValue();
            _output["return"] = g_return;
            _pstage->dump_exec_log(_jexec);
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
                _estage.dump_exec_log(_jexec);
                break;
            }
            if ( !coas::run_stage_file(_send, _estage) ) {
                g_return = ERR_IN_END;
                _output["error"] = _estage.err_string();
                _estage.stack_toJson(_output);
                _output["root"] = _estage.rootValue();
                _output["return"] = g_return;
                _estage.dump_exec_log(_jexec);
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

void rek_dump_report(Json::Value& report_node, int& p, int& f, int& u, bool print = true) {
    if ( report_node.isMember("stages") ) {
        Json::Value& _jstages = report_node["stages"];
        for ( auto i = _jstages.begin(); i != _jstages.end(); ++i ) {
            std::string _state = (*i)["state"].asString();
            if ( _state == "passed" ) {
                if ( print ) {
                    std::cout << "[" << CLI_GREEN << "PASSED" << CLI_NOR << "]: " 
                        << i.key().asString() << " ";
                }
                ++p;
            }
            else if ( _state == "unknown" ) {
                if ( print ) {
                    std::cout << "[" << CLI_YELLOW << "UNKNOWN" << CLI_NOR << "]: " 
                        << i.key().asString() << " ";
                }
                ++u;
            }
            else {
                if ( print ) {
                    std::cout << "[" << CLI_RED << "FAILED" << CLI_NOR << "]: " 
                        << i.key().asString() << " ";
                }
                ++f;
            }
            if ( print ) {
                if ( i->isMember("time") ) {
                    std::cout << "(+" << __time_format((*i)["time"].asDouble()) << ")";
                }
                std::cout << std::endl;
            }
        }
    }
    if ( report_node.isMember("children") ) {
        Json::Value& _jchildren = report_node["children"];
        for ( auto i = _jchildren.begin(); i != _jchildren.end(); ++i ) {
            rek_dump_report((*i), p, f, u);
        }
    }
}

const std::string gReportDefaultTemplate =
"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"
"AA==";

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
        ignore_result(chdir(_work_dir.c_str()));
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
        // Dump basic stage report
        int _passed = 0, _failed = 0, _unknow = 0;
        rek_dump_report(_rootGroup, _passed, _failed, _unknow, !_quiet);
        _reportValue["time"] = _running_time;
        _reportValue["passed"] = _passed;
        _reportValue["failed"] = _failed;
        _reportValue["unknown"] = _unknow;

        if ( !_no_report ) {
            std::string _reportHtml;
            if ( _report_template.size() > 0 ) {
                std::ifstream _rtf(_report_template);
                if ( !_rtf ) {
                    std::cerr << "cannot open report template" << std::endl;
                    _reportHtml = utils::gunzip_data(utils::base64_decode(gReportDefaultTemplate));
                } else {
                    std::string _rts((std::istreambuf_iterator<char>(_rtf)),
                                     std::istreambuf_iterator<char>());
                    _reportHtml = std::move(_rts);
                }
            } else {
                _reportHtml = utils::gunzip_data(utils::base64_decode(gReportDefaultTemplate));
            }
            auto _placehold = _reportHtml.find("$COAS.REPORT.RAW$");
            size_t _plen = strlen("$COAS.REPORT.RAW$");

            if ( _placehold == std::string::npos ) {
                std::cerr 
                    << "invalidate report template, missing placeholder: $COAS.REPORT.RAW$" 
                    << std::endl;
            } else {
                Json::FastWriter _rfw;
                std::string _reportContent = _rfw.write(_reportValue);
                _reportContent = utils::base64_encode(_reportContent);
                std::string _rp = (_report_path.size() > 0 ? 
                    _report_path : g_execPath + "/report.html");
                FILE *_reportfs = fopen(_rp.c_str(), "w+");
                if ( _reportfs == NULL ) {
                    std::cerr << "cannot open " << _rp << " for output" << std::endl;
                } else {
                    fwrite(_reportHtml.c_str(), sizeof(char), _placehold, _reportfs);
                    fwrite(_reportContent.c_str(), sizeof(char), _reportContent.size(), _reportfs);
                    fwrite(_reportHtml.c_str() + _placehold + _plen, sizeof(char), 
                        _reportHtml.size() - _placehold - _plen, _reportfs);
                    fclose(_reportfs);
                }
            }

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