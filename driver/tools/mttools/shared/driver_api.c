/*
 * Copyright (c) 2007 Metalink Broadband (Israel)
 *
 * Driver API
 *
 */

#include "mtlkinc.h"

#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/if.h>
#include <linux/wireless.h>

#include "compat.h"
#include "mtlkinc.h"

#include "driver_api.h"
#include "dataex.h"

#define LOG_LOCAL_GID   GID_DRIVER_API
#define LOG_LOCAL_FID   1

#define DRVCTL_LOAD_PROGMODEL 1

typedef struct _network_interface
{
    char name[IFNAMSIZ];
} network_interface;

typedef struct _drv_ctl
{
    int32 cmd;
    int32 datalen;
} drv_ctl;

int gsocket = -1;

int gifcount = 0;
network_interface gifs[32];
int ifconnected = -1;

char itrfname[IFNAMSIZ];

// Taken from BCLSockServer
static int find_interfaces( void )
{
  FILE *f;
  char buffer[1024];
//  char ifname[IFNAMSIZ];
  char *tmp, *tmp2;
  struct iwreq req;

  f = fopen("/proc/net/dev", "r");
  if (!f) {
    // FIXME: make errors more verbose
    ELOG_S("fopen(): %s", strerror(errno));
    return -1;
  }

  // Skipping first two lines...
  if (!fgets(buffer, 1024, f)) {
    ELOG_S("fgets(): %s", strerror(errno));
    fclose(f);
    return -1;
  }

  if (!fgets(buffer, 1024, f)) {
    perror("fgets()");
    fclose(f);
    return -1;
  }

  // Reading interface descriptions...
  while (fgets(buffer, 1024, f)) {
    memset(itrfname, 0, IFNAMSIZ);

    // Skipping through leading space characters...
    tmp = buffer;
    while (*tmp == ' ') {
      tmp++;
    }

    // Getting interface name...
    tmp2 = strstr(tmp, ":");
    strncpy(itrfname, tmp,
      tmp2 - tmp > IFNAMSIZ ? IFNAMSIZ : tmp2 - tmp);

    ILOG1_S("Found network interface \"%s\"", itrfname);

    // Checking if interface supports wireless extensions...
    strncpy(req.ifr_name, itrfname, IFNAMSIZ);
    if (ioctl(gsocket, SIOCGIWNAME, &req)) {
      ILOG1_S("Interface \"%s\" has no wireless extensions...", itrfname);
      continue;
    } else {
      ILOG1_S("Interface \"%s\" has wireless extensions...", itrfname);
    }

    // Adding interface into our database...
    memcpy(gifs[gifcount].name, itrfname, IFNAMSIZ);
    gifcount++;
  }

  fclose(f);

  return 0;
}

int
driver_connected(void)
{
  return ifconnected != -1;
}

int
driver_setup_connection(const char *itrfname)
{
  int soc;
  int i;
  int found = 0;

  ifconnected = -1;

  soc = socket(AF_INET, SOCK_DGRAM, 0);
  if (soc <= 0) {
    ELOG_S("socket(): %s", strerror(errno));
    return -1;
  }

  gsocket = soc;

  if (find_interfaces() < 0) {
    close(gsocket);
    gsocket = -1;
    return -1;
  }

  for (i = 0; i < gifcount; ++i) {
    if (!strcmp(gifs[i].name, itrfname)) {
      ifconnected = i;
      found = 1;
      break;
    }
  }

  if (!found) {
    ELOG_S("Wireless interface \"%s\" not found", itrfname);
    return -1;
  }

  return 0;
}

int
driver_ioctl(int id, char *buf, size_t len)
{
  int retval;

  ASSERT(ifconnected != -1);

  /*ILOG0_DDD("Calling ioctl %d (0x%X), data size %d", id, id, len);*/

  if ((id >= MTLK_WE_IOCTL_FIRST) && (id <= MTLK_WE_IOCTL_LAST)) {
    struct iwreq wreq;

    /* this is wireless extensions private ioctl */
    memset(&wreq, 0, sizeof(wreq));
    memcpy(wreq.ifr_ifrn.ifrn_name, gifs[ifconnected].name, IFNAMSIZ);
    wreq.u.data.pointer = (void *) buf;
    retval = ioctl(gsocket, id, &wreq);
  } else {
    struct ifreq req;

    memset(&req, 0, sizeof(req));
    memcpy(req.ifr_name, gifs[ifconnected].name, IFNAMSIZ);
    req.ifr_data = (void *) buf;
    retval = ioctl(gsocket, id, &req);
  }
  if (retval == -1) {
    ELOG_DS("Ioctl call failed (code %d): %s", retval, strerror(errno));
    return -1;
  }

  /*ILOG0_D("Ioctl call: success (code %d)", retval);*/
  return retval;
}

void
driver_close_connection(void)
{
  ILOG2_V("close the interface");
  close(gsocket);
}
