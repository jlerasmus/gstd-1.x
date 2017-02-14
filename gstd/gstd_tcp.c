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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <gst/gst.h>

#include "gstd_ipc.h"
#include "gstd_tcp.h"
#include "gstd_element.h"

/* Gstd TCP debugging category */
GST_DEBUG_CATEGORY_STATIC (gstd_tcp_debug);
#define GST_CAT_DEFAULT gstd_tcp_debug

#define GSTD_DEBUG_DEFAULT_LEVEL GST_LEVEL_INFO

struct _GstdTcp
{
  GstdIpc parent;
  guint16 port;
  GSocketService *service;
};

struct _GstdTcpClass
{
  GstdIpcClass parent_class;
};


typedef GstdReturnCode GstdTCPFunc (GstdSession *, gchar *, gchar *, gchar **);
typedef struct _GstdTCPCmd
{
  gchar *cmd;
  GstdTCPFunc *callback;
} GstdTCPCmd;


enum
{
  PROP_PORT = 1,
  N_PROPERTIES                  // NOT A PROPERTY
};

G_DEFINE_TYPE (GstdTcp, gstd_tcp, GSTD_TYPE_IPC)

/* VTable */
     static gboolean
         gstd_tcp_callback (GSocketService * service,
    GSocketConnection * connection,
    GObject * source_object, gpointer user_data);
     static GstdReturnCode
         gstd_tcp_parse_cmd (GstdSession * session, const gchar * cmd,
    gchar ** response);
     static GstdReturnCode gstd_tcp_parse_raw_cmd (GstdSession * session,
    gchar * action, gchar * args, gchar ** response);
     static GstdReturnCode gstd_tcp_create (GstdSession * session,
    GstdObject * obj, gchar * args, gchar ** response);
     static GstdReturnCode gstd_tcp_read (GstdSession * session,
    GstdObject * obj, gchar * args, gchar ** reponse);
     static GstdReturnCode gstd_tcp_update_by_type (GstdSession * session,
    GstdObject * obj, gchar * args, gchar ** response);
     static GstdReturnCode gstd_tcp_delete (GstdSession * session,
    GstdObject * obj, gchar * args, gchar ** response);
     static GstdReturnCode gstd_tcp_pipeline_create (GstdSession *, gchar *,
    gchar *, gchar **);
     static GstdReturnCode gstd_tcp_pipeline_delete (GstdSession *, gchar *,
    gchar *, gchar **);
     static GstdReturnCode gstd_tcp_pipeline_play (GstdSession *, gchar *,
    gchar *, gchar **);
     static GstdReturnCode gstd_tcp_pipeline_pause (GstdSession *, gchar *,
    gchar *, gchar **);
     static GstdReturnCode gstd_tcp_pipeline_stop (GstdSession *, gchar *,
    gchar *, gchar **);
     static GstdReturnCode gstd_tcp_element_set (GstdSession *, gchar *,
    gchar *, gchar **);
     static GstdReturnCode gstd_tcp_element_get (GstdSession *, gchar *,
    gchar *, gchar **);
     static GstdReturnCode gstd_tcp_list_pipelines (GstdSession *, gchar *,
    gchar *, gchar **);
     static GstdReturnCode gstd_tcp_list_elements (GstdSession *, gchar *,
    gchar *, gchar **);
     static GstdReturnCode gstd_tcp_list_properties (GstdSession *, gchar *,
    gchar *, gchar **);
     static void gstd_tcp_set_property (GObject *, guint, const GValue *,
    GParamSpec *);
     static void gstd_tcp_get_property (GObject *, guint, GValue *,
    GParamSpec *);
     static void gstd_tcp_dispose (GObject *);
gboolean
gstd_tcp_init_get_option_group (GstdIpc * base, GOptionGroup ** group);

     static GstdTCPCmd cmds[] = {
       {"create", gstd_tcp_parse_raw_cmd},
       {"read", gstd_tcp_parse_raw_cmd},
       {"update", gstd_tcp_parse_raw_cmd},
       {"delete", gstd_tcp_parse_raw_cmd},

       {"pipeline_create", gstd_tcp_pipeline_create},
       {"pipeline_delete", gstd_tcp_pipeline_delete},
       {"pipeline_play", gstd_tcp_pipeline_play},
       {"pipeline_pause", gstd_tcp_pipeline_pause},
       {"pipeline_stop", gstd_tcp_pipeline_stop},

       {"element_set", gstd_tcp_element_set},
       {"element_get", gstd_tcp_element_get},

       {"list_pipelines", gstd_tcp_list_pipelines},
       {"list_elements", gstd_tcp_list_elements},
       {"list_properties", gstd_tcp_list_properties},
       {NULL}
     };

