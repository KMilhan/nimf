libexec_PROGRAMS = nimf-ibus
nimf_ibus_SOURCES = nimf-ibus.c
nimf_ibus_CPPFLAGS = $(IM_NIMF_IBUS_DEPS_CFLAGS) -I$(top_srcdir)/libnimf
nimf_ibus_LDADD = $(top_builddir)/libnimf/libnimf.la $(IM_NIMF_IBUS_DEPS_LIBS)

componentdir = $(datadir)/ibus/component
nodist_component_DATA = nimf.xml
EXTRA_DIST = nimf.xml.in
CLEANFILES = nimf.xml

nimf.xml: $(srcdir)/nimf.xml.in Makefile
	$(AM_V_GEN)sed 's|@NIMF_IBUS_EXEC@|$(libexecdir)/nimf-ibus|g' $< > $@
