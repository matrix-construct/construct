#!/usr/bin/python

import os
import sys

if len(sys.argv) < 2:
	print "Usage: %s <sqlite3 dump file>" % sys.argv[0]
	sys.exit(1)

file = sys.argv[1];
for line in open(file):
	if not line.startswith("INSERT"):
		continue

	line = line.split(" ", 3)
	line = line[3].rstrip(");\n")
	line = line.lstrip("VALUES(")
	line = line.split(",", 2)
	line = line[2].split("',", 1)
	line = line[1].strip("\\'")
	sys.stdout.write(line)

sys.exit(0)
