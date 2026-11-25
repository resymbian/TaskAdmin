#!/bin/sh
# S60 SDK build scripts date from Perl 5.8 and use constructs removed in 5.22+.
set -eu

TOOLS=${EPOCROOT:?}/epoc32/tools

# Perl 5.22+ rejects defined(%hash); these checks only wanted the hashref.
perl -pi -e 's/defined %\{\$Plat\{(\$\w+)\}\}/defined \$Plat{$1}/g' "$TOOLS/e32plat.pm"
# On Unix the drive-letter regex never matches, leaving $1 undefined.
perl -pi -e 's/my \$MacroHRHLocation_Drive = lc\(\$1\);/my \$MacroHRHLocation_Drive = defined(\$1) ? lc(\$1) : q{};/' "$TOOLS/e32variant.pm"
