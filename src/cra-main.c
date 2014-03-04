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

#include "cra-context.h"
#include "cra-package.h"
#include "cra-plugin.h"
#include "cra-plugin-loader.h"
#include "cra-utils.h"

#include <glib.h>
#include <locale.h>
#include <rpm/rpmlib.h>

typedef struct {
	gchar		*filename;
	gchar		*tmpdir;
	CraPackage	*pkg;
} CraTask;

/**
 * cra_task_free:
 */
static void
cra_task_free (CraTask *task)
{
	cra_package_log (task->pkg,
			 CRA_PACKAGE_LOG_LEVEL_INFO,
			 "Flushing %s", task->filename);
	cra_package_log_flush (task->pkg, NULL);
	g_object_unref (task->pkg);
	g_free (task->filename);
	g_free (task->tmpdir);
	g_free (task);
}

/**
 * cra_task_process_func:
 */
static void
cra_task_process_func (gpointer data, gpointer user_data)
{
	const gchar *pkg_name;
	CraApp *app;
	CraContext *ctx = (CraContext *) user_data;
	CraPackage *pkg_extra;
	CraPlugin *plugin = NULL;
	CraTask *task = (CraTask *) data;
	gboolean ret;
	gchar **filelist;
	gchar *tmp;
	GError *error = NULL;
	GList *apps = NULL;
	GList *l;
	guint i;

	/* get file list */
	ret = cra_package_ensure_filelist (task->pkg, &error);
	if (!ret) {
		cra_package_log (task->pkg,
				 CRA_PACKAGE_LOG_LEVEL_WARNING,
				 "Failed to get file list: %s",
				 error->message);
		g_error_free (error);
		goto out;
	}

	/* did we get a file match on any plugin */
	cra_package_log (task->pkg,
			 CRA_PACKAGE_LOG_LEVEL_INFO,
			 "Getting filename match for %s",
			 task->filename);
	filelist = cra_package_get_filelist (task->pkg);
	for (i = 0; filelist[i] != NULL; i++) {
		plugin = cra_plugin_loader_match_fn (ctx->plugins, filelist[i]);
		if (plugin != NULL)
			break;
	}
	if (plugin == NULL)
		goto out;

	/* delete old tree if it exists */
	ret = cra_utils_ensure_exists_and_empty (task->tmpdir, &error);
	if (!ret) {
		cra_package_log (task->pkg,
				 CRA_PACKAGE_LOG_LEVEL_WARNING,
				 "Failed to clear: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* explode tree */
	ret = cra_package_explode (task->pkg, task->tmpdir, &error);
	if (!ret) {
		cra_package_log (task->pkg,
				 CRA_PACKAGE_LOG_LEVEL_WARNING,
				 "Failed to explode: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add extra packages */
	pkg_name = cra_glob_value_search (ctx->extra_pkgs, cra_package_get_name (task->pkg));
	if (pkg_name != NULL) {
		pkg_extra = cra_context_find_by_pkgname (ctx, pkg_name);
		if (pkg_extra == NULL) {
			cra_package_log (task->pkg,
					 CRA_PACKAGE_LOG_LEVEL_WARNING,
					 "%s requires %s but is not available",
					 cra_package_get_name (task->pkg),
					 pkg_name);
			goto out;
		}
		ret = cra_package_explode (pkg_extra, task->tmpdir, &error);
		if (!ret) {
			cra_package_log (task->pkg,
					 CRA_PACKAGE_LOG_LEVEL_WARNING,
					 "Failed to explode extra file: %s",
					 error->message);
			g_error_free (error);
			goto out;
		}
	}

	/* run plugin */
	cra_package_log (task->pkg,
			 CRA_PACKAGE_LOG_LEVEL_INFO,
			 "Processing %s with %s [%p]",
			 task->filename,
			 plugin->name,
			 g_thread_self ());
	apps = cra_plugin_process (plugin, task->pkg, task->tmpdir, &error);
	if (apps == NULL) {
		cra_package_log (task->pkg,
				 CRA_PACKAGE_LOG_LEVEL_WARNING,
				 "Failed to run process: %s",
				 error->message);
		g_error_free (error);
		goto out;
	}

	/* print */
	for (l = apps; l != NULL; l = l->next) {
		app = l->data;

		/* never set */
		if (cra_app_get_id_full (app) == NULL) {
			cra_package_log (task->pkg,
					 CRA_PACKAGE_LOG_LEVEL_INFO,
					 "app id not set for %s",
					 cra_package_get_name (task->pkg));
			continue;
		}

		/* is application backlisted */
		if (cra_glob_value_search (ctx->blacklisted_ids,
					   cra_app_get_id (app))) {
			cra_package_log (task->pkg,
					 CRA_PACKAGE_LOG_LEVEL_INFO,
					 "app id %s is blacklisted",
					 cra_package_get_name (task->pkg));
			continue;
		}

		/* copy data from pkg into app */
		if (cra_package_get_url (task->pkg) != NULL)
			cra_app_set_homepage_url (app, cra_package_get_url (task->pkg));
		if (cra_package_get_license (task->pkg) != NULL)
			cra_app_set_project_license (app, cra_package_get_license (task->pkg));

		/* run each refine plugin on each app */
		ret = cra_plugin_loader_process_app (ctx->plugins,
						     task->pkg,
						     app,
						     task->tmpdir,
						     &error);
		if (!ret) {
			cra_package_log (task->pkg,
					 CRA_PACKAGE_LOG_LEVEL_WARNING,
					 "Failed to run process: %s",
					 error->message);
			g_error_free (error);
			goto out;
		}

		/* don't include apps that *still* require appdata */
		if (cra_app_get_requires_appdata (app)) {
			cra_package_log (task->pkg,
					 CRA_PACKAGE_LOG_LEVEL_INFO,
					 "%s required appdata but none provided",
					 cra_app_get_id_full (app));
			continue;
		}

		/* don't include apps that have no icon */
		if (cra_app_get_icon (app) == NULL) {
			cra_package_log (task->pkg,
					 CRA_PACKAGE_LOG_LEVEL_INFO,
					 "%s has no icon",
					 cra_app_get_id_full (app));
			continue;
		}

		tmp = cra_app_to_string (app);
		cra_package_log (task->pkg, CRA_PACKAGE_LOG_LEVEL_NONE, "%s", tmp);
		g_free (tmp);
	}

	/* delete tree */
	ret = cra_utils_rmtree (task->tmpdir, &error);
	if (!ret) {
		cra_package_log (task->pkg,
				 CRA_PACKAGE_LOG_LEVEL_WARNING,
				 "Failed to delete tree: %s",
				 error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_list_foreach (apps, (GFunc) g_object_unref, NULL);
	g_list_free (apps);
	cra_task_free (task);
}

/**
 * cra_task_sort_cb:
 */
static gint
cra_task_sort_cb (gconstpointer a, gconstpointer b, gpointer user_data)
{
	CraTask *task_a = (CraTask *) a;
	CraTask *task_b = (CraTask *) b;
	return g_strcmp0 (task_a->filename, task_b->filename);
}

/**
 * cra_context_add_filename:
 */
static gboolean
cra_context_add_filename (CraContext *ctx, const gchar *filename, GError **error)
{
	CraPackage *pkg;
	gboolean ret;

	/* open */
	pkg = cra_package_new ();
	ret = cra_package_open (pkg, filename, error);
	if (!ret)
		goto out;

	/* is package name blacklisted */
	if (cra_glob_value_search (ctx->blacklisted_pkgs,
				   cra_package_get_name (pkg)) != NULL) {
		cra_package_log (pkg,
				 CRA_PACKAGE_LOG_LEVEL_INFO,
				 "%s is blacklisted",
				 cra_package_get_filename (pkg));
		goto out;
	}

	/* add to array */
	g_ptr_array_add (ctx->packages, g_object_ref (pkg));
out:
	g_object_unref (pkg);
	return ret;
}

/**
 * main:
 */
int
main (int argc, char **argv)
{
	const gchar *filename;
	const gint max_threads = 1;
	CraContext *ctx = NULL;
	CraPackage *pkg;
	CraTask *task;
	gboolean ret;
	gboolean verbose = FALSE;
	gchar *buildone = NULL;
	gchar *log_dir = NULL;
	gchar *packages_dir = NULL;
	gchar *temp_dir = NULL;
	gchar *tmp;
	GDir *dir = NULL;
	GError *error = NULL;
	GOptionContext *option_context;
	GThreadPool *pool;
	guint i;
	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			"Show extra debugging information", NULL },
		{ "log-dir", '\0', 0, G_OPTION_ARG_STRING, &log_dir,
			"Set the logging directory", NULL },
		{ "packages-dir", '\0', 0, G_OPTION_ARG_STRING, &packages_dir,
			"Set the packages directory", NULL },
		{ "temp-dir", '\0', 0, G_OPTION_ARG_STRING, &temp_dir,
			"Set the temporary directory", NULL },
		{ "buildone", '\0', 0, G_OPTION_ARG_STRING, &buildone,
			"Set the temporary directory", NULL },
		{ NULL}
	};

	option_context = g_option_context_new (NULL);
	g_option_context_add_main_entries (option_context, options, NULL);
	ret = g_option_context_parse (option_context, &argc, &argv, &error);
	if (!ret) {
		g_print ("Failed to parse arguments: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	if (verbose)
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

	/* set defaults */
	if (packages_dir == NULL)
		packages_dir = g_strdup ("./packages");
	if (temp_dir == NULL)
		temp_dir = g_strdup ("./tmp");
	if (log_dir == NULL)
		log_dir = g_strdup ("./logs");

	rpmReadConfigFiles (NULL, NULL);
	setlocale (LC_ALL, "");

	/* set up state */
	ret = cra_utils_ensure_exists_and_empty (temp_dir, &error);
	if (!ret) {
		g_warning ("failed to create temp dir: %s", error->message);
		g_error_free (error);
		goto out;
	}
	ret = cra_utils_ensure_exists_and_empty (log_dir, &error);
	if (!ret) {
		g_warning ("failed to create log dir: %s", error->message);
		g_error_free (error);
		goto out;
	}
	ret = cra_utils_ensure_exists_and_empty ("./icons", &error);
	if (!ret) {
		g_warning ("failed to create icons dir: %s", error->message);
		g_error_free (error);
		goto out;
	}

	ctx = cra_context_new ();
	ret = cra_plugin_loader_setup (ctx->plugins, &error);
	if (!ret) {
		g_warning ("failed to set up plugins: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* create thread pool */
	pool = g_thread_pool_new (cra_task_process_func,
				  ctx,
				  max_threads,
				  TRUE,
				  &error);
	if (pool == NULL) {
		g_warning ("failed to set up pool: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* sort by name */
	g_thread_pool_set_sort_function (pool, cra_task_sort_cb, NULL);

	/* scan each package */
	if (buildone == NULL) {
		g_debug ("Scanning packages");
		dir = g_dir_open (packages_dir, 0, &error);
		if (dir == NULL) {
			g_warning ("failed to open packages: %s", error->message);
			g_error_free (error);
			goto out;
		}
		while ((filename = g_dir_read_name (dir)) != NULL) {
			tmp = g_build_filename (packages_dir, filename, NULL);
			ret = cra_context_add_filename (ctx, tmp, &error);
			if (!ret) {
				g_warning ("Failed to open package %s: %s",
					   tmp, error->message);
				g_error_free (error);
				goto out;
			}
			g_free (tmp);
		}
	} else {
		ret = cra_context_add_filename (ctx, buildone, &error);
	}

	/* add each package */
	g_debug ("Processing packages");
	for (i = 0; i < ctx->packages->len; i++) {
		pkg = g_ptr_array_index (ctx->packages, i);

		/* create task */
		task = g_new0 (CraTask, 1);
		task->filename = g_strdup (cra_package_get_filename (pkg));
		task->tmpdir = g_build_filename (temp_dir, cra_package_get_name (pkg), NULL);
		task->pkg = g_object_ref (pkg);

		/* add task to pool */
		ret = g_thread_pool_push (pool, task, &error);
		if (!ret) {
			cra_package_log (task->pkg,
					 CRA_PACKAGE_LOG_LEVEL_WARNING,
					 "failed to set up pool: %s",
					 error->message);
			g_error_free (error);
			goto out;
		}
	}

	/* wait for them to finish */
	if (pool != NULL)
		g_thread_pool_free (pool, FALSE, TRUE);

	/* success */
	g_debug ("Done!");
out:
	g_free (buildone);
	g_free (packages_dir);
	g_free (temp_dir);
	g_free (log_dir);
	g_option_context_free (option_context);
	if (ctx != NULL)
		cra_context_free (ctx);
	if (dir != NULL)
		g_dir_close (dir);
	return 0;
}
