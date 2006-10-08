/*
 * Copyright (c) 2003-2006 Andrea Luzzardi <scox@sig11.org>
 *
 * This file is part of the pam_usb project. pam_usb is free software;
 * you can redistribute it and/or modify it under the terms of the GNU General
 * Public License version 2, as published by the Free Software Foundation.
 *
 * pam_usb is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "conf.h"
#include "xpath.h"
#include "log.h"

static void	pusb_conf_options_get_from(t_pusb_options *opts,
					   const char *from,
					   xmlDoc *doc)
{
  pusb_xpath_get_string_from(doc, from, "option[@name='hostname']",
			     opts->hostname, sizeof(opts->hostname));
  pusb_xpath_get_string_from(doc, from, "option[@name='system_otp_directory']",
			     opts->system_otp_directory, sizeof(opts->system_otp_directory));
  pusb_xpath_get_string_from(doc, from, "option[@name='device_otp_directory']",
			     opts->device_otp_directory, sizeof(opts->device_otp_directory));
  pusb_xpath_get_bool_from(doc, from, "option[@name='debug']",
			   &(opts->debug));
  pusb_xpath_get_bool_from(doc, from, "option[@name='enable']",
			   &(opts->enable));
  pusb_xpath_get_bool_from(doc, from, "option[@name='try_otp']",
			   &(opts->try_otp));
  pusb_xpath_get_bool_from(doc, from, "option[@name='enforce_otp']",
			   &(opts->enforce_otp));
  pusb_xpath_get_int_from(doc, from, "option[@name='probe_timeout']",
			  &(opts->probe_timeout));
}

static int		pusb_conf_parse_options(t_pusb_options *opts,
						xmlDoc *doc,
						const char *user,
						const char *service)
{
  char			*xpath = NULL;
  size_t		xpath_size;
  int			i;
  struct s_opt_list	opt_list[] = {
    { CONF_DEVICE_XPATH, opts->device.name },
    { CONF_USER_XPATH, (char *)user },
    { CONF_SERVICE_XPATH, (char *)service },
    { NULL, NULL }
  };

  pusb_conf_options_get_from(opts, "//configuration/defaults/", doc);
  for (i = 0; opt_list[i].name != NULL; ++i)
    {
      xpath_size = strlen(opt_list[i].name) + strlen(opt_list[i].value) + 1;
      if (!(xpath = malloc(xpath_size)))
	{
	  log_error("malloc error\n");
	  return (0);
	}
      memset(xpath, 0x00, xpath_size);
      snprintf(xpath, xpath_size, opt_list[i].name, opt_list[i].value, "");
      pusb_conf_options_get_from(opts, xpath, doc);
      free(xpath);
    }
  return (1);
}

static int	pusb_conf_device_get_property(t_pusb_options *opts,
					      xmlDoc *doc,
					      const char *property,
					      char *store,
					      size_t size)
{
  char		*xpath = NULL;
  size_t	xpath_len;
  int		retval;

  xpath_len = strlen(CONF_DEVICE_XPATH) + strlen(opts->device.name) + \
    strlen(property) + 1;
  if (!(xpath = malloc(xpath_len)))
    {
      log_error("malloc error!\n");
      return (0);
    }
  memset(xpath, 0x00, xpath_len);
  snprintf(xpath, xpath_len, CONF_DEVICE_XPATH, opts->device.name,
	   property);
  retval = pusb_xpath_get_string(doc, xpath, store, size);
  free(xpath);
  return (retval);
}

static int	pusb_conf_parse_device(t_pusb_options *opts, xmlDoc *doc)
{
  log_debug("Parsing settings...\n");
  if (!pusb_conf_device_get_property(opts, doc, "vendor", opts->device.vendor,
				      sizeof(opts->device.vendor)))
    return (0);
  if (!pusb_conf_device_get_property(opts, doc, "model", opts->device.model,
				     sizeof(opts->device.model)))
    return (0);
  if (!pusb_conf_device_get_property(opts, doc, "serial", opts->device.serial,
				     sizeof(opts->device.serial)))
    return (0);
  return (1);
}

int	pusb_conf_init(t_pusb_options *opts)
{
  memset(opts, 0x00, sizeof(*opts));
  if (gethostname(opts->hostname, sizeof(opts->hostname)) == -1)
    {
      log_error("gethostname: %s\n", strerror(errno));
      return (0);
    }
  strcpy(opts->system_otp_directory, "./");
  strcpy(opts->device_otp_directory, ".auth");
  opts->probe_timeout = 10;
  opts->enable = 1;
  opts->try_otp = 1;
  opts->enforce_otp = 0;
  opts->debug = 0;
  return (1);
}

int		pusb_conf_parse(const char *file, t_pusb_options *opts,
				const char *user, const char *service)
{
  xmlDoc	*doc = NULL;
  int		retval;
  char		device_xpath[sizeof(CONF_USER_XPATH) + CONF_USER_MAXLEN + \
			     sizeof("device")];

  if (strlen(user) > CONF_USER_MAXLEN)
    {
      log_error("Username \"%s\" is too long (max: %d)\n", user,
		CONF_USER_MAXLEN);
      return (0);
    }
  if (!(doc = xmlReadFile(file, NULL, 0)))
    {
      log_error("Unable to parse \"%s\"\n", file);
      return (0);
    }
  snprintf(device_xpath, sizeof(device_xpath), CONF_USER_XPATH, user,
	   "device");
  retval = pusb_xpath_get_string(doc,
				 device_xpath,
				 opts->device.name,
				 sizeof(opts->device.name));
  if (!retval || !pusb_conf_parse_device(opts, doc))
    {
      log_error("No device found for user \"%s\"\n", user);
      xmlFreeDoc(doc);
      xmlCleanupParser();
      return (0);
    }
  if (!pusb_conf_parse_options(opts, doc, user, service))
    {
      xmlFreeDoc(doc);
      xmlCleanupParser();
      return (0);
    }
  xmlFreeDoc(doc);
  xmlCleanupParser();
  return (1);
}
