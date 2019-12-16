/*
    modules.cpp
    2019-12-13
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

#include <cotask.h>
#include <conet.h>
using namespace pe::co;

#include "modules.h"
#include "costage.h"

namespace coas {

    bool __func_default_match(const Json::Value& invoker, bool is_root) {
        return ( invoker.isArray() || invoker.isObject() );
    }
    bool __func_root_match(const Json::Value& invoker, bool is_root) {
        return is_root;
    }
    bool __func_array_match(const Json::Value& invoker, bool is_root) {
        return invoker.isArray();
    }
    bool __func_object_match(const Json::Value& invoker, bool is_root) {
        return invoker.isObject();
    }
    bool __func_string_match(const Json::Value& invoker, bool is_root) {
        return invoker.isString();
    }

    bool __func_job_match(const Json::Value& invoker, bool is_root) {
        if ( !invoker.isObject() ) return false;
        if ( 
            !invoker.isMember("__stack") || 
            !invoker.isMember("__type") ||
            invoker["__type"].asString() != "coas.job"
        ) return false;
        return true;
    }
    bool __func_http_match(const Json::Value& invoker, bool is_root) {
        if ( !invoker.isObject() ) return false;
        if ( 
            !invoker.isMember("url") ||
            !invoker.isMember("__type") || 
            invoker["__type"].asString() != "coas.httpreq"
        ) return false;
        return true;
    }
    rpn::item_t __func_compare(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args,
        std::function< bool(const Json::Value&, const Json::Value&) > comp
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        if ( args.size() > 1 ) {
            return module_manager::ret_error("too many arguments");
        }
        const rpn::item_t* _parg = NULL;
        if ( args.size() == 1 ) { 
            auto _itarg = args.begin();
            _parg = &(*_itarg);
            if ( _parg->type != rpn::IT_EXEC ) {
                return module_manager::ret_error("invalidate argument");
            }
        }

        if ( _pthis->isArray() ) {
            if ( _parg == NULL ) {
                Json::Value* _plast = &(*_pthis)[0];
                for ( Json::ArrayIndex i = 1; i < _pthis->size(); ++i ) {
                    if ( comp((*_pthis)[i], *_plast) ) _plast = &(*_pthis)[i];
                }
                rpn::item_t _ret{rpn::IT_NUMBER, *_plast};
                if ( _plast->isObject() ) _ret.type = rpn::IT_OBJECT;
                else if ( _plast->isNumeric() ) _ret.type = rpn::IT_NUMBER;
                else if ( _plast->isString() ) _ret.type = rpn::IT_STRING;
                else if ( _plast->isBool() ) _ret.type = rpn::IT_BOOL;
                return _ret;
            } else {
                int _ilast = 0;
                for ( Json::ArrayIndex i = 0; i < _pthis->size(); ++i ) {
                    rpn::item_t _this = this_path;
                    _this.value.append((int)i);
                    rpn::item_t _last = this_path;
                    _last.value.append(_ilast);
                    stage.push_this(std::move(_this));
                    stage.push_last(std::move(_last));

                    // Invoke the sub func to compare
                    auto _e = stage.invoke_group( _parg->value.asString() );

                    stage.pop_last();
                    stage.pop_this();

                    // toast the error message
                    if (_e == E_ASSERT) return module_manager::ret_error(stage.err_string());
                    if (_e == E_OK ) return module_manager::ret_error("missing compare result");
                    if ( stage.returnValue().isBool() == false ) {
                        return module_manager::ret_error("invalidate return type");
                    }
                    if ( stage.returnValue().asBool() ) _ilast = i;
                }
                rpn::item_t _ret = this_path;
                _ret.value.append(_ilast);
                return _ret;
            }
        } else {
            if ( _parg == NULL ) return module_manager::ret_error("missing compare function");
            auto _i = _pthis->begin();
            std::string _nlast = _i.key().asString();
            for (; _i != _pthis->end(); ++_i ) {
                rpn::item_t _this = this_path;
                _this.value.append(_i.key().asString());
                rpn::item_t _last = this_path;
                _last.value.append(_nlast);
                stage.push_this(std::move(_this));
                stage.push_last(std::move(_last));

                // Invoke the sub func to compare
                auto _e = stage.invoke_group( _parg->value.asString() );

                stage.pop_last();
                stage.pop_this();

                // toast the error message
                if (_e == E_ASSERT) return module_manager::ret_error(stage.err_string());
                if (_e == E_OK ) return module_manager::ret_error("missing compare result");
                if ( stage.returnValue().isBool() == false ) {
                    return module_manager::ret_error("invalidate return type");
                }
                if ( stage.returnValue().asBool() ) _nlast = _i.key().asString();
            }
            rpn::item_t _ret = this_path;
            _ret.value.append(_nlast);
            return _ret;
        }
    }
    rpn::item_t __func_max( 
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        return __func_compare(stage, this_path, args, 
            [](const Json::Value& _this, const Json::Value& _last) {
                return _this > _last;
            }
        );
    }
    rpn::item_t __func_min( 
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        return __func_compare(stage, this_path, args, 
            [](const Json::Value& _this, const Json::Value& _last) {
                return _this < _last;
            }
        );
    }
    rpn::item_t __func_fetch(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        if ( args.size() != 1 ) {
            return module_manager::ret_error("missing or invalidate arguments");
        }
        const rpn::item_t* _parg = NULL;
        auto _itarg = args.begin();
        _parg = &(*_itarg);
        if ( _parg->type != rpn::IT_STACK ) {
            return module_manager::ret_error("invalidate argument");
        }

        if ( _pthis->isArray() ) {
            for ( Json::ArrayIndex i = 0; i < _pthis->size(); ++i ) {
                rpn::item_t _this = this_path;
                _this.value.append((int)i);
                rpn::item_t _last = this_path;
                _last.value.append(i == 0 ? 0 : (i - 1));
                stage.push_this(std::move(_this));
                stage.push_last(std::move(_last));

                // Invoke the sub func to compare
                auto _e = stage.invoke_group( _parg->value.asString() );

                stage.pop_last();
                stage.pop_this();

                // toast the error message
                if (_e == E_ASSERT) return module_manager::ret_error(stage.err_string());
                if (_e == E_OK ) return module_manager::ret_error("missing compare result");
                if ( stage.returnValue().isBool() == false ) {
                    return module_manager::ret_error("invalidate return type");
                }
                // First match
                if ( stage.returnValue().asBool() ) return _this;
            }
            return rpn::item_t{rpn::IT_NULL, Json::Value(Json::nullValue)};
        } else {
            if ( _parg == NULL ) return module_manager::ret_error("missing compare function");
            auto _i = _pthis->begin();
            std::string _nlast = _i.key().asString();
            for (; _i != _pthis->end(); ++_i ) {
                rpn::item_t _this = this_path;
                _this.value.append(_i.key().asString());
                rpn::item_t _last = this_path;
                _last.value.append(_nlast);
                stage.push_this(std::move(_this));
                stage.push_last(std::move(_last));

                // Invoke the sub func to compare
                auto _e = stage.invoke_group( _parg->value.asString() );

                stage.pop_last();
                stage.pop_this();

                // toast the error message
                if (_e == E_ASSERT) return module_manager::ret_error(stage.err_string());
                if (_e == E_OK ) return module_manager::ret_error("missing compare result");
                if ( stage.returnValue().isBool() == false ) {
                    return module_manager::ret_error("invalidate return type");
                }
                if ( stage.returnValue().asBool() ) return _this;
            }
            return rpn::item_t{rpn::IT_NULL, Json::Value(Json::nullValue)};
        }
    }
    rpn::item_t __func_foreach(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        if ( args.size() != 1 ) {
            return module_manager::ret_error("missing or invalidate arguments");
        }
        const rpn::item_t* _parg = NULL;
        auto _itarg = args.begin();
        _parg = &(*_itarg);
        if ( _parg->type != rpn::IT_STACK ) {
            return module_manager::ret_error("invalidate argument");
        }

        if ( _pthis->isArray() ) {
            for ( Json::ArrayIndex i = 0; i < _pthis->size(); ++i ) {
                rpn::item_t _this = this_path;
                _this.value.append((int)i);
                rpn::item_t _last = this_path;
                _last.value.append(i == 0 ? 0 : (i - 1));
                stage.push_this(std::move(_this));
                stage.push_last(std::move(_last));

                // Invoke the sub func to compare
                auto _e = stage.invoke_group( _parg->value.asString() );

                stage.pop_last();
                stage.pop_this();

                // toast the error message
                if (_e == E_ASSERT) return module_manager::ret_error(stage.err_string());
                if (_e != E_OK ) return module_manager::ret_error("foreach should not return anything");
            }
            return rpn::item_t{rpn::IT_VOID, Json::Value(Json::nullValue)};
        } else {
            if ( _parg == NULL ) return module_manager::ret_error("missing compare function");
            auto _i = _pthis->begin();
            std::string _nlast = _i.key().asString();
            for (; _i != _pthis->end(); ++_i ) {
                rpn::item_t _this = this_path;
                _this.value.append(_i.key().asString());
                rpn::item_t _last = this_path;
                _last.value.append(_nlast);
                stage.push_this(std::move(_this));
                stage.push_last(std::move(_last));

                // Invoke the sub func to compare
                auto _e = stage.invoke_group( _parg->value.asString() );

                stage.pop_last();
                stage.pop_this();

                // toast the error message
                if (_e == E_ASSERT) return module_manager::ret_error(stage.err_string());
                if (_e != E_OK ) return module_manager::ret_error("foreach should not return anything");
            }
            return rpn::item_t{rpn::IT_VOID, Json::Value(Json::nullValue)};
        }
    }
    rpn::item_t __func_filter(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        if ( args.size() != 1 ) {
            return module_manager::ret_error("missing or invalidate arguments");
        }
        const rpn::item_t* _parg = NULL;
        auto _itarg = args.begin();
        _parg = &(*_itarg);
        if ( _parg->type != rpn::IT_STACK ) {
            return module_manager::ret_error("invalidate argument");
        }

        Json::Value _result(Json::arrayValue);

        if ( _pthis->isArray() ) {
            for ( Json::ArrayIndex i = 0; i < _pthis->size(); ++i ) {
                rpn::item_t _this = this_path;
                _this.value.append((int)i);
                rpn::item_t _last = this_path;
                _last.value.append(i == 0 ? 0 : (i - 1));
                stage.push_this(std::move(_this));
                stage.push_last(std::move(_last));

                // Invoke the sub func to compare
                auto _e = stage.invoke_group( _parg->value.asString() );

                stage.pop_last();
                stage.pop_this();

                // toast the error message
                if (_e == E_ASSERT) return module_manager::ret_error(stage.err_string());
                if (_e == E_OK ) return module_manager::ret_error("missing compare result");
                if ( stage.returnValue().isBool() == false ) {
                    return module_manager::ret_error("invalidate return type");
                }
                // First match
                if ( stage.returnValue().asBool() ) {
                    _result.append((*_pthis)[i]);
                }
            }
        } else {
            if ( _parg == NULL ) return module_manager::ret_error("missing compare function");
            auto _i = _pthis->begin();
            std::string _nlast = _i.key().asString();
            for (; _i != _pthis->end(); ++_i ) {
                rpn::item_t _this = this_path;
                _this.value.append(_i.key().asString());
                rpn::item_t _last = this_path;
                _last.value.append(_nlast);
                stage.push_this(std::move(_this));
                stage.push_last(std::move(_last));

                // Invoke the sub func to compare
                auto _e = stage.invoke_group( _parg->value.asString() );

                stage.pop_last();
                stage.pop_this();

                // toast the error message
                if (_e == E_ASSERT) return module_manager::ret_error(stage.err_string());
                if (_e == E_OK ) return module_manager::ret_error("missing compare result");
                if ( stage.returnValue().isBool() == false ) {
                    return module_manager::ret_error("invalidate return type");
                }
                // First match
                if ( stage.returnValue().asBool() ) {
                    _result.append(*_i);
                }
            }
        }
        return rpn::item_t{rpn::IT_ARRAY, _result};
    }

    bool __func_size_match(const Json::Value& invoker, bool is_root) {
        return !invoker.isNumeric();
    }
    rpn::item_t __func_size(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        if ( args.size() != 0 ) {
            return module_manager::ret_error("too many arguments");
        }

        if ( _pthis->isString() ) return rpn::item_t{rpn::IT_NUMBER, (double)_pthis->asString().size()};
        if ( _pthis->isNull() ) return rpn::item_t{rpn::IT_NUMBER, (double)0};
        return rpn::item_t{rpn::IT_NUMBER, (double)_pthis->size()};
    }
    rpn::item_t __func_stdin(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        if ( args.size() != 0 ) {
            return module_manager::ret_error("too many arguments");
        }

        std::string _line;
        pe::co::waiting_signals _sig = pe::co::no_signal;
        fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK );
        do {
            _sig = pe::co::this_task::wait_fd_for_event(
                STDIN_FILENO, pe::co::event_read, std::chrono::seconds(1)
            );
            if ( _sig == pe::co::bad_signal ) break;
            if ( _sig == pe::co::receive_signal ) {
                std::getline(std::cin, _line);
                return rpn::item_t{rpn::IT_STRING, Json::Value(_line)};
            }
        } while ( true );
        return module_manager::ret_error("input interrpted");
    }
    rpn::item_t __func_stdout(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        if ( args.size() == 0 ) {
            std::cout << stage.rootValue().toStyledString();
        } else {
            for ( auto& i : args ) {
                if ( i.type == rpn::IT_PATH ) {
                    Json::Value* _node = stage.get_node(i.value);
                    if ( _node == NULL ) {
                        std::cout << "null" << std::endl;
                    } else {
                        std::cout << *_node << std::endl;
                    }
                } else {
                    std::cout << i.value << std::endl;
                }
            }
        }
        return rpn::item_t{rpn::IT_VOID, Json::Value(Json::nullValue)};
    }
    rpn::item_t __func_stderr(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        if ( args.size() == 0 ) {
            std::cerr << stage.rootValue().toStyledString();
        }
        for ( auto& i : args ) {
            if ( i.type == rpn::IT_PATH ) {
                Json::Value* _node = stage.get_node(i.value);
                if ( _node == NULL ) {
                    std::cerr << "null" << std::endl;
                } else {
                    std::cerr << *_node << std::endl;
                }
            } else {
                std::cerr << i.value << std::endl;
            }
        }
        return rpn::item_t{rpn::IT_VOID, Json::Value(Json::nullValue)};
    }
    rpn::item_t __func_job(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        if ( args.size() != 1 ) {
            return module_manager::ret_error("missing or invalidate job argument");
        }
        const rpn::item_t* _parg = NULL;
        auto _itarg = args.begin();
        _parg = &(*_itarg);
        if ( _parg->type != rpn::IT_STACK ) {
            return module_manager::ret_error("invalidate argument");
        }
        Json::Value _jobValue(Json::objectValue);
        _jobValue["__stack"] = _parg->value;    // Copy the stack information
        _jobValue["__type"] = "coas.job";
        return rpn::item_t{rpn::IT_OBJECT, _jobValue};
    }

    rpn::item_t __func_invoke(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        if ( args.size() != 1 ) {
            return module_manager::ret_error("missing or invalidate job argument");
        }
        const rpn::item_t* _parg = NULL;
        auto _itarg = args.begin();
        _parg = &(*_itarg);
        if ( _parg->type != rpn::IT_STRING && _parg->type != rpn::IT_PATH ) {
            return module_manager::ret_error("invalidate argument");
        }
        std::string _target;
        if ( _parg->type == rpn::IT_PATH ) {
            Json::Value* _pname = stage.get_node(_parg->value);
            if ( _pname == NULL || !_pname->isString() ) {
                return module_manager::ret_error("invalidate invoke target");
            }
            _target = _pname->asString();
        } else {
            _target = _parg->value.asString();
        }
        auto _e = stage.invoke_group(_target);
        if ( _e == E_ASSERT ) return module_manager::ret_error(stage.err_string());
        if ( _e == E_RETURN ) return stage.resultItem();
        return rpn::item_t{rpn::IT_VOID, Json::Value(Json::nullValue)};
    }

    rpn::item_t __func_do(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        if ( args.size() != 0 ) {
            return module_manager::ret_error("too many arguments in do");
        }
        Json::Value* _pthis = stage.get_node(this_path.value);
        auto _e = stage.invoke_group((*_pthis)["__stack"].asString());
        if ( _e == E_ASSERT ) return module_manager::ret_error(stage.err_string());
        if ( _e == E_RETURN ) return stage.resultItem();
        return rpn::item_t{rpn::IT_VOID, Json::Value(Json::nullValue)};
    }

    rpn::item_t __func_condition(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        if ( args.size() != 2 ) {
            return module_manager::ret_error("missing condition argument");
        }
        if ( (
                args.begin()->type != rpn::IT_OBJECT && 
                args.begin()->type != rpn::IT_NULL
            ) || args.rbegin()->type != rpn::IT_OBJECT 
        ) {
            return module_manager::ret_error("invalidate arguments in condition");
        }
        auto& _expr = args.begin()->value;
        auto& _job = args.rbegin()->value;
        if ( !_expr.isNull() ) {
            if ( 
                !_expr.isMember("__stack") || 
                !_expr.isMember("__type") || 
                _expr["__type"].asString() != "coas.job" 
            ) {
                return module_manager::ret_error("invalidate expr");
            }
        }
        if ( 
            !_job.isMember("__stack") || 
            !_job.isMember("__type") || 
            _job["__type"].asString() != "coas.job" 
        ) {
            return module_manager::ret_error("invalidate job");
        }
        Json::Value _condValue(Json::objectValue);
        _condValue["__expr"] = _expr;
        _condValue["__job"] = _job;
        _condValue["__type"] = "coas.cond";
        return rpn::item_t{rpn::IT_OBJECT, _condValue};
    }
    rpn::item_t __func_check(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        if ( args.size() != 0 ) {
            return module_manager::ret_error("too many arguments in check");
        }
        Json::Value* _pthis = stage.get_node(this_path.value);
        if ( _pthis->size() == 0 ) {
            return module_manager::ret_error("need at least one condition to check");
        }
        for ( Json::ArrayIndex i = 0; i < _pthis->size(); ++i ) {
            Json::Value& _cond = (*_pthis)[i];
            if ( !_cond.isMember("__type") ) {
                return module_manager::ret_error("invalidate condition");
            }
            if ( _cond["__type"].asString() != "coas.cond" ) {
                return module_manager::ret_error("invalidate condition type");
            }
            if ( !_cond.isMember("__expr") ) {
                return module_manager::ret_error("missing expr in condition");
            }
            if ( !_cond.isMember("__job") ) {
                return module_manager::ret_error("missing job in condition");
            }
            bool _expr_result = false;
            if ( _cond["__expr"].isNull() ) {
                if ( i != (_pthis->size() - 1) ) {
                    return module_manager::ret_error("null expr can only be the last case");
                }
                _expr_result = true;
            } else {
                Json::Value& _expr = _cond["__expr"];
                if ( 
                    !_expr.isMember("__stack") || 
                    !_expr.isMember("__type") || 
                    _expr["__type"].asString() != "coas.job" 
                ) {
                    return module_manager::ret_error("invalidate expr");
                }
                auto _e = stage.invoke_group(_expr["__stack"].asString());
                if ( _e == E_ASSERT ) return module_manager::ret_error(stage.err_string());
                if ( _e == E_OK ) return module_manager::ret_error("expr with no boolean return");
                if ( !stage.returnValue().isBool() ) {
                    return module_manager::ret_error("exrp's return value is not a boolean");
                }
                _expr_result = stage.returnValue().asBool();
            }
            // Skip next if exrp check failed
            if ( _expr_result == false ) continue;

            Json::Value& _job = _cond["__job"];
            if ( 
                !_job.isMember("__stack") || 
                !_job.isMember("__type") || 
                _job["__type"].asString() != "coas.job" 
            ) {
                return module_manager::ret_error("invalidate job");
            }
            auto _e = stage.invoke_group(_job["__stack"].asString());
            if ( _e == E_ASSERT ) return module_manager::ret_error(stage.err_string());
            if ( _e == E_RETURN ) return module_manager::ret_error("invalidate return in condition's job");
            break;  // Now we match the condition.
        }
        return rpn::item_t{rpn::IT_VOID, Json::Value(Json::nullValue)};
    }
    rpn::item_t __func_loop(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        if ( args.size() != 3 ) {
            return module_manager::ret_error("missing loop argument");
        }
        auto _it = args.begin();
        if ( 
            _it->type != rpn::IT_BOOL && 
            _it->type != rpn::IT_OBJECT
        ) {
            return module_manager::ret_error("invalidate expr in loop");
        }
        const Json::Value& _expr = _it->value;
        ++_it;
        if ( _it->type != rpn::IT_OBJECT ) {
            return module_manager::ret_error("invalidate job in loop");
        }
        const Json::Value& _job = _it->value;
        ++_it;
        if ( _it->type != rpn::IT_OBJECT && _it->type != rpn::IT_NULL ) {
            return module_manager::ret_error("invalidate after in loop");
        }
        const Json::Value& _after = _it->value;

        if ( !_expr.isBool() ) {
            if ( 
                !_expr.isMember("__stack") || 
                !_expr.isMember("__type") || 
                _expr["__type"].asString() != "coas.job" 
            ) {
                return module_manager::ret_error("invalidate expr");
            }
        }
        if ( 
            !_job.isMember("__stack") || 
            !_job.isMember("__type") || 
            _job["__type"].asString() != "coas.job" 
        ) {
            return module_manager::ret_error("invalidate job");
        }
        if ( !_after.isNull() ) {
            if ( 
                !_after.isMember("__stack") || 
                !_after.isMember("__type") || 
                _after["__type"].asString() != "coas.job" 
            ) {
                return module_manager::ret_error("invalidate after");                
            }
        }

        while ( true ) {
            bool _condition = false;
            if ( _expr.isBool() ) _condition = _expr.asBool();
            else {
                auto _e = stage.invoke_group(_expr["__stack"].asString());
                if ( _e == E_ASSERT ) return module_manager::ret_error(stage.err_string());
                if ( _e == E_OK ) return module_manager::ret_error("expr must has a return value");
                if ( !stage.returnValue().isBool() ) {
                    return module_manager::ret_error("expr must return a boolean value");
                }
                _condition = stage.returnValue().asBool();
            }
            if ( !_condition ) break;

            // Do job
            auto _ej = stage.invoke_group(_job["__stack"].asString());
            if ( _ej == E_ASSERT ) return module_manager::ret_error(stage.err_string());
            if ( _ej == E_RETURN ) return module_manager::ret_error("invalidate return in loop's job");

            // Do after
            if ( _after.isNull() ) continue;
            auto _ea = stage.invoke_group(_after["__stack"].asString());
            if ( _ea == E_ASSERT ) return module_manager::ret_error(stage.err_string());
            if ( _ea == E_RETURN ) return module_manager::ret_error("invalidate return in loop's after");
        }
        return rpn::item_t{rpn::IT_VOID, Json::Value(Json::nullValue)};
    }

    rpn::item_t __func_httpreq(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        if ( args.size() != 1 ) {
            return module_manager::ret_error("missing url");
        }
        auto _it = args.begin();
        std::string _url;
        if ( _it->type == rpn::IT_PATH ) {
            Json::Value* _purl = stage.get_node(_it->value);
            if ( _purl == NULL ) {
                return module_manager::ret_error("null url");
            }
            if ( !_purl->isString() ) {
                return module_manager::ret_error("invalidate url format");
            }
            _url = _purl->asString();
        } else if ( _it->type == rpn::IT_STRING ) {
            _url = _it->value.asString();
        } else {
            return module_manager::ret_error("invalidate url type");
        }
        // Create the url object
        net::http_request _req = net::http_request::fromURL(_url);
        Json::Value _jreq(Json::objectValue);
        _jreq["url"] = _url;
        _jreq["method"] = "GET";
        _jreq["path"] = _req.path();
        Json::Value _jh(Json::objectValue);
        for ( auto i = _req.header.begin(); i != _req.header.end(); ++i ) {
            _jh[i->first] = i->second;
        }
        _jreq["header"] = _jh;
        std::string _body;
        for ( auto i = _req.body.begin(); i != _req.body.end(); ++i ) {
            _body += *i;
        }
        _jreq["body"] = _body;
        _jreq["__type"] = "coas.httpreq";
        return rpn::item_t{rpn::IT_OBJECT, _jreq};
    }    

    rpn::item_t __func_send(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        size_t _timedout = 60000;   // 1 min
        if ( args.size() > 1 ) {
            return module_manager::ret_error("too many arguments in http send");
        }
        if ( args.size() == 1 ) {
            auto _tit = args.begin();
            if ( _tit->type == rpn::IT_NUMBER ) {
                _timedout = _tit->value.asUInt();
            } else if ( _tit->type == rpn::IT_PATH ) {
                Json::Value* _pt = stage.get_node(_tit->value);
                if ( _pt == NULL ) {
                    return module_manager::ret_error("null timedout");
                }
                if ( !_pt->isNumeric() ) {
                    return module_manager::ret_error("invalid type of timedout");
                }
                _timedout = _pt->asUInt();
            } else {
                return module_manager::ret_error("invalidate timedout object");
            }
        }
        Json::Value* _pthis = stage.get_node(this_path.value);
        net::http_request _req = net::http_request::fromURL((*_pthis)["url"].asString());
        if ( _pthis->isMember("method") && (*_pthis)["method"].isString() ) {
            _req.method() = (*_pthis)["method"].asString();
        }
        if ( _pthis->isMember("header") && (*_pthis)["header"].isObject() ) {
            Json::Value &_jh = (*_pthis)["header"];
            for ( auto i = _jh.begin(); i != _jh.end(); ++i ) {
                _req.header[i.key().asString()] = i->asString();
            }
        }
        if ( _pthis->isMember("body") && (*_pthis)["body"].isString() ) {
            std::string _body((*_pthis)["body"].asString());
            if ( _body.size() > 0 ) {
                _req.body.append(std::move(_body));
            }
        }
        net::http_response _resp = net::http_connection::send_request(
            _req, std::chrono::milliseconds(_timedout));
        if ( _resp.status_code == net::CODE_001 ) {
            return module_manager::ret_error("request failed, connection failed or timedout");
        }
        Json::Value _jresp(Json::objectValue);
        _jresp["__type"] = "coas.httpresp";
        _jresp["status_code"] = (int)_resp.status_code;
        _jresp["message"] = _resp.message;
        Json::Value _jh(Json::objectValue);
        for ( auto i = _resp.header.begin(); i != _resp.header.end(); ++i ) {
            _jh[i->first] = i->second;
        }
        _jresp["header"] = _jh;
        std::string _body;
        for ( auto i = _resp.body.begin(); i != _resp.body.end(); ++i ) {
            _body += *i;
        }
        _jresp["body"] = _body;
        return rpn::item_t{rpn::IT_OBJECT, std::move(_jresp)};
    }
    rpn::item_t __func_json(
        costage& stage, 
        const rpn::item_t& this_path, 
        const std::list< rpn::item_t >& args 
    ) {
        if ( args.size() != 0 ) {
            return module_manager::ret_error("too many arguments in json");
        }
        Json::Value* _pthis = stage.get_node(this_path.value);
        Json::Value _node;
        Json::Reader _jreader;
        if ( !_jreader.parse(_pthis->asString(), _node, false) ) {
            return module_manager::ret_error(_jreader.getFormattedErrorMessages());
        }
        return rpn::item_t{rpn::IT_OBJECT, std::move(_node)};
    }
    // C'str
    module_manager::module_manager() {
        // nothing
    }

    // Singleton Instance
    module_manager& module_manager::singleton() {
        static module_manager _mm;
        return _mm;
    }
    module_manager::~module_manager() {
        // Nothing
    }

    // Initialize all default modules
    void module_manager::init_default_modules() {
        module_manager::register_module(module_type{
            "max", &__func_default_match, &__func_max
        });
        module_manager::register_module(module_type{
            "min", &__func_default_match, &__func_min
        });
        module_manager::register_module(module_type{
            "fetch", &__func_default_match, &__func_fetch
        });
        module_manager::register_module(module_type{
            "foreach", &__func_default_match, &__func_foreach
        });
        module_manager::register_module(module_type{
            "filter", &__func_default_match, &__func_filter
        });
        module_manager::register_module(module_type{
            "size", &__func_size_match, &__func_size
        });
        module_manager::register_module(module_type{
            "stdin", &__func_root_match, &__func_stdin
        });
        module_manager::register_module(module_type{
            "stdout", &__func_root_match, &__func_stdout
        });
        module_manager::register_module(module_type{
            "stderr", &__func_root_match, &__func_stderr
        });
        module_manager::register_module(module_type{
            "expr", &__func_root_match, &__func_job
        });
        module_manager::register_module(module_type{
            "job", &__func_root_match, &__func_job
        });
        module_manager::register_module(module_type{
            "after", &__func_root_match, &__func_job
        });
        module_manager::register_module(module_type{
            "invoke", &__func_root_match, &__func_invoke
        });
        module_manager::register_module(module_type{
            "do", &__func_job_match, &__func_do
        });
        module_manager::register_module(module_type{
            "condition", &__func_root_match, &__func_condition
        });
        module_manager::register_module(module_type{
            "check", &__func_array_match, &__func_check
        });
        module_manager::register_module(module_type{
            "loop", &__func_root_match, &__func_loop
        });
        module_manager::register_module(module_type{
            "httpreq", &__func_root_match, &__func_httpreq
        });
        module_manager::register_module(module_type{
            "send", &__func_http_match, &__func_send
        });
        module_manager::register_module(module_type{
            "json", &__func_string_match, &__func_json
        });
    }

    // Register a new module
    void module_manager::register_module( module_type&& m ) {
        ptr_module_type _pmodule = std::make_shared< module_type >(m);
        auto _mit = singleton().module_map_.find(m.name);
        if ( _mit == singleton().module_map_.end() ) {
            ptr_module_list_type _plist = std::make_shared< module_list_t >();
            _plist->push_back(_pmodule);
            singleton().module_map_[m.name] = _plist;
        } else {
            _mit->second->push_back(_pmodule);
        }
    }

    // Search if a module with name has been registered
    ptr_module_type module_manager::search_module( 
        const std::string& name, 
        const Json::Value& invoker,
        bool is_root
    ) {
        auto _mit = singleton().module_map_.find(name);
        if ( _mit == singleton().module_map_.end() ) return nullptr;
        for ( auto& _pmodule : (*_mit->second) ) {
            if ( _pmodule->is_match == nullptr ) return _pmodule;
            if ( _pmodule->is_match(invoker, is_root) ) return _pmodule;
        }
        return nullptr;
    }

    // Utility
    // Create a error object for returning
    rpn::item_t module_manager::ret_error( const std::string& message ) {
        return rpn::item_t{rpn::IT_ERROR, Json::Value(message)};
    }

}

// Push Chen
//