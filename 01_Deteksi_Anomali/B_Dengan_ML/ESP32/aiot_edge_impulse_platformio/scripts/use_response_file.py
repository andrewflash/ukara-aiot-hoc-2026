#
# The Edge Impulse SDK compiles into ~875 object files. The final link
# command lists every object on a single line, which can exceed the OS
# "Argument list too long" limit (esp. once the Adafruit MPU6050 / Wire
# stack is added on top of the SDK).
#
# Route the long object list through a temporary GNU ld "@response file"
# (TEMPFILE) so it never hits the shell command line.
#
Import("env")

# Switch to a response file well before the OS limit.
env["MAXLINELENGTH"] = 8192

# Put just the (huge) object list into a response file for the link and
# archive commands; the rest of the command stays inline. Guard against
# re-wrapping if this script is evaluated more than once.
if "TEMPFILE" not in env["LINKCOM"]:
    env["LINKCOM"] = env["LINKCOM"].replace("$SOURCES", "${TEMPFILE('$SOURCES')}")
if "TEMPFILE" not in env["ARCOM"]:
    env["ARCOM"] = env["ARCOM"].replace("$SOURCES", "${TEMPFILE('$SOURCES')}")
