/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <limits.h>
#include <archive.h>
#include <archive_entry.h>

#include <rpm/rpmlib.h>
#include <rpm/rpmts.h>

#include "cra-package-rpm.h"
#include "cra-plugin.h"

typedef struct _CraPackageRpmPrivate	CraPackageRpmPrivate;
struct _CraPackageRpmPrivate
{
	Header		 h;
};

G_DEFINE_TYPE_WITH_PRIVATE (CraPackageRpm, cra_package_rpm, CRA_TYPE_PACKAGE)

#define GET_PRIVATE(o) (cra_package_rpm_get_instance_private (o))

/**
 * cra_package_rpm_finalize:
 **/
static void
cra_package_rpm_finalize (GObject *object)
{
	CraPackageRpm *pkg = CRA_PACKAGE_RPM (object);
	CraPackageRpmPrivate *priv = GET_PRIVATE (pkg);

	headerFree (priv->h);

	G_OBJECT_CLASS (cra_package_rpm_parent_class)->finalize (object);
}

/**
 * cra_package_rpm_init:
 **/
static void
cra_package_rpm_init (CraPackageRpm *pkg)
{
}

/**
 * cra_package_rpm_ensure_simple:
 **/
static gboolean
cra_package_rpm_ensure_simple (CraPackage *pkg, GError **error)
{
	CraPackageRpm *pkg_rpm = CRA_PACKAGE_RPM (pkg);
	CraPackageRpmPrivate *priv = GET_PRIVATE (pkg_rpm);
	gboolean ret = TRUE;
	gchar *srcrpm;
	gchar *tmp;
	rpmtd td = NULL;

	/* get the simple stuff */
	td = rpmtdNew ();
	headerGet (priv->h, RPMTAG_NAME, td, HEADERGET_MINMEM);
	cra_package_set_name (pkg, rpmtdGetString (td));
	headerGet (priv->h, RPMTAG_VERSION, td, HEADERGET_MINMEM);
	cra_package_set_version (pkg, rpmtdGetString (td));
	headerGet (priv->h, RPMTAG_RELEASE, td, HEADERGET_MINMEM);
	cra_package_set_release (pkg, rpmtdGetString (td));
	headerGet (priv->h, RPMTAG_ARCH, td, HEADERGET_MINMEM);
	cra_package_set_arch (pkg, rpmtdGetString (td));
	headerGet (priv->h, RPMTAG_EPOCH, td, HEADERGET_MINMEM);
	cra_package_set_epoch (pkg, rpmtdGetNumber (td));
	headerGet (priv->h, RPMTAG_URL, td, HEADERGET_MINMEM);
	cra_package_set_url (pkg, rpmtdGetString (td));
	headerGet (priv->h, RPMTAG_LICENSE, td, HEADERGET_MINMEM);
	cra_package_set_license (pkg, rpmtdGetString (td));

	/* source */
	headerGet (priv->h, RPMTAG_SOURCERPM, td, HEADERGET_MINMEM);
	srcrpm = g_strdup (rpmtdGetString (td));
	tmp = g_strstr_len (srcrpm, -1, ".src.rpm");
	if (tmp != NULL)
		*tmp = '\0';
	cra_package_set_source (pkg, srcrpm);
	if (td != NULL)
		rpmtdFree (td);
	g_free (srcrpm);
	return ret;
}

/**
 * cra_package_rpm_add_release:
 **/
static void
cra_package_rpm_add_release (CraPackage *pkg,
			     guint64 timestamp,
			     const gchar *name,
			     const gchar *text)
{
	AsRelease *release;
	const gchar *version;
	gchar *name_dup;
	gchar *tmp;
	gchar *vr;

	/* get last string chunk */
	name_dup = g_strchomp (g_strdup (name));
	vr = g_strrstr (name_dup, " ");
	if (vr == NULL)
		goto out;

	/* get last string chunk */
	version = vr + 1;
	tmp = g_strstr_len (version, -1, "-");
	if (tmp == NULL) {
		if (g_strstr_len (version, -1, ">") != NULL)
			goto out;
	} else {
		*tmp = '\0';
	}

	/* remove any epoch */
	tmp = g_strstr_len (version, -1, ":");
	if (tmp != NULL)
		version = tmp + 1;

	/* is version already in the database */
	release = cra_package_get_release (pkg, version);
	if (release != NULL) {
		/* use the earlier timestamp to ignore auto-rebuilds with just
		 * a bumped release */
		if (timestamp < as_release_get_timestamp (release))
			as_release_set_timestamp (release, timestamp);
	} else {
		release = as_release_new ();
		as_release_set_version (release, version, -1);
		as_release_set_timestamp (release, timestamp);
		as_release_set_description (release, NULL, text, -1);
		cra_package_add_release (pkg, version, release);
		g_object_unref (release);
	}
out:
	g_free (name_dup);
}

