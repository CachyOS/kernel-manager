#! /usr/bin/env python3
"""// Copyright (C) 2022 Vladislav Nepogodin
//
// This file is part of CachyOS kernel manager.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
"""

from sys import argv
import os
import json

if len(argv) != 3:
    print('Wrong number of arguments! Please specify header directory.')
    exit(1)

HEADER_ROOT = argv[1]
SOURCE_ROOT = argv[2]
SCRIPT_MTIME = os.stat(__file__).st_mtime

def needs_run():
    try:
        for filename in ['compile_options.hpp']:
            st = os.stat(HEADER_ROOT + '/' + filename)
            if st.st_mtime < SCRIPT_MTIME:
                return True
    except FileNotFoundError:
        return True
    return False

def main():
    if needs_run():
        header_file = HEADER_ROOT + '/compile_options.hpp'
        header = open(header_file, 'w', encoding='utf-8', newline='\u000A')

        compile_options = {}
        with open(SOURCE_ROOT + '/src/compile_options.json', 'r') as f:
            compile_options = json.load(f)

        header.write(globals()['__doc__'])
        header.write("""#pragma once

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#include <frozen/unordered_map.h>
#include <frozen/string.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <string_view>

namespace detail {

""")

        for map_name in compile_options.keys():
            values_count = len(compile_options[map_name])
            header.write('constexpr frozen::unordered_map<frozen::string, std::string_view, {}> {} '.format(values_count, map_name))
            header.write('{\n')
            map_obj = compile_options[map_name]
            for key in map_obj:
                header.write('    {')
                header.write('"{}", "{}"'.format(key, map_obj[key]))
                header.write('},\n')
            header.write('\n};\n')

        header.write("\n} // namespace detail\n")
        header.close()
    else:
        print("Output files up-to-date.")

main()
