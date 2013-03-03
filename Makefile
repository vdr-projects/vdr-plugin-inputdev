PLUGIN  = inputdev
VERSION = 0.0.1

plugin_SOURCES = \
	inputdev.cc \
	inputdev.h \
	plugin.cc \
	modmap.cc \
	modmap.h

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

LINGUAS	= de

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
GPERF		?= gperf
SED		?= sed

INSTALL		?= install
INSTALL_DATA	?= $(INSTALL) -p -m 0644
INSTALL_PLUGIN	?= $(INSTALL) -p -m 0755
INSTALL_BIN	?= $(INSTALL) -p -m 0755
MKDIR_P		?= $(INSTALL) -d -m 0755
TOUCH_C		?= touch -c

GPERF_FLAGS	 = -L C++ --readonly-tables --enum --ignore-case

SYSTEMD_CFLAGS	 = $(shell ${PKG_CONFIG} --cflags libsystemd-daemon || echo "libsystemd_missing")
SYSTEMD_LIBS	 = $(shell ${PKG_CONFIG} --libs libsystemd-daemon || echo "libsystemd_missing")

VDR_CFLAGS	 = $(shell ${PKG_CONFIG} --variable=cflags vdr)
VDR_CXXFLAGS	 = $(shell ${PKG_CONFIG} --variable=cxxflags vdr)


TAR_FLAGS	 = --owner root --group root --mode a+rX,go-w

AM_CPPFLAGS	 = -DPACKAGE_VERSION=\"${VERSION}\" -DSOCKET_PATH=\"${SOCKET_PATH}\" \
		   -D_GNU_SOURCE -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

AM_MSGMERGEFLAGS =  -U --force-po --no-wrap --no-location --backup=none -q

AM_XGETTEXTFLAGS = -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP \
		   --package-name=vdr-$(PLUGIN) --package-version=$(VERSION) \
		   --msgid-bugs-address='<see README>'

WARN_OPTS	 = -Wall -W -Wno-missing-field-initializers -Wextra

AM_CXXFLAGS	+= $(VDR_CXXFLAGS) $(WARN_OPTS)
AM_CFLAGS	+= $(VDR_CFLAGS) $(WARN_OPTS)

ifneq ($(USE_SYSTEMD),)
  AM_CPPFLAGS	+= -DVDR_USE_SYSTEMD
  AM_CXXFLAGS	+= ${SYSTEMD_CFLAGS}
  LIBS		+= ${SYSTEMD_LIBS}
endif

prefix		 = /usr/local
datadir		 = $(prefix)/share
plugindir	 = $(patsubst $(DESTDIR)/%,/%,$(PLUGINLIBDIR))
udevdir		 = $(prefix)/lib/udev
localedir	 = $(datadir)/locale

vdr_PLUGINS	 = libvdr-$(PLUGIN).so.$(APIVERSION)

### The directory environment:

VDRDIR ?= ../../..
LIBDIR ?= ../../lib
TMPDIR ?= /tmp

SOCKET_PATH = /var/run/vdr/inputdev

### Allow user defined options to overwrite defaults:

-include $(VDRDIR)/Make.global
-include $(VDRDIR)/Make.config

### The version number of VDR's plugin API (taken from VDR's "config.h"):

APIVERSION = $(shell ${PKG_CONFIG} --variable=apiversion vdr)

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

all: $(vdr_PLUGINS) vdr-inputdev i18n

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

po/%.mo:	po/%.po
	$(MSGFMT) -c -o $@ $<


-include $(OBJS:%.o=%.d)

### Internationalization (I18N):

POTFILE   = po/$(PLUGIN).pot

_po_files = $(addsuffix .po,$(addprefix po/,$(LINGUAS)))
_mo_files = $(_po_files:%.po=%.mo)
_full_mo_path = $(DESTDIR)$(localedir)/$1/LC_MESSAGES/vdr-$(PLUGIN).mo
_all_inst_mo  = $(foreach l,$(LINGUAS),$(call _full_mo_path,$l))

$(_all_inst_mo):$(call _full_mo_path,%):	po/%.mo
	$(MKDIR_P) ${@D}
	$(INSTALL_DATA) $< $@

i18n:	$(_mo_files)

install-i18n:	$(_all_inst_mo)

%.po:	$(POTFILE)
	$(MSGMERGE) $(AM_MSGMERGEFLAGS) $@ $<
	@$(TOUCH_C) $@

$(POTFILE):	$(plugin_SOURCES)
	$(XGETTEXT) $(AM_XGETTEXTFLAGS) -o $@ $^
	@$(TOUCH_C) $@

### Targets:
vdr-inputdev:	$(helper_SOURCES)
	$(CC) $(call _buildflags,C) $^ -o $@

modmap.o:	gen-keymap.h

$(vdr_PLUGINS): $(plugin_OBJS)
	$(CXX) $(AM_LDFLAGS) $(LDFLAGS) $(LDFLAGS_$@) -shared -o $@ $^ $(LIBS)

_packages = $(addprefix $(PACKAGE),.xz .gz)

dist:  $(_packages) $(addsuffix .asc,$(_packages))

_tar_transform = --transform='s!^!$(ARCHIVE)/!'

$(PACKAGE):	$(_po_files) $(_all_sources)
	$(TAR) cf $@ $(TAR_FLAGS) $(_tar_transform) $(sort $^)

%.asc:		%
	$(GPG) --detach-sign --armor --output $@ $<

$(DESTDIR)$(plugindir) $(DESTDIR)$(udevdir):
	$(MKDIR_P) $@

install:	install-i18n install-plugin install-extra

install-plugin:	$(vdr_PLUGINS) | $(DESTDIR)$(plugindir) 
	$(INSTALL_PLUGIN) $(vdr_PLUGINS) $(DESTDIR)$(plugindir)/

install-extra:	vdr-inputdev | $(DESTDIR)$(udevdir)
	$(INSTALL_BIN) vdr-inputdev $(DESTDIR)$(udevdir)/

clean:
	@rm -f $(OBJS) libvdr*.so libvdr*.so.* *.d *.tgz core* *~ po/*.mo po/*.pot
	@rm -f vdr-inputdev

###

KEY_BLACKLIST = RESERVED\|MAX\|CNT

KEYMAP_SED = \
  -e '/^\#define KEY_\($(KEY_BLACKLIST)\)$$/d' \
  -e '/^\#define KEY_/{' \
  -e 's/^\#define KEY_\([A-Za-z0-9_]\+\)[[:space:]]*$$/\L\1\E,KEY_\1/p' \
  -e '};d'

gen-keymap.h:	gen-keymap.perf
	$(GPERF) $(GPERF_FLAGS) -t $< --output-file=$@

gen-keymap.perf:	Makefile
	rm -f $@ $@.tmp
	@echo '%readonly-tables' > $@.tmp
	@echo 'struct keymap_def { char const *name; unsigned int num; };' >> $@.tmp
	@echo '%%' >> $@.tmp
	@$(CC) $(call _buildflags,C) -imacros 'linux/input.h' -E -dN - </dev/null | \
		$(SED) $(KEYMAP_SED) | sort -n >>$@.tmp
	@mv $@.tmp $@
