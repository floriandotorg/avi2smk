# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands
- Build: `cmake -DCMAKE_BUILD_TYPE=Release . && make`
- Build with tests: `cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON . && make`
- Run tests: `ctest`
- Run single test: `./test_name` (where test_name is one of: test_huffman, test_bitstream, test_palette)

## Code Style Guidelines
- C++20 standard
- Namespaces: Use for major components (avi, smk)
- Naming: snake_case for functions/variables, camelCase for classes
- Member variables: Prefix with underscore (_name)
- Headers: System headers first (alphabetically), then project headers
- Error handling: Use exceptions with std::runtime_error and std::format
- Class structure: Public methods first, then private
- Indentation: 4 spaces
- File organization: Split implementation between .hpp and .cpp
- Prefer modern C++ features (std::span, std::ranges, etc.)
- No external dependencies

Always maintain the existing code style when making changes.

## Rules

- write code concise and without comments
- always use brackets for if statements (or newline in python)
- don't insert unnecessary try/catches
- use n as counter in for loops
- use prefix increment when possible (++n)
- do not add or commit anything with git
