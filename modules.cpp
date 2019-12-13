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

#include "modules.h"
#include "costage.h"

namespace coas {

    rpn::item_t __func_max( costage& stage, const rpn::item_t& this_path, std::list< rpn::item_t > args ) {
        Json::Value* _pthis = stage.get_node(this_path.value);
        if ( _pthis == NULL ) {
            return rpn::item_t{rpn::IT_ERROR, Json::Value("this BAD_ACCESS")};
        }
        Json::Value* _pmax = NULL;
        if ( _pthis->isArray() ) {
            _pmax = &(*_pthis)[0];
            for ( Json::ArrayIndex i = 1; i < _pthis->size(); ++i ) {
                if ( *_pmax < (*_pthis)[i] ) _pmax = &(*_pthis)[i];
            }
        }
        return rpn::item_t{rpn::IT_NUMBER, *_pmax};
    }

}

// Push Chen
//