PLUGIN  = inputdev
VERSION = 0.0.1

### The C++ compiler and options:

PKG_CONFIG = pkg-config
SYSTEMD_CFLAGS = $(shell ${PKG_CONFIG} --cflags libsystemd-daemon || echo "libsystemd_missing")
SYSTEMD_LIBS   = $(shell ${PKG_CONFIG} --libs libsystemd-daemon || echo "libsystemd_missing")

SOCKET_PATH = /var/run/vdr/inputdev

AM_CXXFLAGS = -DPACKAGE_VERSION=\"${VERSION}\" -DSOCKET_PATH=\"${SOCKET_PATH}\" ${SYSTEMD_CFLAGS}
AM_CXXFLAGS +=  -D_FORTIFY_SOURCE=2 -Wall -Werror

LIBS	    = ${SYSTEMD_LIBS}

CXX      ?= g++
CXXFLAGS ?= -g -O3 -Wall -Werror=overloaded-virtual -Wno-parentheses

### The directory environment:

VDRDIR ?= ../../..
LIBDIR ?= ../../lib
TMPDIR ?= /tmp

### Allow user defined options to overwrite defaults:

include $(VDRDIR)/Make.global
-include $(VDRDIR)/Make.config

### The version number of VDR's plugin API (taken from VDR's "config.h"):

APIVERSION = $(shell sed -ne '/define APIVERSION/s/^.*"\(.*\)".*$$/\1/p' $(VDRDIR)/config.h)

### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### Includes and Defines (add further entries here):

INCLUDES += -I$(VDRDIR)/include

DEFINES += -D_GNU_SOURCE -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

### The object files (add further files here):

OBJS = plugin.o inputdev.o

### The main target:

all: libvdr-$(PLUGIN).so i18n

### Implicit rules:

%.o: %.c
	$(CXX) $(AM_CXXFLAGS) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) $<

### Dependencies:

MAKEDEP = $(CXX) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

-include $(DEPFILE)

### Internationalization (I18N):

PODIR     = po
LOCALEDIR = $(VDRDIR)/locale
I18Npo    = $(wildcard $(PODIR)/*.po)
I18Nmsgs  = $(addprefix $(LOCALEDIR)/, $(addsuffix /LC_MESSAGES/vdr-$(PLUGIN).mo, $(notdir $(foreach file, $(I18Npo), $(basename $(file))))))
I18Npot   = $(PODIR)/$(PLUGIN).pot

%.mo: %.po
	msgfmt -c -o $@ $<

$(I18Npot): $(wildcard *.c)
	xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP --package-name=vdr-$(PLUGIN) --package-version=$(VERSION) --msgid-bugs-address='<see README>' -o $@ $^

%.po: $(I18Npot)
	msgmerge -U --no-wrap --no-location --backup=none -q $@ $<
	@touch $@

$(I18Nmsgs): $(LOCALEDIR)/%/LC_MESSAGES/vdr-$(PLUGIN).mo: $(PODIR)/%.mo
	@mkdir -p $(dir $@)
	cp $< $@

.PHONY: i18n
i18n: $(I18Nmsgs) $(I18Npot)

### Targets:

libvdr-$(PLUGIN).so: $(OBJS)
	$(CXX) $(LDFLAGS) -shared $(OBJS) $(LIBS) -o $@
	@cp --remove-destination $@ $(LIBDIR)/$@.$(APIVERSION)

dist: $(I18Npo) clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean:
	@-rm -f $(OBJS) $(DEPFILE) *.so *.tgz core* *~ $(PODIR)/*.mo $(PODIR)/*.pot
