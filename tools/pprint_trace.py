#!/usr/bin/python
# encoding: utf-8
'''
Pretty-prints a backtrace to demangling all C++ function names.
'''

import sys
import argparse
import re
import subprocess

# regular expression that matches a line in the backtrace
REGEX = re.compile(r'^(=>|  )#[0-9]+ +[0-9a-f]+ in ([^+]+)\+0x.*')


def demangle(symbol):
    out = subprocess.check_output(('c++filt', symbol))
    if out:
        return out.decode('utf-8').strip()
    return symbol


def demangleFile(infile, outfile):
    for line in infile:
        m = REGEX.match(line)
        if m:
            symbol = m.group(2)
            newSymbol = demangle(symbol)
            line = line.replace(symbol, newSymbol)
        outfile.write(line)


def main(desc):
    '''Command line options.'''

    # Setup argument parser
    parser = argparse.ArgumentParser(description=desc)
    parser.add_argument('-i', '--infile', dest='infile', help='input file (default: stdin)')
    parser.add_argument('-o', '--oufile', dest='outfile', help='output file (default: stdout)')

    # Process arguments
    options = parser.parse_args()

    infile = sys.stdin
    if options.infile and options.infile != '-':
        infile = open(options.infile, 'r')
    outfile = sys.stdout
    if options.outfile:
        outfile = open(options.outfile, 'a')

    demangleFile(infile, outfile)
    outfile.flush()

    return 0


if __name__ == "__main__":
    sys.exit(main(__doc__))
