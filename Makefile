# The real makefile is very GNU make specific, so here's a POSIX makefile
# to forward the basic requests.

ALLTARG=server clean install rebuild vps zip

default:
	+@gmake

$(ALLTARG):
	+@gmake $@
