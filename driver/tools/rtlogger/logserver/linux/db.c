/*
 * Copyright (c) 2007-2008 Metalink Broadband (Israel)
 *
 * Database
 *
 * Written by: Andrey Fidrya
 *
 */

#include "mtlkinc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logsrv_utils.h"
#include "db.h"

#define LOG_LOCAL_GID   GID_DB
#define LOG_LOCAL_FID   1

struct scd_entry *scd_data = NULL;

static int scd_entry_push_back(int oid, int gid, int fid, int lid, char *text);
static int scd_destroy(void);

int
db_init(void)
{
  int rslt = 0;

  return rslt;
}

int
db_destroy(void)
{
  int rslt = 0;

  if (0 != scd_destroy()) {
    rslt = -1;
    goto cleanup;
  }

cleanup:
  return rslt;
}


int
db_register_scd_entry(int oid, int gid, int fid, int lid, char *text)
{
  int rslt = 0;
  struct scd_entry *ent;

  for (ent = scd_data; ent; ent = ent->next) {
    if (ent->oid == oid && ent->gid == gid && ent->fid == fid && ent->lid == lid) {
      ELOG_DDDD("Duplicate text entry found for OID/GID/FID/LID combination: %d,%d,%d,%d",
          oid, gid, fid, lid);
      rslt = -1;
      goto cleanup;
    }
  }

  if (0 != scd_entry_push_back(oid, gid, fid, lid, text)) {
    rslt = -1;
    goto cleanup;
  }

cleanup:
  return rslt;
}

static int
scd_entry_push_back(int oid, int gid, int fid, int lid, char *text)
{
  int rslt = 0;
  struct scd_entry *ent;

  ent = (struct scd_entry *) malloc(sizeof(struct scd_entry));
  if (!ent) {
    ELOG_V("Out of memory");
    rslt = -1;
    goto cleanup;
  }
  memset(ent, 0, sizeof(*ent));

  ent->oid = oid;
  ent->gid = gid;
  ent->fid = fid;
  ent->lid = lid;
  ent->text = strdup(text);
  if (!ent->text) {
    ELOG_V("Out of memory");
    rslt = -1;
    goto cleanup;
  }

  ent->next = scd_data;
  scd_data = ent;
  ILOG2_DDDS("Text added to SCD db (gid %d, fid %d, lid %d): %s",
      gid, fid, lid, text);

cleanup:
  if (rslt != 0) {
    if (ent) {
      if (ent->text)
        free(ent->text);
      free(ent);
    }
  }
  return rslt;
}

char *
scd_get_text(int oid, int gid, int fid, int lid)
{
  struct scd_entry *ent;

  for (ent = scd_data; ent; ent = ent->next)
    if (ent->oid == oid &&
        ent->gid == gid &&
        ent->fid == fid &&
        ent->lid == lid)
      return ent->text;

  return NULL;
}

static int
scd_destroy(void)
{
  int rslt = 0;
  struct scd_entry *pnext;

  while (scd_data) {
    pnext = scd_data->next;

    free(scd_data->text);
    free(scd_data);

    scd_data = pnext;
  }

  return rslt;
}


