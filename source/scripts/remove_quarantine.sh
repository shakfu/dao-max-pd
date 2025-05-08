echo "\
It's better to run codesigned and notarized software in general
for secure software. In the apple ecosystem, this requires the developer
to have an apple developer account which costs 100 USD per year.

If you are building externals yourself, or 
using github actions where you can transparently read the code that
is to be built remotely, you can optionally remove the quarantine
that apple applies to any externally downloaded code which is not
codesigned and notarized (with apple developer credentials).

It does this by the following commandline instructions:

codesign --sign - --timestamp --force externals/*.mxo/**/MacOS/*
/usr/bin/xattr -r -d com.apple.quarantine externals/*.mxo

codesign --sign - --timestamp --force externals/pd/**/*.pd_darwin
/usr/bin/xattr -r -d com.apple.quarantine externals/pd/**/*.pd_darwin

What does codesign --sign - do?

This is called an ad-hoc codesign. 
see: https://developer.apple.com/documentation/security/seccodesignatureflags/adhoc
see: https://github.com/Cycling74/min-devkit

What does xattr -r -d com.apple.quarantine do?

That command causes the OS to skip the malware check for the file specified;
it normally gets removed if it doesn't find any in it. Running it should only
be done if the computer falsely detects some and it's from a trusted source.

It's up to you to decide if you have built it from a trusted source, so the
the commands are commented out by default. Uncomment at your own risk.
"
# codesign --sign - --timestamp --force externals/*.mxo/**/MacOS/*
# /usr/bin/xattr -r -d com.apple.quarantine externals/*.mxo
# codesign --sign - --timestamp --force externals/pd/**/*.pd_darwin
# /usr/bin/xattr -r -d com.apple.quarantine externals/pd/**/*.pd_darwin