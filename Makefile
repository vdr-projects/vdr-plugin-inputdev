PLUGIN  = inputdev
VERSION = 0.0.1

### The C++ compiler and options:

CXX		?= $(CC)
TAR		?= tar
XZ		?= xz
GPG		?= gpg
GZIP		?= gzip
MSGFMT		?= msgfmt
MSGMERGE	?= msgmerge
XGETTEXT	?= xgettext
PKG_CONFIG	?= pkg-config

INSTALL		?= install
INSTALL_DATA	?= $(INSTALL) -p -m 0644

SYSTEMD_CFLAGS	 = $(shell ${PKG_CONFIG} --cflags libsystemd-daemon || echo "libsystemd_missing")
SYSTEMD_LIBS	 = $(shell ${PKG_CONFIG} --libs libsystemd-daemon || echo "libsystemd_missing")

TAR_FLAGS	 = --owner root --group root --mode a+rX,go-w

AM_CPPFLAGS	 = -DPACKAGE_VERSION=\"${VERSION}\" -DSOCKET_PATH=\"${SOCKET_PATH}\" \
		   -D_GNU_SOURCE -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

WARN_OPTS	 = -Wall -W -Wno-missing-field-initializers -Wextra

AM_CXXFLAGS	+= $(WARN_OPTS)
AM_CFLAGS	+= $(WARN_OPTS)

ifneq ($(USE_SYSTEMD),)
AM_CPPFLAGS	+= -DVDR_USE_SYSTEMD
AM_CXXFLAGS	+= ${SYSTEMD_CFLAGS}
LIBS		+= ${SYSTEMD_LIBS}
endif

plugin_SOURCES = \
	inputdev.cc \
	inputdev.h \
	plugin.cc \

helper_SOURCES = \
	udevhelper.c

extra_SOURCES = \
	COPYING \
	COPYING.gpl-2 \
	COPYING.gpl-3 \
	Makefile \
	README.txt \
	contrib/96-vdrkeymap.rules \
	contrib/hama-mce \
	contrib/tt6400-ir \
	contrib/x10-wti

### The directory environment:

VDRDIR ?= ../../..
LIBDIR ?= ../../lib
TMPDIR ?= /tmp

SOCKET_PATH = /var/run/vdr/inputdev

### Allow user defined options to overwrite defaults:

include $(VDRDIR)/Make.global
-include $(VDRDIR)/Make.config

### The version number of VDR's plugin API (taken from VDR's "config.h"):

APIVERSION = $(shell sed -ne '/define APIVERSION/s/^.*"\(.*\)".*$$/\1/p' $(VDRDIR)/config.h)

### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE).tar

### The object files (add further files here):

_all_sources = $(plugin_SOURCES) $(helper_SOURCES) $(extra_SOURCES)

_objects = \
  $(patsubst %.c,%.o,$(filter %.c,$(plugin_SOURCES))) \
  $(patsubst %.cc,%.o,$(filter %.cc,$(plugin_SOURCES)))

plugin_OBJS = $(call _objects,$(plugin_SOURCES))
helper_OBJS = $(call _objects,$(helper_SOURCES))

OBJS = $(plugin_OBJS) $(helper_OBJS)

### The main target:

all: libvdr-$(PLUGIN).so vdr-inputdev i18n

### Implicit rules:
_buildflags = $(foreach k,CPP $1 LD, $(AM_$kFLAGS) $($kFLAGS) $($kFLAGS_$@))

%.o: %.c
	$(CC) $(call _buildflags,C) -MMD -MP -c $< -o $@

%.o: %.cc
	$(CC) $(call _buildflags,CXX) -MMD -MP -c $< -o $@

%.xz:	%
	@rm -f $@.tmp $@
	$(XZ) -c < $< >$@.tmp
	@mv $@.tmp $@

%.gz:	%
	@rm -f $@.tmp $@
	$(GZIP) -c < $< >$@.tmp
	@mv $@.tmp $@

%.mo: %.po
	$(MSGFMT) -c -o $@ $<


-include $(OBJS:%.o=%.d)

### Internationalization (I18N):

PODIR     = po
LOCALEDIR = $(VDRDIR)/locale
I18Npo    = $(wildcard $(PODIR)/*.po)
I18Nmsgs  = $(addprefix $(LOCALEDIR)/, $(addsuffix /LC_MESSAGES/vdr-$(PLUGIN).mo, $(notdir $(foreach file, $(I18Npo), $(basename $(file))))))
I18Npot   = $(PODIR)/$(PLUGIN).pot

$(I18Npot): $(wildcard *.c *.cc)
	$(XGETTEXT) -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP --package-name=vdr-$(PLUGIN) --package-version=$(VERSION) --msgid-bugs-address='<see README>' -o $@ $^

%.po: $(I18Npot)
	$(MSGMERGE) -U --no-wrap --no-location --backup=none -q $@ $<
	@touch $@

$(I18Nmsgs): $(LOCALEDIR)/%/LC_MESSAGES/vdr-$(PLUGIN).mo: $(PODIR)/%.mo
	$(INSTALL_DATA) -D $< $@

.PHONY: i18n
i18n: $(I18Nmsgs) $(I18Npot)

### Targets:
vdr-inputdev:	$(helper_SOURCES)
	$(CC) $(call _buildflags,C) $^ -o $@

libvdr-$(PLUGIN).so: $(plugin_OBJS)
	$(CXX) $(AM_LDFLAGS) $(LDFLAGS) $(LDFLAGS_$@) -shared -o $@ $^ $(LIBS)
	@cp --remove-destination $@ $(LIBDIR)/$@.$(APIVERSION)

_packages = $(addprefix $(PACKAGE),.xz .gz)

dist:  $(_packages) $(addsuffix .asc,$(_packages))

_tar_transform = --transform='s!^!$(ARCHIVE)/!'

$(PACKAGE):	$(I18Npo) $(_all_sources)
	$(TAR) cf $@ $(TAR_FLAGS) $(_tar_transform) $(sort $^)

%.asc:		%
	$(GPG) --detach-sign --armor --output $@ $<

clean:
	@rm -f $(OBJS) libvdr*.so libvdr*.so.* *.d *.tgz core* *~ $(PODIR)/*.mo $(PODIR)/*.pot
	@rm -f vdr-inputdev