static void
gstd_tcp_class_init (GstdTcpClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstdIpcClass *gstdipc_class = GSTD_IPC_CLASS (klass);
  GParamSpec *properties[N_PROPERTIES] = { NULL, };
  guint debug_color;
  object_class->set_property = gstd_tcp_set_property;
  object_class->get_property = gstd_tcp_get_property;
  gstdipc_class->start = GST_DEBUG_FUNCPTR (gstd_tcp_start);
  gstdipc_class->stop = GST_DEBUG_FUNCPTR (gstd_tcp_stop);
  gstdipc_class->get_option_group =
      GST_DEBUG_FUNCPTR (gstd_tcp_init_get_option_group);
  object_class->dispose = gstd_tcp_dispose;

  properties[PROP_PORT] =
      g_param_spec_uint ("port",
      "Port",
      "The port to start listening to",
      0,
      G_MAXINT,
      GSTD_TCP_DEFAULT_PORT,
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | GSTD_PARAM_READ);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /* Initialize debug category with nice colors */
  debug_color = GST_DEBUG_FG_BLACK | GST_DEBUG_BOLD | GST_DEBUG_BG_WHITE;
  GST_DEBUG_CATEGORY_INIT (gstd_tcp_debug, "gstdtcp", debug_color,
      "Gstd TCP category");
}

static void
gstd_tcp_init (GstdTcp * self)
{
  GST_INFO_OBJECT (self, "Initializing gstd Tcp");
  GstdIpc *base = GSTD_IPC (self);
  self->port = GSTD_TCP_DEFAULT_PORT;
  self->service = NULL;
  base->enabled = FALSE;
}

static void
gstd_tcp_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec)
{
  GstdTcp *self = GSTD_TCP (object);

  gstd_object_set_code (GSTD_OBJECT (self), GSTD_EOK);

  switch (property_id) {
    case PROP_PORT:
      GST_DEBUG_OBJECT (self, "Returning post %u", self->port);
      g_value_set_uint (value, self->port);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      gstd_object_set_code (GSTD_OBJECT (self), GSTD_NO_RESOURCE);
      break;
  }
}

static void
gstd_tcp_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec)
{
  GstdTcp *self = GSTD_TCP (object);

  gstd_object_set_code (GSTD_OBJECT (self), GSTD_EOK);

  switch (property_id) {
    case PROP_PORT:
      GST_DEBUG_OBJECT (self, "Changing port to %u", self->port);
      self->port = g_value_get_uint (value);
      GST_DEBUG_OBJECT (self, "Value changed %u", self->port);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      gstd_object_set_code (GSTD_OBJECT (self), GSTD_NO_RESOURCE);
      break;
  }
}


static void
gstd_tcp_dispose (GObject * object)
{
  GstdTcp *self = GSTD_TCP (object);

  GST_INFO_OBJECT (object, "Deinitializing gstd TCP");

  if (self->service) {
    self->service = NULL;
  }

  G_OBJECT_CLASS (gstd_tcp_parent_class)->dispose (object);
}



static gboolean
gstd_tcp_callback (GSocketService * service,
    GSocketConnection * connection, GObject * source_object, gpointer user_data)
{

  GstdSession *session = GSTD_SESSION (user_data);
  GInputStream *istream;
  GOutputStream *ostream;
  gint read;
  const guint size = 1024 * 1024;
  gchar *output = NULL;
  gchar *response;
  gchar message[size];
  GstdReturnCode ret;

  g_return_val_if_fail (session, TRUE);

  istream = g_io_stream_get_input_stream (G_IO_STREAM (connection));
  ostream = g_io_stream_get_output_stream (G_IO_STREAM (connection));

  read = g_input_stream_read (istream, message, size, NULL, NULL);
  message[read] = '\0';

  ret = gstd_tcp_parse_cmd (session, message, &output);

  /* Prepend the code to the output */
  response =
      g_strdup_printf ("{\n  code : %d\n  response : %s\n}", ret, output);
  g_free (output);

  g_output_stream_write (ostream, response, size, NULL, NULL);
  g_free (response);
  return FALSE;
}