/**
 * cra_package_rpm_ensure_releases:
 **/
static gboolean
cra_package_rpm_ensure_releases (CraPackage *pkg, GError **error)
{
	CraPackageRpm *pkg_rpm = CRA_PACKAGE_RPM (pkg);
	CraPackageRpmPrivate *priv = GET_PRIVATE (pkg_rpm);
	gboolean ret = TRUE;
	guint i;
	rpmtd td[3] = { NULL, NULL, NULL };

	/* read out the file list */
	for (i = 0; i < 3; i++)
		td[i] = rpmtdNew ();
	/* get the ChangeLog info */
	headerGet (priv->h, RPMTAG_CHANGELOGTIME, td[0], HEADERGET_MINMEM);
	headerGet (priv->h, RPMTAG_CHANGELOGNAME, td[1], HEADERGET_MINMEM);
	headerGet (priv->h, RPMTAG_CHANGELOGTEXT, td[2], HEADERGET_MINMEM);
	while (rpmtdNext (td[0]) != -1 &&
	       rpmtdNext (td[1]) != -1 &&
	       rpmtdNext (td[2]) != -1) {
		cra_package_rpm_add_release (pkg,
					     rpmtdGetNumber (td[0]),
					     rpmtdGetString (td[1]),
					     rpmtdGetString (td[2]));
	}
	for (i = 0; i < 3; i++) {
		rpmtdFreeData (td[i]);
		rpmtdFree (td[i]);
	}
	return ret;
}

/**
 * cra_package_rpm_ensure_deps:
 **/
static gboolean
cra_package_rpm_ensure_deps (CraPackage *pkg, GError **error)
{
	CraPackageRpm *pkg_rpm = CRA_PACKAGE_RPM (pkg);
	CraPackageRpmPrivate *priv = GET_PRIVATE (pkg_rpm);
	const gchar *dep;
	gchar **deps = NULL;
	gboolean ret = TRUE;
	gchar *tmp;
	gint rc;
	guint i = 0;
	rpmtd td = NULL;

	/* read out the dep list */
	td = rpmtdNew ();
	rc = headerGet (priv->h, RPMTAG_REQUIRENAME, td, HEADERGET_MINMEM);
	if (!rc) {
		ret = FALSE;
		g_set_error (error,
			     CRA_PLUGIN_ERROR,
			     CRA_PLUGIN_ERROR_FAILED,
			     "Failed to read list of requires %s",
			     cra_package_get_filename (pkg));
		goto out;
	}
	deps = g_new0 (gchar *, rpmtdCount (td) + 1);
	while (rpmtdNext (td) != -1) {
		dep = rpmtdGetString (td);
		if (g_str_has_prefix (dep, "rpmlib"))
			continue;
		if (g_strcmp0 (dep, "/bin/sh") == 0)
			continue;
		deps[i] = g_strdup (dep);
		tmp = g_strstr_len (deps[i], -1, "(");
		if (tmp != NULL)
			*tmp = '\0';
		/* TODO: deduplicate */
		i++;
	}
	cra_package_set_deps (pkg, deps);
out:
	g_strfreev (deps);
	rpmtdFreeData (td);
	rpmtdFree (td);
	return ret;
}

/**
 * cra_package_rpm_ensure_filelists:
 **/
