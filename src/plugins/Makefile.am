AM_CPPFLAGS =						\
	$(APPSTREAM_CFLAGS)				\
	$(GDKPIXBUF_CFLAGS)				\
	$(GLIB_CFLAGS)					\
	$(LIBARCHIVE_CFLAGS)				\
	$(LIBSOUP_CFLAGS)				\
	$(RPM_CFLAGS)					\
	$(SQLITE_CFLAGS)				\
	$(FREETYPE_CFLAGS)				\
	-I..						\
	-DG_LOG_DOMAIN=\"Cra\"

plugindir = $(libdir)/cra-plugins
plugin_LTLIBRARIES =					\
	libcra_plugin_appdata.la			\
	libcra_plugin_blacklist.la			\
	libcra_plugin_desktop.la			\
	libcra_plugin_gir.la				\
	libcra_plugin_font.la				\
	libcra_plugin_gettext.la			\
	libcra_plugin_gstreamer.la			\
	libcra_plugin_hardcoded.la			\
	libcra_plugin_ibus_sql.la			\
	libcra_plugin_ibus_xml.la			\
	libcra_plugin_metainfo.la			\
	libcra_plugin_nm.la

libcra_plugin_gstreamer_la_SOURCES = cra-plugin-gstreamer.c
libcra_plugin_gstreamer_la_LIBADD = $(GLIB_LIBS)
libcra_plugin_gstreamer_la_LDFLAGS = -module -avoid-version
libcra_plugin_gstreamer_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libcra_plugin_metainfo_la_SOURCES = cra-plugin-metainfo.c
libcra_plugin_metainfo_la_LIBADD = $(GLIB_LIBS)
libcra_plugin_metainfo_la_LDFLAGS = -module -avoid-version
libcra_plugin_metainfo_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libcra_plugin_gettext_la_SOURCES = cra-plugin-gettext.c
libcra_plugin_gettext_la_LIBADD = $(GLIB_LIBS)
libcra_plugin_gettext_la_LDFLAGS = -module -avoid-version
libcra_plugin_gettext_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libcra_plugin_blacklist_la_SOURCES = cra-plugin-blacklist.c
libcra_plugin_blacklist_la_LIBADD = $(GLIB_LIBS)
libcra_plugin_blacklist_la_LDFLAGS = -module -avoid-version
libcra_plugin_blacklist_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libcra_plugin_hardcoded_la_SOURCES = cra-plugin-hardcoded.c
libcra_plugin_hardcoded_la_LIBADD = $(GLIB_LIBS)
libcra_plugin_hardcoded_la_LDFLAGS = -module -avoid-version
libcra_plugin_hardcoded_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libcra_plugin_desktop_la_SOURCES = cra-plugin-desktop.c
libcra_plugin_desktop_la_LIBADD = $(GLIB_LIBS) $(GDKPIXBUF_LIBS)
libcra_plugin_desktop_la_LDFLAGS = -module -avoid-version
libcra_plugin_desktop_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libcra_plugin_gir_la_SOURCES = cra-plugin-gir.c
libcra_plugin_gir_la_LIBADD = $(GLIB_LIBS) $(GDKPIXBUF_LIBS)
libcra_plugin_gir_la_LDFLAGS = -module -avoid-version
libcra_plugin_gir_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libcra_plugin_appdata_la_SOURCES = cra-plugin-appdata.c
libcra_plugin_appdata_la_LIBADD = $(GLIB_LIBS) $(GDKPIXBUF_LIBS)
libcra_plugin_appdata_la_LDFLAGS = -module -avoid-version
libcra_plugin_appdata_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libcra_plugin_nm_la_SOURCES = cra-plugin-nm.c
libcra_plugin_nm_la_LIBADD = $(GLIB_LIBS)
libcra_plugin_nm_la_LDFLAGS = -module -avoid-version
libcra_plugin_nm_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libcra_plugin_ibus_sql_la_SOURCES = cra-plugin-ibus-sql.c
libcra_plugin_ibus_sql_la_LIBADD = $(GLIB_LIBS) $(SQLITE_LIBS)
libcra_plugin_ibus_sql_la_LDFLAGS = -module -avoid-version
libcra_plugin_ibus_sql_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libcra_plugin_ibus_xml_la_SOURCES = cra-plugin-ibus-xml.c
libcra_plugin_ibus_xml_la_LIBADD = $(GLIB_LIBS) $(SQLITE_LIBS)
libcra_plugin_ibus_xml_la_LDFLAGS = -module -avoid-version
libcra_plugin_ibus_xml_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libcra_plugin_font_la_SOURCES = cra-plugin-font.c
libcra_plugin_font_la_LIBADD = $(GLIB_LIBS) $(FREETYPE_LIBS) $(GDKPIXBUF_LIBS)
libcra_plugin_font_la_LDFLAGS = -module -avoid-version
libcra_plugin_font_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

-include $(top_srcdir)/git.mk
