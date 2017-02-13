/*
 * Gstreamer Daemon - Gst Launch under steroids
 * Copyright (C) 2015 RidgeRun Engineering <support@ridgerun.com>
 *
 * This file is part of Gstd.
 *
 * Gstd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Gstd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Gstd.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gstd_pipeline_deleter.h"
#include "gstd_pipeline.h"


#include <gobject/gvaluecollector.h>
#include <gst/gst.h>
#include <string.h>

#include "gstd_list.h"
#include "gstd_object.h"


/* Gstd Core debugging category */
GST_DEBUG_CATEGORY_STATIC (gstd_pipeline_deleter_debug);
#define GST_CAT_DEFAULT gstd_pipeline_deleter_debug

#define GSTD_DEBUG_DEFAULT_LEVEL GST_LEVEL_INFO

static void gstd_pipeline_deleter_delete (GstdIDeleter * iface,
    GstdObject * object);

typedef struct _GstdPipelineDeleterClass GstdPipelineDeleterClass;

/**
 * GstdPipelineDeleter:
 * A wrapper for the conventional pipeline_deleter
 */
struct _GstdPipelineDeleter
{
  GObject parent;
};

struct _GstdPipelineDeleterClass
{
  GObjectClass parent_class;
};


static void
gstd_ideleter_interface_init (GstdIDeleterInterface * iface)
{
  iface->delete = gstd_pipeline_deleter_delete;
}

G_DEFINE_TYPE_WITH_CODE (GstdPipelineDeleter, gstd_pipeline_deleter,
    G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (GSTD_TYPE_IDELETER,
        gstd_ideleter_interface_init));

static void
gstd_pipeline_deleter_class_init (GstdPipelineDeleterClass * klass)
{
  guint debug_color;

  /* Initialize debug category with nice colors */
  debug_color = GST_DEBUG_FG_BLACK | GST_DEBUG_BOLD | GST_DEBUG_BG_WHITE;
  GST_DEBUG_CATEGORY_INIT (gstd_pipeline_deleter_debug, "gstdpipelinedeleter",
      debug_color, "Gstd Pipeline Deleter category");
}

static void
gstd_pipeline_deleter_init (GstdPipelineDeleter * self)
{
  GST_INFO_OBJECT (self, "Initializing pipeline deleter");
}

static void
gstd_pipeline_deleter_delete (GstdIDeleter * iface, GstdObject * object)
{
  g_return_if_fail (iface);

  /* Stop the pipe if playing */
  gstd_object_update (GSTD_OBJECT (object), "state", GSTD_PIPELINE_NULL, NULL);

  g_object_unref (object);
}