static gboolean
cra_package_rpm_ensure_filelists (CraPackage *pkg, GError **error)
{
	CraPackageRpm *pkg_rpm = CRA_PACKAGE_RPM (pkg);
	CraPackageRpmPrivate *priv = GET_PRIVATE (pkg_rpm);
	const gchar **dirnames = NULL;
	gboolean ret = TRUE;
	gchar **filelist = NULL;
	gint32 *dirindex = NULL;
	gint rc;
	guint i;
	rpmtd td[3] = { NULL, NULL, NULL };

	/* read out the file list */
	for (i = 0; i < 3; i++)
		td[i] = rpmtdNew ();
	rc = headerGet (priv->h, RPMTAG_DIRNAMES, td[0], HEADERGET_MINMEM);
	if (rc)
		rc = headerGet (priv->h, RPMTAG_BASENAMES, td[1], HEADERGET_MINMEM);
	if (rc)
		rc = headerGet (priv->h, RPMTAG_DIRINDEXES, td[2], HEADERGET_MINMEM);
	if (!rc) {
		ret = FALSE;
		g_set_error (error,
			     CRA_PLUGIN_ERROR,
			     CRA_PLUGIN_ERROR_FAILED,
			     "Failed to read package file list %s",
			     cra_package_get_filename (pkg));
		goto out;
	}
	i = 0;
	dirnames = g_new0 (const gchar *, rpmtdCount (td[0]) + 1);
	while (rpmtdNext (td[0]) != -1)
		dirnames[i++] = rpmtdGetString (td[0]);
	i = 0;
	dirindex = g_new0 (gint32, rpmtdCount (td[2]) + 1);
	while (rpmtdNext (td[2]) != -1)
		dirindex[i++] = rpmtdGetNumber (td[2]);
	i = 0;
	filelist = g_new0 (gchar *, rpmtdCount (td[1]) + 1);
	while (rpmtdNext (td[1]) != -1) {
		filelist[i] = g_build_filename (dirnames[dirindex[i]],
						rpmtdGetString (td[1]),
						NULL);
		i++;
	}
	cra_package_set_filelist (pkg, filelist);
out:
	for (i = 0; i < 3; i++) {
		rpmtdFreeData (td[i]);
		rpmtdFree (td[i]);
	}
	g_strfreev (filelist);
	g_free (dirindex);
	g_free (dirnames);
	return ret;
}

/**
 * cra_package_rpm_open:
 **/
static gboolean
cra_package_rpm_open (CraPackage *pkg, const gchar *filename, GError **error)
{
	CraPackageRpm *pkg_rpm = CRA_PACKAGE_RPM (pkg);
	CraPackageRpmPrivate *priv = GET_PRIVATE (pkg_rpm);
	FD_t fd;
	gboolean ret = TRUE;
	gint rc;
	rpmts ts;

	/* open the file */
	ts = rpmtsCreate ();
	fd = Fopen (filename, "r");
	if (fd <= 0) {
		ret = FALSE;
		g_set_error (error,
			     CRA_PLUGIN_ERROR,
			     CRA_PLUGIN_ERROR_FAILED,
			     "Failed to open package %s", filename);
		goto out;
	}

	/* create package */
	rc = rpmReadPackageFile (ts, fd, filename, &priv->h);
	if (rc != RPMRC_OK) {
		ret = FALSE;
		g_set_error (error,
			     CRA_PLUGIN_ERROR,
			     CRA_PLUGIN_ERROR_FAILED,
			     "Failed to read package %s", filename);
		goto out;
	}

	/* read package stuff */
	ret = cra_package_rpm_ensure_simple (pkg, error);
	if (!ret)
		goto out;
	ret = cra_package_rpm_ensure_releases (pkg, error);
	if (!ret)
		goto out;
	ret = cra_package_rpm_ensure_deps (pkg, error);
	if (!ret)
		goto out;
	ret = cra_package_rpm_ensure_filelists (pkg, error);
	if (!ret)
		goto out;
out:
	rpmtsFree (ts);
	Fclose (fd);
	return ret;
}

/**
 * cra_package_rpm_class_init:
 **/
static void
cra_package_rpm_class_init (CraPackageRpmClass *klass)
{
	CraPackageClass *package_class = CRA_PACKAGE_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = cra_package_rpm_finalize;
	package_class->open = cra_package_rpm_open;
}

/**
 * cra_package_rpm_init_cb:
 **/
static gpointer
cra_package_rpm_init_cb (gpointer user_data)
{
	rpmReadConfigFiles (NULL, NULL);
	return NULL;
}

/**
 * cra_package_rpm_new:
 **/
CraPackage *
cra_package_rpm_new (void)
{
	CraPackage *pkg;
	static GOnce rpm_init = G_ONCE_INIT;
	g_once (&rpm_init, cra_package_rpm_init_cb, NULL);
	pkg = g_object_new (CRA_TYPE_PACKAGE_RPM, NULL);
	return CRA_PACKAGE (pkg);
}