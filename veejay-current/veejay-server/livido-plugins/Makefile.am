lividoplugin_LTLIBRARIES = lvd_bgsubtract.la \
			   lvd_solid.la \
			   lvd_colortone.la \
			   lvd_displaywall.la \
			   lvd_stroboscope.la \
			   lvd_colorexchange.la \
			   lvd_starfield.la \
			   lvd_explosion.la 

lvd_bgsubtract_la_SOURCES = lvd_bgsubtract.c
lvd_solid_la_SOURCES = lvd_solid.c
lvd_colortone_la_SOURCES = lvd_colortone.c
lvd_displaywall_la_SOURCES = lvd_displaywall.c
lvd_stroboscope_la_SOURCES = lvd_stroboscope.c
lvd_colorexchange_la_SOURCES = lvd_colorexchange.c
lvd_starfield_la_SOURCES = lvd_starfield.c
lvd_explosion_la_SOURCES = lvd_explosion.c

AM_CPPFLAGS = $(FFMPEG_CFLAGS) $(VJE_CFLAGS)
AM_CFLAGS = -I@top_srcdir@/include $(VJE_CFLAGS)
AM_LDFLAGS = -module -avoid-version -lm -export-dynamic
AM_LIBTOOLFLAGS = --tag=disable-static

lividoplugindir = @libdir@/livido-plugins

install-lividopluginLTLIBRARIES:$(lividoplugin_LTLIBRARIES)
	mkdir -p $(DESTDIR)/$(lividoplugindir)
	list='$(lividoplugin_LTLIBRARIES)'; \
	for file in $$list; do \
	 sofile=`basename $$file .la`.so; \
	 $(INSTALL_PROGRAM) .libs/$$sofile $(DESTDIR)/$(lividoplugindir);\
	done

uninstall-lividopluginLTLIBRARIES:
	list='$(lividoplugin_LTLIBRARIES)'; \
	for file in $$list; do \
	 sofile=`basename $$file .la`.so; \
	 rm -f $(DESTDIR)/$(lividoplugindir)/$$sofile; \
	done

