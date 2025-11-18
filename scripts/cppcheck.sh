#!/bin/sh

cppcheck -j 8 --xml --xml-version=2 --project=build/compile_commands.json --project-configuration=Release --enable=warning,information,portability,performance --addon=cert --addon=threadsafety --addon=scripts/misra.json --suppressions-list=scripts/cppcheck.txt 2> cppcheck.xml
