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

#include <libsoup/soup.h>
#include <appstream-glib.h>

#include "cra-plugin.h"
#include "cra-screenshot.h"

typedef struct _CraScreenshotPrivate	CraScreenshotPrivate;
struct _CraScreenshotPrivate
{
	gchar			*app_id;
	gchar			*basename;
	SoupSession		*session;
	gboolean		 only_source;
	AsImage			*source;
	CraPackage		*pkg;
};

G_DEFINE_TYPE_WITH_PRIVATE (CraScreenshot, cra_screenshot, AS_TYPE_SCREENSHOT)

#define GET_PRIVATE(o) (cra_screenshot_get_instance_private (o))

/**
 * cra_screenshot_finalize:
 **/
static void
cra_screenshot_finalize (GObject *object)
{
	CraScreenshot *screenshot = CRA_SCREENSHOT (object);
	CraScreenshotPrivate *priv = GET_PRIVATE (screenshot);

	g_free (priv->app_id);
	g_free (priv->basename);
	g_object_unref (priv->session);
	g_object_unref (priv->source);
	if (priv->pkg != NULL)
		g_object_unref (priv->pkg);

	G_OBJECT_CLASS (cra_screenshot_parent_class)->finalize (object);
}

/**
 * cra_screenshot_init:
 **/
static void
cra_screenshot_init (CraScreenshot *screenshot)
{
	CraScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	priv->source = as_image_new ();
	as_image_set_kind (priv->source, AS_IMAGE_KIND_SOURCE);
	priv->session = soup_session_new_with_options (SOUP_SESSION_USER_AGENT, "createrepo_as",
						       SOUP_SESSION_TIMEOUT, 5000,
						       NULL);
	soup_session_add_feature_by_type (priv->session,
					  SOUP_TYPE_PROXY_RESOLVER_DEFAULT);
}

/**
 * cra_screenshot_set_pixbuf:
 **/
void
cra_screenshot_set_pixbuf (CraScreenshot *screenshot, GdkPixbuf *pixbuf)
{
	AsImage *im;
	CraScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	const gchar *mirror_uri;
	gchar *size_str;
	gchar *url_tmp;
	guint i;
	guint sizes[] = { 624, 351, 112, 63, 752, 423, 0 };

	as_image_set_pixbuf (priv->source, pixbuf);
	priv->basename = g_strdup_printf ("%s-%s.png",
					  priv->app_id,
					  as_image_get_md5 (priv->source));

	/* create the source */
	mirror_uri = cra_package_get_config (priv->pkg, "MirrorURI");
	if (priv->only_source) {
		im = as_image_new ();
		as_image_set_kind (im, AS_IMAGE_KIND_SOURCE);
		as_image_set_width (im, gdk_pixbuf_get_width (pixbuf));
		as_image_set_height (im, gdk_pixbuf_get_height (pixbuf));
		url_tmp = g_build_filename (mirror_uri,
					    "source",
					    priv->basename,
					    NULL);
		as_image_set_url (im, url_tmp, -1);
		as_screenshot_add_image (AS_SCREENSHOT (screenshot), im);
		g_free (url_tmp);

	/* create the various images in the _baseclass_ screenshot */
	} else {
		for (i = 0; sizes[i] != 0; i += 2) {
			size_str = g_strdup_printf ("%ix%i", sizes[i], sizes[i+1]);
			url_tmp = g_build_filename (mirror_uri,
						    size_str,
						    priv->basename,
						    NULL);
			im = as_image_new ();
			as_image_set_kind (im, AS_IMAGE_KIND_THUMBNAIL);
			as_image_set_width (im, sizes[i]);
			as_image_set_height (im, sizes[i+1]);
			as_image_set_url (im, url_tmp, -1);
			as_screenshot_add_image (AS_SCREENSHOT (screenshot), im);
			g_free (url_tmp);
			g_free (size_str);
			g_object_unref (im);
		}
	}
}

/**
 * cra_screenshot_load_filename:
 **/
gboolean
cra_screenshot_load_filename (CraScreenshot *screenshot,
			      const gchar *filename,
			      GError **error)
{
	CraScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	gboolean ret = TRUE;

	/* load source */
	ret = as_image_load_filename (priv->source, filename, error);
	if (!ret)
		goto out;

	/* is the aspect ratio of the source perfectly 16:9 */
	if ((as_image_get_width (priv->source) / 16) * 9 !=
	     as_image_get_height (priv->source)) {
		cra_package_log (priv->pkg,
				 CRA_PACKAGE_LOG_LEVEL_WARNING,
				 "%s is not in 16:9 aspect ratio",
				 filename);
	}
out:
	return ret;
}

/**
 * cra_screenshot_save_pixbuf:
 **/
static gboolean
cra_screenshot_save_pixbuf (CraScreenshot *screenshot,
			    guint width, guint height,
			    GError **error)
{
	CraScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	const gchar *output_dir;
	gboolean ret = TRUE;
	gchar *filename = NULL;
	gchar *size_str;

	/* does screenshot already exist */
	size_str = g_strdup_printf ("%ix%i", width, height);
	output_dir = cra_package_get_config (priv->pkg, "OutputDir");
	filename = g_build_filename (output_dir,
				     "screenshots",
				     size_str,
				     priv->basename,
				     NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		cra_package_log (priv->pkg,
				 CRA_PACKAGE_LOG_LEVEL_DEBUG,
				 "%s screenshot already exists", size_str);
		goto out;
	}

	/* is the aspect ratio of the source perfectly 16:9 */
	ret = as_image_save_filename (priv->source,
				      filename,
				      width,
				      height,
				      AS_IMAGE_SAVE_FLAG_PAD_16_9,
				      error);
	if (!ret)
		goto out;

	/* set new AppStream compatible screenshot name */
	cra_package_log (priv->pkg,
			 CRA_PACKAGE_LOG_LEVEL_DEBUG,
			 "saved %s screenshot", size_str);
out:
	g_free (filename);
	g_free (size_str);
	return ret;
}

