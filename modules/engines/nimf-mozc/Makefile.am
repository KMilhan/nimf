moduledir = $(libdir)/nimf/modules
module_LTLIBRARIES = libnimf-mozc.la

gsettings_SCHEMAS = org.nimf.engines.nimf-mozc.gschema.xml
@GSETTINGS_RULES@

NIMF_MOZC_CFLAGS = \
	$(EXTRA_CFLAGS) \
	-I$(top_srcdir)/libnimf \
	-DG_LOG_DOMAIN=\"nimf\" \
	$(NIMF_MOZC_DEPS_CFLAGS)

NIMF_MOZC_SOURCES = \
	nimf-mozc-helper.c \
	nimf-mozc-helper.h \
	nimf-mozc-key.c \
	nimf-mozc-key.h \
	nimf-mozc-protocol.h \
	nimf-mozc-sexp.c \
	nimf-mozc-sexp.h \
	$(NULL)

libnimf_mozc_la_SOURCES = nimf-mozc.c $(NIMF_MOZC_SOURCES)
libnimf_mozc_la_CFLAGS  = $(NIMF_MOZC_CFLAGS)
libnimf_mozc_la_LDFLAGS = -avoid-version -module $(NIMF_MOZC_DEPS_LIBS)
libnimf_mozc_la_LIBADD  = $(top_builddir)/libnimf/libnimf.la

check_PROGRAMS = test-nimf-mozc
TESTS = $(check_PROGRAMS)
test_nimf_mozc_SOURCES = test-nimf-mozc.c $(NIMF_MOZC_SOURCES)
test_nimf_mozc_CFLAGS  = \
	$(NIMF_MOZC_CFLAGS) \
	-DFAKE_HELPER=\"$(abs_srcdir)/test-fake-helper.sh\"
test_nimf_mozc_LDFLAGS = $(NIMF_MOZC_DEPS_LIBS)
test_nimf_mozc_LDADD   = $(top_builddir)/libnimf/libnimf.la

EXTRA_DIST = \
	$(gsettings_SCHEMAS) \
	test-fake-helper.sh \
	$(NULL)

install-data-hook:
	chmod -x $(DESTDIR)$(moduledir)/libnimf-mozc.so
	rm    -f $(DESTDIR)$(moduledir)/libnimf-mozc.la

uninstall-hook:
	 rm    -f $(DESTDIR)$(moduledir)/libnimf-mozc.so
	-rmdir -p $(DESTDIR)$(moduledir)

DISTCLEANFILES = Makefile.in