GstdReturnCode
gstd_tcp_start (GstdIpc * base, GstdSession * session)
{
  guint debug_color;
  GError *error = NULL;
  GstdTcp *self = GSTD_TCP (base);
  GSocketService **service = &self->service;
  guint16 port = self->port;

  if (!base->enabled) {
    GST_DEBUG_OBJECT (self, "TCP not enabled, skipping");
    goto out;
  }

  GST_DEBUG_OBJECT (self, "Starting TCP");

  if (!gstd_tcp_debug) {
    /* Initialize debug category with nice colors */
    debug_color = GST_DEBUG_FG_BLACK | GST_DEBUG_BOLD | GST_DEBUG_BG_WHITE;
    GST_DEBUG_CATEGORY_INIT (gstd_tcp_debug, "gstdtcp", debug_color,
        "Gstd TCP category");
  }
  // Close any existing connection
  gstd_tcp_stop (base);

  *service = g_socket_service_new ();

  g_socket_listener_add_inet_port (G_SOCKET_LISTENER (*service),
      port, NULL /* G_OBJECT(session) */ , &error);
  if (error)
    goto noconnection;

  /* listen to the 'incoming' signal */
  g_signal_connect (*service,
      "incoming", G_CALLBACK (gstd_tcp_callback), session);

  /* start the socket service */
  g_socket_service_start (*service);

out:
  return GSTD_EOK;

noconnection:
  {
    GST_ERROR_OBJECT (session, "%s", error->message);
    g_error_free (error);
    return GSTD_NO_CONNECTION;
  }
}

GstdReturnCode
gstd_tcp_stop (GstdIpc * base)
{
  GstdTcp *self = GSTD_TCP (base);
  GSocketService **service = &self->service;
  GstdSession *session = base->session;
  GSocketListener *listener = G_SOCKET_LISTENER (*service);


  g_return_val_if_fail (session, GSTD_NULL_ARGUMENT);

  GST_DEBUG_OBJECT (self, "Entering TCP stop ");

  if (*service) {
    GST_INFO_OBJECT (session, "Closing TCP connection for %s",
        GSTD_OBJECT_NAME (session));
    g_socket_listener_close (listener);
    g_socket_service_stop (*service);
    g_object_unref (*service);
    *service = NULL;
  }

  return GSTD_EOK;
}

static gboolean
gstd_tcp_is_num (const gchar * str)
{
  for (; *str != '\0'; str++) {
    if (!g_ascii_isdigit (*str))
      return FALSE;
  }
  return TRUE;
}

