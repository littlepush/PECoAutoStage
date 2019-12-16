# PECoAutoStage
This is an automate API test tool based on PECo Framework, a.k.a. `coas`.

We consider a single test case as a stage in `coas`, a stage is a serious of codes to process a root JSON node.

We use `$.assert` or `$.return` to determine if a stage is passed or failed.

## Install

### Dependence

* [`PEUtils`](https://github.com/littlepush/PEUtils)
* [`PECoTask`](https://github.com/littlepush/PECoTask)
* [`PECoNet`](https://github.com/littlepush/PECoNet)

### Build and Install

```
make && sudo make install
```

The bin file will be found under `/usr/local/bin`

#### Make a debug version

```
make -f Makefile.debug && sudo make -f Makefile.debug install
```

This will create a debug version of `coas` with filename `coasd`

### Usa a script

```
curl -o- -SL "https://raw.githubusercontent.com/littlepush/PECoAutoStage/master/install_script.sh" | bash
```



## costage file

A file with extension ".costage" is a costage file.

We can run a single costage file or a folder contains several costage files.

#### _begin.costage

A specifial costage file named `_begin.costage` in the folder will be run before any other stages.

the root JSON node of the begin stage will be copy to all other stage

#### _final.costage

A specifial costage file named `_final.costage` in the folder will be run after all stages stop.

The root JSON node of `_final.costage` will be like the following:

```
{
  "result": {
    "stage_file": {
      "stage": bool,
      "time": double
    },
    ...
  }
}
```

You can write your own code in `_final.costage` to make a report.


## Basic Code Syntax

```
'Left Path' = 'Right Value'
```

### Path

In `coas`, we have two kinds of path:

1. Path begin with `$` is a root path, it refers to the root JSON node of the stage.
2. Path begin with `@` is a local path, it refers to current call stack's JSON node, and will be destoried after the function call.

The left side of a `=` can only be a path.

#### new path

If a subpath of a node does not existed, it will create it and be initialized as null

#### array index

If a subnode is an array item, use `[]` direct after the node name will refer to the item at specified index.

#### complex path

```
$.(expr).($.other_string_value_path)
```

Use a pair of `()` after a `.` means the following path name will be calculated by the expression.

## Right Value

A right value can be a number, a string, reference to another path, or return value of an expression.

#### Number

```
$.myNumber1 = 0
$.myNumber2 = 1.2
$.myNumber3 = -1.5
```

#### String

```
$.myString = "String Here"
```

#### Reference Path

```
$.myString = "Hello CoAS"
$.myAnotherString = $.myString
```

#### Expression

```
$.myValue = (1 + 2) * 3 / 4.5
```

### Sub Stack

```
$.void = $.myarray.foreach({
	...
})
```

the code between a pair of `{}` will be consider as a sub code stack.

### Function

```
.func function_name
...
```

In a costage file, the line next to `.func ...` will be parsed as a function, the function's code will be end until we meet another function or `.stage` part.

### Include

```
.include file_path
```

Import a module file into current stage. The included file can only contains functions.



## Built-in Keypath

### void: `$.void`

`void` must be the left side of the code. Whatever the right value is, it will be ignored.

Usually we use `$.void` when we invoke a function without any return value.

### return: `$.return`

`return` must be the left side of the code. Any right value can be set to `$.return`, and any code after `$.return` will be ignored. the function or stage will exit immediately.

If `$.return` is in stage part, then must be a boolean value, and the stage's state is depended on the value of return.

### assert: `$.assert`

`assert` must be the left side of the code, and the right value must be a boolean value.

if `assert` is equal to `false`, the stage will exit immediately and the state of the stage will be `failed`

### this: `$.this`

`this` is used to refer to the invoker of current function.

### last: `$.last`

`last` is used to refer to the last invoker of a loop.

```
$.void = $.myarray.foreach({
	$.my_another_array = $.this + $.last
})
```



## Built-in Syntax Module

### max

Get the max value of an array or an object, can set a compare stack to replace the default compare logic

```
$.a[0] = 1
$.a[1] = 2
$.a[2] = 3
$.result = $.a.max()
$.result2 = $.a.max({
	$.return = $.this > $.last
})
```

### min

Get the max value of an array or an object, can set a compare stack to replace the default compare logic

```
$.a[0] = 1
$.a[1] = 2
$.a[2] = 3
$.result = $.a.min()
```

### fetch

Get the first item match the give rule of an array or an object.

```
$.case_array[0] = 1
$.case_array[1] = 2
$.case_array[2] = 3
$.match_value = $.case_array.fetch({
    $.return = $.this > 2
})
```

### foreach

Run a loop to the array or object, return void

```
$.case_array[0] = 1
$.case_array[1] = 2
$.case_array[2] = 3

$.void = $.case_array.foreach({
    $.this = $.this + 1
})
```



### filter

Return an array of objects that match the filter rule

```
$.case_array[0] = 1
$.case_array[1] = 2
$.case_array[2] = 3
$.filter_result = $.case_array.filter({
    $.return = ($.this % 2) == 0 
})
```



### size

Return the length of array, key count of object, or length of string

```
$.array_size = $.case_array.size();
$.string_size = $.object.key1.size();
```



### stdin

Get input from STDIN, this is a root only syntax

```
$.myInput = $.stdin()
```



### stdout

Output any object to the STDOUT, if no argument is set, output the root value

```
$.void = $.stdout("my words")
$.void = $.stdou()
```



### stderr

Output any object to the STDERR

```
$.void = $.stderr("my words")
$.void = $.stderr()
```

### invoke

Invoke a function defined in current stage

```
.func do_something
$.return = true

.stage
$.result = $.invoke("do_something")
```



### job

Wrap a serious code and create a function, the function name will be stored in the sub path `__stack`.

```
$.myJob = $.job({
	$.myValue = 1
	$.myValue2 = $.myValue + 2
})

[RootValue]
{
	"myJob": {
		"__stack": "function address",
		"__type": "coas.job"
	}
}
```



### do

Invoke a job stored in the root value

```
$.myJob = $.job({
	$.myValue = 1
	$.myValue2 = $.myValue + 2
})
$.void = $.myJob.do()

[RootValue]
{
	"myJob": {
		"__stack": "function address",
		"__type": "coas.job"
	},
	"myValue": 1
	"myValue2": 3
}
```



### condition, expr, check

We use these four syntax to create a `if, else if, else` block. 

Use `$.condition` to create a check list, and store to an array. 

A condition contains an expression and a job. An expression must return a boolean value.

Use `$.case.check()` to test all case in the list, if any case matches, invoke its job.

The last case's expression can be a `null`, and will work as `if ... else ...`

```
$.void = $.stdout("please input a word: ")
$.myValue = "1"
$.case[0] = $.condition( $.expr({
    $.return = $.myValue == "1"
}), $.job({
    $.case_job = "job 1 == 0"
}) )
$.case[1] = $.condition( null, $.job({
    $.case_job = "job 1 == 1"
}))
$.void = $.case.check();
```



### loop, after

A loop contains 3 arguments: `expr`, `job`, `after`

In each loop, check `expr` first, if return false, break the loop

Then do the job.

If `after` is not null, invoke after the job, then go back to check `expr`

```
.func do_in_loop
$.void = $.stdout($.loop_index)

.stage
$.loop_index = 0
$.void = $.loop($.expr({
    $.return = $.loop_index < 10
}), $.job({
    $.void = $.invoke("do_in_loop")
}), $.after({
    $.loop_index = $.loop_index + 1
}))
```



### httpreq, send

Create an HTTP request object with given URL

```
$.github = $.httpreq("https://github.com")
```

The http request object contains the following items:

```
{
	"url": "req url",
	"path": "path in the url",
	"method": "http method",
	"header": {
		"key": "value"
	},
	"body": "body string",
	"__type": "coas.httpreq"
}
```

You can change `method`, or add any specifial `header` or set the `body` to post.

Then, invoke `send` function on the request object

```
$.github_result = $.github.send()
```

The `gitHub_result` will be an HTTP resposne object, as following:

```
{
	"__type": "coas.httpresp",
	"status_code": code,
	"message": "http message",
	"header": {
		"key": "value"
	},
	"body": "body string"
}
```

Then you can do anything you want use this response object

```
// Check if http response success
$.assert = $.github_result.status_code == 200
```



## Demo

All demo stage used to test the application is under folder `demo`

```
[Run]
coas ./demo

[Output]
"please input a word: "
0
1
2
3
4
5
6
7
8
9
{
	"__type" : "coas.httpreq",
	"body" : "",
	"header" :
	{
		"Host" : "github.com"
	},
	"method" : "GET",
	"path" : "/",
	"url" : "https://github.com"
}
./demo/case1.costage: assert failed: $.assert = $.result == $.boolean
[FAILED]: ./demo/case1.costage, 6.96ms
[ PASS ]: ./demo/case2.costage, 7.57ms
[ PASS ]: ./demo/case3.costage, 1.58ms
[ PASS ]: ./demo/case4.costage, 1.8ms
[ PASS ]: ./demo/case5.costage, 3.06s
All Stage: 5
Passed: 4
Failed: 1
Available: 80%
Running time: 3.06s
All stage time: 3.07s
Max stage time: [./demo/case5.costage]: 3.06s
Min stage time: [./demo/case3.costage]: 1.58ms
```



## TODO:

* Complete the Document
* add `&&`, `||` operators
* add `&`, `|`, `&=`, `|=`, `<<`, `<<=`, `>>`, `>>=` operators
* add `append`,`insert`,`pop`,`push`,`delete` as built-in keyword
* add Python module support
* add Redis connection support
* add File support



## Contributing

If you want to contribute to a project and make it better, your help is very welcome. Contributing is also a great way to learn more about social coding on Github, new technologies and and their ecosystems and how to make constructive, helpful bug reports, feature requests and the noblest of all contributions: a good, clean pull request.

### How to make a clean pull request

Look for a project's contribution instructions. If there are any, follow them.

- Create a personal fork of the project on Github.
- Clone the fork on your local machine. Your remote repo on Github is called `origin`.
- Add the original repository as a remote called `upstream`.
- If you created your fork a while ago be sure to pull upstream changes into your local repository.
- Create a new branch to work on! Branch from `develop` if it exists, else from `master`.
- Implement/fix your feature, comment your code.
- Follow the code style of the project, including indentation.
- If the project has tests run them!
- Write or adapt tests as needed.
- Add or change the documentation as needed.
- Squash your commits into a single commit with git's [interactive rebase](https://help.github.com/articles/interactive-rebase). Create a new branch if necessary.
- Push your branch to your fork on Github, the remote `origin`.
- From your fork open a pull request in the correct branch. Target the project's `develop` branch if there is one, else go for `master`!
- ...
- Once the pull request is approved and merged you can pull the changes from `upstream` to your local repo and delete
your extra branch(es).

And last but not least: Always write your commit messages in the present tense. Your commit message should describe what the commit, when applied, does to the code – not what you did to the code.



## License

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