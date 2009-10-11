#!/usr/bin/python

import sys

class LineReader:
    def __init__(self, inputName):
        self.inputName = inputName
        self.lines = open(inputName).readlines()
        self.line_num = -1

    def get_raw_line(self):
        self.line_num += 1

        if len(self.lines) <= self.line_num:
            return None
            
        retval = self.lines[self.line_num]
        return retval.strip()

    def get_line(self, eofOK = False, skipBlanks = True):
        line = None

        while not line:
            line = self.get_raw_line()

            if line == None:
                if eofOK:
                    return None
                else:
                    self.error("Unexpected EOF")

            if not skipBlanks:
                return line

        return line
    
    def error(self, s):
        sys.stderr.write("%s: line %d: %s\n" % (self.inputName, self.line_num+1, s))
        sys.exit(-1)
    

def check_bit(lr, line, position, char):
    if len(line) <= position or line[position] != char:
        lr.error("Expected column %d to be '%s'" % (position+1, char))
        
def get_bit(lr, line, position, truechar):
    if len(line) <= position:
        return 0

    if line[position] == truechar:
        return 1
    elif line[position] == " ":
        return 0
    else:
        lr.error("Expected column %d to be '%s' or ' '" % (
            position, truechar))

def get_bitmap(lr):
    bitmap = [0] * 8

    l1 = lr.get_line(skipBlanks = False)
    check_bit(lr, l1, 0, ".")
    bitmap[0] = get_bit(lr, l1, 2, "_")
    
    l2 = lr.get_line(skipBlanks = False)
    check_bit(lr, l2, 0, ".")
    bitmap[5] = get_bit(lr, l2, 1, "|")
    bitmap[6] = get_bit(lr, l2, 2, "_")
    bitmap[1] = get_bit(lr, l2, 3, "|")

    l3 = lr.get_line(skipBlanks = False)
    check_bit(lr, l3, 0, ".")
    bitmap[4] = get_bit(lr, l3, 1, "|")
    bitmap[3] = get_bit(lr, l3, 2, "_")
    bitmap[2] = get_bit(lr, l3, 3, "|")
    bitmap[7] = get_bit(lr, l3, 4, ".")

    return bitmap

def main():
    bitmap_map = {}
    
    lr = LineReader(sys.argv[1])

    while True:
        line = lr.get_line(eofOK = True)

        if not line:
            break

        if len(line) != 1:
            lr.error("expected line with single char being defined, got '%s'" % (line))

        char = line[0]

        bitmap = get_bitmap(lr)
        bitmap_map[char] = bitmap

    sys.stdout.write("// Automatically generated.  Do not edit.\n")
    for i in xrange(32, 127):
        char = chr(i)

        if char in bitmap_map:
            bitmap = bitmap_map[char]
            sys.stdout.write("0b")
            for b in [7, 0, 1, 2, 3, 4, 5, 6]:
                sys.stdout.write("%d" % (bitmap[b]))
            sys.stdout.write(",  // '%s' (chr %d)\n" % (char, ord(char)))
        else:
            sys.stdout.write("0b00000000,  // '%s' (chr %d) - MISSING\n" % (char, ord(char)))
            
    

main()