/**
 * cra_screenshot_save_source:
 **/
static gboolean
cra_screenshot_save_source (CraScreenshot *screenshot, GError **error)
{
	CraScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	const gchar *output_dir;
	gboolean ret = TRUE;
	gchar *filename = NULL;

	/* does screenshot already exist */
	output_dir = cra_package_get_config (priv->pkg, "OutputDir");
	filename = g_build_filename (output_dir,
				     "screenshots",
				     "source",
				     priv->basename,
				     NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		cra_package_log (priv->pkg,
				 CRA_PACKAGE_LOG_LEVEL_DEBUG,
				 "source screenshot already exists");
		goto out;
	}

	/* is the aspect ratio of the source perfectly 16:9 */
	ret = as_image_save_filename (priv->source,
				      filename,
				      0, 0,
				      AS_IMAGE_SAVE_FLAG_NONE,
				      error);
	if (!ret)
		goto out;

	/* set new AppStream compatible screenshot name */
	cra_package_log (priv->pkg,
			 CRA_PACKAGE_LOG_LEVEL_DEBUG,
			 "saved source screenshot");
out:
	g_free (filename);
	return ret;
}

/**
 * cra_screenshot_save_resources:
 **/
gboolean
cra_screenshot_save_resources (CraScreenshot *screenshot, GError **error)
{
	AsImage *im;
	CraScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	GPtrArray *images;
	gboolean ret = TRUE;
	guint i;

	/* any non-stock icon set */
	if (as_image_get_pixbuf (priv->source) == NULL)
		goto out;

	/* most of the time, don't save a source image */
	if (priv->only_source) {
		ret = cra_screenshot_save_source (screenshot, error);
		if (!ret)
			goto out;

	/* save each image */
	} else {
		images = as_screenshot_get_images (AS_SCREENSHOT (screenshot));
		for (i = 0; i < images->len; i++) {
			im = g_ptr_array_index (images, i);
			ret = cra_screenshot_save_pixbuf (screenshot,
							  as_image_get_width (im),
							  as_image_get_height (im),
							  error);
			if (!ret)
				goto out;
		}
	}
out:
	return ret;
}

/**
 * cra_screenshot_load_url:
 **/
gboolean
cra_screenshot_load_url (CraScreenshot *screenshot, const gchar *url, GError **error)
{
	CraScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	const gchar *cache_dir;
	gboolean ret = TRUE;
	gchar *basename;
	gchar *cache_filename;
	SoupMessage *msg = NULL;
	SoupStatus status;
	SoupURI *uri = NULL;

	/* download to cache if not already added */
	basename = g_path_get_basename (url);
	cache_dir = cra_package_get_config (priv->pkg, "CacheDir");
	cache_filename = g_strdup_printf ("%s/%s-%s",
					  cache_dir,
					  priv->app_id,
					  basename);
	if (!g_file_test (cache_filename, G_FILE_TEST_EXISTS)) {
		uri = soup_uri_new (url);
		if (uri == NULL) {
			ret = FALSE;
			g_set_error (error,
				     CRA_PLUGIN_ERROR,
				     CRA_PLUGIN_ERROR_FAILED,
				     "Could not parse '%s' as a URL", url);
			goto out;
		}
		cra_package_log (priv->pkg,
				 CRA_PACKAGE_LOG_LEVEL_DEBUG,
				 "Downloading %s", url);
		msg = soup_message_new_from_uri (SOUP_METHOD_GET, uri);
		status = soup_session_send_message (priv->session, msg);
		if (status != SOUP_STATUS_OK) {
			ret = FALSE;
			g_set_error (error,
				     CRA_PLUGIN_ERROR,
				     CRA_PLUGIN_ERROR_FAILED,
				     "Downloading failed: %s",
				     soup_status_get_phrase (status));
			goto out;
		}

		/* save new file */
		ret = g_file_set_contents (cache_filename,
					   msg->response_body->data,
					   msg->response_body->length,
					   error);
		if (!ret)
			goto out;
	}

	/* load the pixbuf */
	ret = cra_screenshot_load_filename (screenshot, cache_filename, error);
	if (!ret)
		goto out;
out:
	if (uri != NULL)
		soup_uri_free (uri);
	if (msg != NULL)
		g_object_unref (msg);
	g_free (basename);
	g_free (cache_filename);
	return ret;
}

/**
 * cra_screenshot_set_only_source:
 **/
void
cra_screenshot_set_only_source (CraScreenshot *screenshot, gboolean only_source)
{
	CraScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	priv->only_source = only_source;
}

/**
 * cra_screenshot_class_init:
 **/
static void
cra_screenshot_class_init (CraScreenshotClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cra_screenshot_finalize;
}

/**
 * cra_screenshot_new:
 **/
CraScreenshot *
cra_screenshot_new (CraPackage *pkg, const gchar *app_id)
{
	CraScreenshot *screenshot;
	CraScreenshotPrivate *priv;
	screenshot = g_object_new (CRA_TYPE_SCREENSHOT, NULL);
	priv = GET_PRIVATE (screenshot);
	if (pkg != NULL)
		priv->pkg = g_object_ref (pkg);
	priv->app_id = g_strdup (app_id);
	return CRA_SCREENSHOT (screenshot);
}