static GstdReturnCode
gstd_tcp_create (GstdSession * session, GstdObject * obj, gchar * args,
    gchar ** response)
{
  gchar **tokens;
  gchar *name;
  gchar *description;
  GstdObject *new;
  GstdReturnCode ret;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (GSTD_IS_OBJECT (obj), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (args, GSTD_NULL_ARGUMENT);

  // This may mean a potential leak
  g_warn_if_fail (!*response);

  GST_FIXME_OBJECT (session,
      "Currently hardcoded to create pipelines, we must be "
      "generic enough to create any type of object");

  // Tokens has the form {'name', <name>, 'description', <description>}
  tokens = g_strsplit (args, " ", 4);
  name = tokens[1];
  description = tokens[3];

  if (!name || name[0] == '\0')
    goto noname;

  if (!description || description[0] == '\0')
    goto nodescription;

  ret = gstd_object_create (obj, name, description);
  if (ret)
    goto noobject;

  gstd_object_read (obj, name, &new, NULL);
  gstd_object_to_string (new, response);
  g_object_unref (new);

  return ret;

noname:
  {
    GST_ERROR_OBJECT (session, "Missing name for the new pipeline");
    g_strfreev (tokens);
    return GSTD_NULL_ARGUMENT;
  }
nodescription:
  {
    GST_ERROR_OBJECT (session, "Missing description for pipeline \"%s\"", name);
    g_strfreev (tokens);
    return GSTD_NULL_ARGUMENT;
  }
noobject:
  {
    g_strfreev (tokens);
    return ret;
  }
}

static GstdReturnCode
gstd_tcp_read (GstdSession * session, GstdObject * obj, gchar * args,
    gchar ** response)
{
  gchar **tokens;
  GParamSpec *pspec;
  GObject *properties;
  GValue value = G_VALUE_INIT;
  gchar *svalue;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (GSTD_IS_OBJECT (obj), GSTD_NULL_ARGUMENT);

  // This may mean a potential leak
  g_warn_if_fail (!*response);

  // Print the raw object
  if (!args)
    return gstd_object_to_string (obj, response);

  tokens = g_strsplit (args, " ", -1);

  // Print the property
  /* If its a GstdElement element we need to parse the pspec from
     the internal element */
  if (GSTD_IS_ELEMENT (obj))
    gstd_object_read (obj, "gstelement", &properties, NULL);
  else
    properties = G_OBJECT (obj);

  pspec =
      g_object_class_find_property (G_OBJECT_GET_CLASS (properties), tokens[0]);
  if (!pspec)
    goto noprop;

  /* Automagical type value serialization */
  g_value_init (&value, pspec->value_type);
  g_object_get_property (G_OBJECT (properties), tokens[0], &value);
  svalue = g_strdup_value_contents (&value);
  g_value_unset (&value);

  *response = g_strdup_printf ("{\n    %s : %s\n  }", tokens[0], svalue);
  g_free (svalue);

  return GSTD_EOK;

noprop:
  {
    GST_ERROR_OBJECT (session, "Unexisting property \"%s\" in %s",
        tokens[0], GSTD_OBJECT_NAME (obj));
    return GSTD_BAD_COMMAND;
  }
}

static GstdReturnCode
gstd_tcp_update_by_type (GstdSession * session, GstdObject * obj, gchar * args,
    gchar ** response)
{
  gchar **tokens;
  gchar **property;
  gchar *prop;
  gchar *svalue;
  GParamSpec *pspec;
  GObject *properties;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (GSTD_IS_OBJECT (obj), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (args, GSTD_NULL_ARGUMENT);

  *response = NULL;

  tokens = g_strsplit (args, " ", -1);
  property = tokens;
  while (*property) {
    prop = *property++;
    svalue = *property++;

    if (!*svalue)
      goto novalue;

    /* If its a GstdElement element we need to parse the pspec from
       the internal element */
    if (GSTD_IS_ELEMENT (obj))
      gstd_object_read (obj, "gstelement", &properties, NULL);
    else
      properties = G_OBJECT (obj);

    pspec =
        g_object_class_find_property (G_OBJECT_GET_CLASS (properties), prop);
    if (!pspec)
      goto noprop;

    if (G_TYPE_CHAR == pspec->value_type ||
        G_TYPE_UCHAR == pspec->value_type ||
        G_TYPE_STRING == pspec->value_type) {
      return gstd_object_update (obj, prop, svalue, NULL);
    }

    if (G_TYPE_INT == pspec->value_type) {
      gint d;
      sscanf (svalue, "%d", &d);
      return gstd_object_update (obj, prop, d, NULL);
    }

    if (G_TYPE_UINT == pspec->value_type) {
      guint u;
      sscanf (svalue, "%u", &u);
      return gstd_object_update (obj, prop, u, NULL);
    }

    if (G_TYPE_FLOAT == pspec->value_type) {
      gfloat f;
      sscanf (svalue, "%f", &f);
      return gstd_object_update (obj, prop, f, NULL);
    }

    if (G_TYPE_DOUBLE == pspec->value_type) {
      gdouble lf;
      sscanf (svalue, "%lf", &lf);
      return gstd_object_update (obj, prop, lf, NULL);
    }

    if (G_TYPE_BOOLEAN == pspec->value_type) {
      gboolean b;
      if (!g_ascii_strcasecmp (svalue, "true"))
        b = TRUE;
      else if (!g_ascii_strcasecmp (svalue, "false"))
        b = FALSE;
      else
        goto badboolean;

      return gstd_object_update (obj, prop, b, NULL);
    }

    /* Complex types so  we can refer to the enum value as
       their names rather than ugly numbers */
    if (G_TYPE_IS_ENUM (pspec->value_type)) {
      GEnumClass *c = g_type_class_ref (pspec->value_type);
      GEnumValue *e;
      gint d;

      /* Try by name */
      e = g_enum_get_value_by_name (c, svalue);
      if (e)
        return gstd_object_update (obj, prop, e->value, NULL);

      /* Try by nick */
      e = g_enum_get_value_by_nick (c, svalue);
      if (e)
        return gstd_object_update (obj, prop, e->value, NULL);

      /* Try by integer */
      if (gstd_tcp_is_num (svalue)) {
        sscanf (svalue, "%d", &d);
        return gstd_object_update (obj, prop, d, NULL);
      }

      /* Unknown! */
      goto unknown;
    }

    /* Complex types so  we can refer to the flag value as
       their names rather than ugly numbers */
    if (G_TYPE_IS_FLAGS (pspec->value_type)) {
      GFlagsClass *c = g_type_class_ref (pspec->value_type);
      GFlagsValue *e;
      gint d;

      /* Try by name */
      e = g_flags_get_value_by_name (c, svalue);
      if (e)
        return gstd_object_update (obj, prop, e->value, NULL);

      /* Try by nick */
      e = g_flags_get_value_by_nick (c, svalue);
      if (e)
        return gstd_object_update (obj, prop, e->value, NULL);

      /* Try by integer */
      if (gstd_tcp_is_num (svalue)) {
        sscanf (svalue, "%d", &d);
        return gstd_object_update (obj, prop, d, NULL);
      }

      /* Unknown! */
      goto unknown;
    }

    GST_ERROR_OBJECT (session, "Unable to handle \"%s\" types",
        g_type_name (pspec->value_type));
    g_strfreev (tokens);
    return GSTD_BAD_COMMAND;
  }

novalue:
  {
    GST_ERROR_OBJECT (session, "Missing value for property");
    g_strfreev (tokens);
    return GSTD_BAD_COMMAND;
  }
noprop:
  {
    GST_ERROR_OBJECT (session, "Unexisting property \"%s\" in %s",
        prop, GSTD_OBJECT_NAME (obj));
    g_strfreev (tokens);
    return GSTD_BAD_COMMAND;
  }
unknown:
  {
    GST_ERROR_OBJECT (session, "Invalid enum/flags value \"%s\"", svalue);
    g_strfreev (tokens);
    return GSTD_BAD_VALUE;
  }
badboolean:
  {
    GST_ERROR_OBJECT (session, "Invalid boolean value \"%s\"", svalue);
    g_strfreev (tokens);
    return GSTD_BAD_VALUE;
  }
}

static GstdReturnCode
gstd_tcp_delete (GstdSession * session, GstdObject * obj, gchar * args,
    gchar ** response)
{
  gchar **tokens;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (GSTD_IS_OBJECT (obj), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (args, GSTD_NULL_ARGUMENT);

  *response = NULL;

  tokens = g_strsplit (args, " ", -1);
  return gstd_object_delete (obj, tokens[0]);
}

static GstdReturnCode
gstd_tcp_parse_raw_cmd (GstdSession * session, gchar * action, gchar * args,
    gchar ** response)
{
  gchar **tokens;
  gchar *uri, *rest;
  GstdObject *node;
  GstdReturnCode ret;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (action, GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (args, GSTD_NULL_ARGUMENT);
  g_warn_if_fail (!*response);


  tokens = g_strsplit (args, " ", 2);
  uri = tokens[0];
  rest = tokens[1];

  // Alias the empty string to the base
  if (!uri)
    uri = "/";

  if (gstd_get_by_uri (session, uri, &node))
    goto nonode;

  if (!g_ascii_strcasecmp ("CREATE", action)) {
    ret = gstd_tcp_create (session, node, rest, response);
  } else if (!g_ascii_strcasecmp ("READ", action)) {
    ret = gstd_tcp_read (session, node, rest, response);
  } else if (!g_ascii_strcasecmp ("UPDATE", action)) {
    ret = gstd_tcp_update_by_type (session, node, rest, response);
    gstd_tcp_read (session, node, rest, response);
  } else if (!g_ascii_strcasecmp ("DELETE", action)) {
    ret = gstd_tcp_delete (session, node, rest, response);
  } else
    goto badcommand;

  g_object_unref (node);
  return ret;

nonode:
  {
    GST_ERROR_OBJECT (session, "Malformed URI \"%s\"", uri);
    g_strfreev (tokens);
    return GSTD_NO_RESOURCE;
  }
badcommand:
  {
    GST_ERROR_OBJECT (session, "Unknown command \"%s\"", action);
    g_strfreev (tokens);
    g_object_unref (node);
    return GSTD_BAD_COMMAND;
  }
}

static GstdReturnCode
gstd_tcp_parse_cmd (GstdSession * session, const gchar * cmd, gchar ** response)
{
  gchar **tokens;
  gchar *action, *args;
  GstdTCPCmd *cb;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (cmd, GSTD_NULL_ARGUMENT);
  g_warn_if_fail (!*response);

  tokens = g_strsplit (cmd, " ", 2);
  action = tokens[0];
  args = tokens[1];

  cb = cmds;
  while (cb) {
    if (!g_ascii_strcasecmp (cb->cmd, action)) {
      return cb->callback (session, action, args, response);
    }
    cb++;
  }

  GST_ERROR_OBJECT (session, "Unknown command \"%s\"", action);
  g_strfreev (tokens);

  return GSTD_BAD_COMMAND;
}

static GstdReturnCode
gstd_tcp_pipeline_create (GstdSession * session, gchar * action, gchar * args,
    gchar ** response)
{
  GstdReturnCode ret;
  gchar *uri;
  gchar **tokens;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (args, GSTD_NULL_ARGUMENT);

  tokens = g_strsplit (args, " ", 2);
  g_return_val_if_fail (tokens[0], GSTD_BAD_COMMAND);
  g_return_val_if_fail (tokens[1], GSTD_BAD_COMMAND);

  uri =
      g_strdup_printf ("/pipelines name %s description %s", tokens[0],
      tokens[1]);

  ret = gstd_tcp_parse_raw_cmd (session, "create", uri, response);

  g_free (uri);
  g_strfreev (tokens);

  return ret;
}

static GstdReturnCode
gstd_tcp_pipeline_delete (GstdSession * session, gchar * action, gchar * args,
    gchar ** response)
{
  GstdReturnCode ret;
  gchar *uri;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (args, GSTD_NULL_ARGUMENT);

  uri = g_strdup_printf ("/pipelines %s", args);
  ret = gstd_tcp_parse_raw_cmd (session, "delete", uri, response);
  g_free (uri);

  return ret;
}

static GstdReturnCode
gstd_tcp_pipeline_play (GstdSession * session, gchar * action, gchar * args,
    gchar ** response)
{
  GstdReturnCode ret;
  gchar *uri;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (args, GSTD_NULL_ARGUMENT);

  uri = g_strdup_printf ("/pipelines/%s state playing", args);
  ret = gstd_tcp_parse_raw_cmd (session, "update", uri, response);
  g_free (uri);

  return ret;
}

static GstdReturnCode
gstd_tcp_pipeline_pause (GstdSession * session, gchar * action, gchar * args,
    gchar ** response)
{
  GstdReturnCode ret;
  gchar *uri;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (args, GSTD_NULL_ARGUMENT);

  uri = g_strdup_printf ("/pipelines/%s state paused", args);
  ret = gstd_tcp_parse_raw_cmd (session, "update", uri, response);
  g_free (uri);

  return ret;
}

static GstdReturnCode
gstd_tcp_pipeline_stop (GstdSession * session, gchar * action, gchar * args,
    gchar ** response)
{
  GstdReturnCode ret;
  gchar *uri;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (args, GSTD_NULL_ARGUMENT);

  uri = g_strdup_printf ("/pipelines/%s state null", args);
  ret = gstd_tcp_parse_raw_cmd (session, "update", uri, response);
  g_free (uri);

  return ret;
}

static GstdReturnCode
gstd_tcp_element_set (GstdSession * session, gchar * action, gchar * args,
    gchar ** response)
{
  GstdReturnCode ret;
  gchar *uri;
  gchar **tokens;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (args, GSTD_NULL_ARGUMENT);

  tokens = g_strsplit (args, " ", 4);
  g_return_val_if_fail (tokens[0], GSTD_BAD_COMMAND);
  g_return_val_if_fail (tokens[1], GSTD_BAD_COMMAND);
  g_return_val_if_fail (tokens[2], GSTD_BAD_COMMAND);
  g_return_val_if_fail (tokens[3], GSTD_BAD_COMMAND);

  uri = g_strdup_printf ("/pipelines/%s/elements/%s %s %s",
      tokens[0], tokens[1], tokens[2], tokens[3]);
  ret = gstd_tcp_parse_raw_cmd (session, "update", uri, response);

  g_free (uri);
  g_strfreev (tokens);

  return ret;
}

static GstdReturnCode
gstd_tcp_element_get (GstdSession * session, gchar * action, gchar * args,
    gchar ** response)
{
  GstdReturnCode ret;
  gchar *uri;
  gchar **tokens;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (args, GSTD_NULL_ARGUMENT);

  tokens = g_strsplit (args, " ", 3);
  g_return_val_if_fail (tokens[0], GSTD_BAD_COMMAND);
  g_return_val_if_fail (tokens[1], GSTD_BAD_COMMAND);
  g_return_val_if_fail (tokens[2], GSTD_BAD_COMMAND);

  uri = g_strdup_printf ("/pipelines/%s/elements/%s %s",
      tokens[0], tokens[1], tokens[2]);
  ret = gstd_tcp_parse_raw_cmd (session, "read", uri, response);

  g_free (uri);
  g_strfreev (tokens);

  return ret;
}

static GstdReturnCode
gstd_tcp_list_pipelines (GstdSession * session, gchar * action, gchar * args,
    gchar ** response)
{
  GstdReturnCode ret;
  gchar *uri;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);

  uri = g_strdup_printf ("/pipelines");
  ret = gstd_tcp_parse_raw_cmd (session, "read", uri, response);
  g_free (uri);

  return ret;
}

static GstdReturnCode
gstd_tcp_list_elements (GstdSession * session, gchar * action, gchar * args,
    gchar ** response)
{
  GstdReturnCode ret;
  gchar *uri;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (args, GSTD_NULL_ARGUMENT);

  uri = g_strdup_printf ("/pipelines/%s/elements/", args);
  ret = gstd_tcp_parse_raw_cmd (session, "read", uri, response);
  g_free (uri);

  return ret;
}

static GstdReturnCode
gstd_tcp_list_properties (GstdSession * session, gchar * action, gchar * args,
    gchar ** response)
{
  GstdReturnCode ret;
  gchar *uri;
  gchar **tokens;

  g_return_val_if_fail (GSTD_IS_SESSION (session), GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (args, GSTD_NULL_ARGUMENT);

  tokens = g_strsplit (args, " ", 2);
  g_return_val_if_fail (tokens[0], GSTD_BAD_COMMAND);
  g_return_val_if_fail (tokens[1], GSTD_BAD_COMMAND);

  uri = g_strdup_printf ("/pipelines/%s/elements/%s", tokens[0], tokens[1]);
  ret = gstd_tcp_parse_raw_cmd (session, "read", uri, response);

  g_free (uri);
  g_strfreev (tokens);

  return ret;
}


gboolean
gstd_tcp_init_get_option_group (GstdIpc * base, GOptionGroup ** group)
{
  GstdTcp *self = GSTD_TCP (base);
  GST_DEBUG_OBJECT (self, "TCP init group callback ");
  GOptionEntry tcp_args[] = {
    {"enable-tcp-protocol", 't', 0, G_OPTION_ARG_NONE, &base->enabled,
        "Enable attach the server through a given TCP port ", NULL}
    ,
    {"port", 'p', 0, G_OPTION_ARG_INT, &self->port,
        "Attach to the server through the given port (default 5000)", "port"}
    ,
    {NULL}
  };
  *group = g_option_group_new ("gstd-tcp", ("TCP Options"),
      ("Show TCP Options"), NULL, NULL);

  g_option_group_add_entries (*group, tcp_args);
  return TRUE;
}
