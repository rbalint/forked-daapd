/*
 * Copyright (C) 2009 Julien BLACHE <jb@jblache.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>

#include <stddef.h>

#include <pthread.h>

#include <sqlite3.h>

#include "logger.h"
#include "db.h"


#define STR(x) ((x) ? (x) : "")
#define DB_PATH "/var/cache/mt-daapd/songs3.db" /* FIXME */


#define DB_TYPE_CHAR    1
#define DB_TYPE_INT     2
#define DB_TYPE_INT64   3
#define DB_TYPE_STRING  4

struct col_type_map {
  ssize_t offset;
  short type;
};

#define mfi_offsetof(field) offsetof(struct media_file_info, field)
#define pli_offsetof(field) offsetof(struct playlist_info, field)

/* This list must be kept in sync with
 * - the order of the columns in the songs table
 * - the type and name of the fields in struct media_file_info
 */
static struct col_type_map mfi_cols_map[] =
  {
    { mfi_offsetof(id),              DB_TYPE_INT },
    { mfi_offsetof(path),            DB_TYPE_STRING },
    { mfi_offsetof(fname),           DB_TYPE_STRING },
    { mfi_offsetof(title),           DB_TYPE_STRING },
    { mfi_offsetof(artist),          DB_TYPE_STRING },
    { mfi_offsetof(album),           DB_TYPE_STRING },
    { mfi_offsetof(genre),           DB_TYPE_STRING },
    { mfi_offsetof(comment),         DB_TYPE_STRING },
    { mfi_offsetof(type),            DB_TYPE_STRING },
    { mfi_offsetof(composer),        DB_TYPE_STRING },
    { mfi_offsetof(orchestra),       DB_TYPE_STRING },
    { mfi_offsetof(conductor),       DB_TYPE_STRING },
    { mfi_offsetof(grouping),        DB_TYPE_STRING },
    { mfi_offsetof(url),             DB_TYPE_STRING },
    { mfi_offsetof(bitrate),         DB_TYPE_INT },
    { mfi_offsetof(samplerate),      DB_TYPE_INT },
    { mfi_offsetof(song_length),     DB_TYPE_INT },
    { mfi_offsetof(file_size),       DB_TYPE_INT64 },
    { mfi_offsetof(year),            DB_TYPE_INT },
    { mfi_offsetof(track),           DB_TYPE_INT },
    { mfi_offsetof(total_tracks),    DB_TYPE_INT },
    { mfi_offsetof(disc),            DB_TYPE_INT },
    { mfi_offsetof(total_discs),     DB_TYPE_INT },
    { mfi_offsetof(bpm),             DB_TYPE_INT },
    { mfi_offsetof(compilation),     DB_TYPE_CHAR },
    { mfi_offsetof(rating),          DB_TYPE_INT },
    { mfi_offsetof(play_count),      DB_TYPE_INT },
    { mfi_offsetof(data_kind),       DB_TYPE_INT },
    { mfi_offsetof(item_kind),       DB_TYPE_INT },
    { mfi_offsetof(description),     DB_TYPE_STRING },
    { mfi_offsetof(time_added),      DB_TYPE_INT },
    { mfi_offsetof(time_modified),   DB_TYPE_INT },
    { mfi_offsetof(time_played),     DB_TYPE_INT },
    { mfi_offsetof(db_timestamp),    DB_TYPE_INT },
    { mfi_offsetof(disabled),        DB_TYPE_INT },
    { mfi_offsetof(sample_count),    DB_TYPE_INT64 },
    { mfi_offsetof(force_update),    DB_TYPE_INT },
    { mfi_offsetof(codectype),       DB_TYPE_STRING },
    { mfi_offsetof(index),           DB_TYPE_INT },
    { mfi_offsetof(has_video),       DB_TYPE_INT },
    { mfi_offsetof(contentrating),   DB_TYPE_INT },
    { mfi_offsetof(bits_per_sample), DB_TYPE_INT },
    { mfi_offsetof(album_artist),    DB_TYPE_STRING },
  };

/* This list must be kept in sync with
 * - the order of the columns in the playlists table
 * - the type and name of the fields in struct playlist_info
 */
static struct col_type_map pli_cols_map[] =
  {
    { pli_offsetof(id),           DB_TYPE_INT },
    { pli_offsetof(title),        DB_TYPE_STRING },
    { pli_offsetof(type),         DB_TYPE_INT },
    { pli_offsetof(items),        DB_TYPE_INT },
    { pli_offsetof(query),        DB_TYPE_STRING },
    { pli_offsetof(db_timestamp), DB_TYPE_INT },
    { pli_offsetof(path),         DB_TYPE_STRING },
    { pli_offsetof(index),        DB_TYPE_INT },
  };

#define dbmfi_offsetof(field) offsetof(struct db_media_file_info, field)
#define dbpli_offsetof(field) offsetof(struct db_playlist_info, field)

/* This list must be kept in sync with
 * - the order of the columns in the songs table
 * - the name of the fields in struct db_media_file_info
 */
static ssize_t dbmfi_cols_map[] =
  {
    dbmfi_offsetof(id),
    dbmfi_offsetof(path),
    dbmfi_offsetof(fname),
    dbmfi_offsetof(title),
    dbmfi_offsetof(artist),
    dbmfi_offsetof(album),
    dbmfi_offsetof(genre),
    dbmfi_offsetof(comment),
    dbmfi_offsetof(type),
    dbmfi_offsetof(composer),
    dbmfi_offsetof(orchestra),
    dbmfi_offsetof(conductor),
    dbmfi_offsetof(grouping),
    dbmfi_offsetof(url),
    dbmfi_offsetof(bitrate),
    dbmfi_offsetof(samplerate),
    dbmfi_offsetof(song_length),
    dbmfi_offsetof(file_size),
    dbmfi_offsetof(year),
    dbmfi_offsetof(track),
    dbmfi_offsetof(total_tracks),
    dbmfi_offsetof(disc),
    dbmfi_offsetof(total_discs),
    dbmfi_offsetof(bpm),
    dbmfi_offsetof(compilation),
    dbmfi_offsetof(rating),
    dbmfi_offsetof(play_count),
    dbmfi_offsetof(data_kind),
    dbmfi_offsetof(item_kind),
    dbmfi_offsetof(description),
    dbmfi_offsetof(time_added),
    dbmfi_offsetof(time_modified),
    dbmfi_offsetof(time_played),
    dbmfi_offsetof(db_timestamp),
    dbmfi_offsetof(disabled),
    dbmfi_offsetof(sample_count),
    dbmfi_offsetof(force_update),
    dbmfi_offsetof(codectype),
    dbmfi_offsetof(idx),
    dbmfi_offsetof(has_video),
    dbmfi_offsetof(contentrating),
    dbmfi_offsetof(bits_per_sample),
    dbmfi_offsetof(album_artist),
  };

/* This list must be kept in sync with
 * - the order of the columns in the playlists table
 * - the name of the fields in struct playlist_info
 */
static ssize_t dbpli_cols_map[] =
  {
    dbpli_offsetof(id),
    dbpli_offsetof(title),
    dbpli_offsetof(type),
    dbpli_offsetof(items),
    dbpli_offsetof(query),
    dbpli_offsetof(db_timestamp),
    dbpli_offsetof(path),
    dbpli_offsetof(index),
  };

static __thread sqlite3 *hdl;


char *
db_escape_string(const char *str)
{
  char *escaped;
  char *ret;

  escaped = sqlite3_mprintf("%q", str);
  if (!escaped)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for escaped string\n");

      return NULL;
    }

  ret = strdup(escaped);

  sqlite3_free(escaped);

  return ret;
}

void
free_mfi(struct media_file_info *mfi, int content_only)
{
  if (mfi->path)
    free(mfi->path);

  if (mfi->fname)
    free(mfi->fname);

  if (mfi->title)
    free(mfi->title);

  if (mfi->artist)
    free(mfi->artist);

  if (mfi->album)
    free(mfi->album);

  if (mfi->genre)
    free(mfi->genre);

  if (mfi->comment)
    free(mfi->comment);

  if (mfi->type)
    free(mfi->type);

  if (mfi->composer)
    free(mfi->composer);

  if (mfi->orchestra)
    free(mfi->orchestra);

  if (mfi->conductor)
    free(mfi->conductor);

  if (mfi->grouping)
    free(mfi->grouping);

  if (mfi->description)
    free(mfi->description);

  if (mfi->codectype)
    free(mfi->codectype);

  if (mfi->album_artist)
    free(mfi->album_artist);

  if (!content_only)
    free(mfi);
}

void
free_pli(struct playlist_info *pli, int content_only)
{
  if (pli->title)
    free(pli->title);

  if (pli->query)
    free(pli->query);

  if (pli->path)
    free(pli->path);

  if (!content_only)
    free(pli);
}

void
db_purge_cruft(time_t ref)
{
  char *errmsg;
  int i;
  int ret;
  char *queries[4] = { NULL, NULL, NULL, NULL };
  char *queries_tmpl[4] =
    {
      "DELETE FROM playlistitems WHERE playlistid IN (SELECT id FROM playlists WHERE id <> 1 AND db_timestamp < %" PRIi64 ");",
      "DELETE FROM playlists WHERE id <> 1 AND db_timestamp < %" PRIi64 ";",
      "DELETE FROM playlistitems WHERE songid IN (SELECT id FROM songs WHERE db_timestamp < %" PRIi64 ");",
      "DELETE FROM songs WHERE db_timestamp < %" PRIi64 ";"
    };

  if (sizeof(queries) != sizeof(queries_tmpl))
    {
      DPRINTF(E_LOG, L_DB, "db_purge_cruft(): queries out of sync with queries_tmpl\n");
      return;
    }

  for (i = 0; i < (sizeof(queries_tmpl) / sizeof(queries_tmpl[0])); i++)
    {
      queries[i] = sqlite3_mprintf(queries_tmpl[i], (int64_t)ref);
      if (!queries[i])
	{
	  DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");
	  goto purge_fail;
	}
    }

  for (i = 0; i < (sizeof(queries) / sizeof(queries[0])); i++)
    {
      DPRINTF(E_DBG, L_DB, "Running purge query '%s'\n", queries[i]);

      ret = sqlite3_exec(hdl, queries[i], NULL, NULL, &errmsg);
      if (ret != SQLITE_OK)
	{
	  DPRINTF(E_LOG, L_DB, "Purge query %d error: %s\n", i, errmsg);

	  sqlite3_free(errmsg);
	}
      else
	DPRINTF(E_DBG, L_DB, "Purged %d rows\n", sqlite3_changes(hdl));
    }

 purge_fail:
  for (i = 0; i < (sizeof(queries) / sizeof(queries[0])); i++)
    {
      sqlite3_free(queries[i]);
    }

}


/* Queries */
static int
db_query_get_count(char *query)
{
  sqlite3_stmt *stmt;
  int ret;

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  ret = sqlite3_prepare_v2(hdl, query, -1, &stmt, NULL);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Could not prepare statement: %s\n", sqlite3_errmsg(hdl));
      return -1;
    }

  ret = sqlite3_step(stmt);
  if (ret != SQLITE_ROW)
    {
      DPRINTF(E_LOG, L_DB, "Could not step: %s\n", sqlite3_errmsg(hdl));	

      sqlite3_finalize(stmt);
      return -1;
    }

  ret = sqlite3_column_int(stmt, 0);

  sqlite3_finalize(stmt);

  return ret;
}

static int
db_build_query_index_clause(struct query_params *qp, char **i)
{
  char *idx;

  switch (qp->idx_type)
    {
      case I_FIRST:
	idx = sqlite3_mprintf("LIMIT %d", qp->limit);
	break;

      case I_LAST:
	idx = sqlite3_mprintf("LIMIT -1 OFFSET %d", qp->limit, qp->results - qp->limit);
	break;

      case I_SUB:
	idx = sqlite3_mprintf("LIMIT %d OFFSET %d", qp->limit, qp->offset);
	break;

      case I_NONE:
	*i = NULL;
	return 0;

      default:
	DPRINTF(E_LOG, L_DB, "Unknown index type\n");
	return -1;
    }

  if (!idx)
    {
      DPRINTF(E_LOG, L_DB, "Could not build index string; out of memory");
      return -1;
    }

  *i = idx;

  return 0;
}

static int
db_build_query_items(struct query_params *qp, char **q)
{
  char *query;
  char *count;
  char *idx;
  int ret;

  if (qp->filter)
    count = sqlite3_mprintf("SELECT COUNT(*) FROM songs WHERE %s;", qp->filter);
  else
    count = sqlite3_mprintf("SELECT COUNT(*) FROM songs;");

  if (!count)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for count query string\n");

      return -1;
    }

  qp->results = db_query_get_count(count);
  sqlite3_free(count);

  if (qp->results < 0)
    return -1;

  /* Get index clause */
  ret = db_build_query_index_clause(qp, &idx);
  if (ret < 0)
    return -1;

  if (idx && qp->filter)
    query = sqlite3_mprintf("SELECT * FROM songs WHERE %s %s;", qp->filter, idx);
  else if (idx)
    query = sqlite3_mprintf("SELECT * FROM songs %s;", idx);
  else if (qp->filter)
    query = sqlite3_mprintf("SELECT * FROM songs WHERE %s;", qp->filter);
  else
    query = sqlite3_mprintf("SELECT * FROM songs;");

  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");
      return -1;
    }

  *q = query;

  return 0;
}

static int
db_build_query_pls(struct query_params *qp, char **q)
{
  char *query;
  char *idx;
  int ret;

  qp->results = db_query_get_count("SELECT COUNT(*) FROM playlists;");
  if (qp->results < 0)
    return -1;

  /* Get index clause */
  ret = db_build_query_index_clause(qp, &idx);
  if (ret < 0)
    return -1;

  if (idx)
    query = sqlite3_mprintf("SELECT * FROM playlists %s;", idx);
  else
    query = sqlite3_mprintf("SELECT * FROM playlists;");

  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");
      return -1;
    }

  *q = query;

  return 0;
}

static int
db_build_query_plitems(struct query_params *qp, char **q)
{
  char *query;
  char *count;
  char *idx;
  int ret;

  if (qp->pl_id <= 0)
    {
      DPRINTF(E_LOG, L_DB, "No playlist id specified in playlist items query\n");
      return -1;
    }

  if (qp->pl_id == 1)
    {
      if (qp->filter)
	count = sqlite3_mprintf("SELECT COUNT(*) FROM songs WHERE %s;", qp->filter);
      else
	count = sqlite3_mprintf("SELECT COUNT(*) FROM songs;");
    }
  else
    {
      if (qp->filter)
	count = sqlite3_mprintf("SELECT COUNT(*) FROM songs JOIN playlistitems ON songs.id = playlistitems.songid"
				" WHERE playlistitems.playlistid = %d AND %s;", qp->pl_id, qp->filter);
      else
	count = sqlite3_mprintf("SELECT COUNT(*) FROM songs JOIN playlistitems ON songs.id = playlistitems.songid"
				" WHERE playlistitems.playlistid = %d;", qp->pl_id);
    }

  if (!count)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for count query string\n");

      return -1;
    }

  qp->results = db_query_get_count(count);
  sqlite3_free(count);

  if (qp->results < 0)
    return -1;

  /* Get index clause */
  ret = db_build_query_index_clause(qp, &idx);
  if (ret < 0)
    return -1;

  if (idx && qp->filter)
    query = sqlite3_mprintf("SELECT songs.* FROM songs JOIN playlistitems ON songs.id = playlistitems.songid"
			    " WHERE playlistitems.playlistid = %d AND %s ORDER BY playlistitems.id ASC %s;",
			    qp->pl_id, qp->filter, idx);
  else if (idx)
    query = sqlite3_mprintf("SELECT songs.* FROM songs JOIN playlistitems ON songs.id = playlistitems.songid"
			    " WHERE playlistitems.playlistid = %d ORDER BY playlistitems.id ASC %s;",
			    qp->pl_id, idx);
  else if (qp->filter)
    query = sqlite3_mprintf("SELECT songs.* FROM songs JOIN playlistitems ON songs.id = playlistitems.songid"
			    " WHERE playlistitems.playlistid = %d AND %s ORDER BY playlistitems.id ASC;",
			    qp->pl_id, qp->filter);
  else
    query = sqlite3_mprintf("SELECT songs.* FROM songs JOIN playlistitems ON songs.id = playlistitems.songid"
			    " WHERE playlistitems.playlistid = %d ORDER BY playlistitems.id ASC;",
			    qp->pl_id);

  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");
      return -1;
    }

  *q = query;

  return 0;
}

static int
db_build_query_browse(struct query_params *qp, char *field, char **q)
{
  char *query;
  char *count;
  char *idx;
  int ret;

  if (qp->filter)
    count = sqlite3_mprintf("SELECT COUNT(DISTINCT %s) FROM songs WHERE data_kind = 0 AND %s != '' AND %s;", field, field, qp->filter);
  else
    count = sqlite3_mprintf("SELECT COUNT(DISTINCT %s) FROM songs WHERE data_kind = 0 AND %s != '';", field, field);

  if (!count)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for count query string\n");

      return -1;
    }

  qp->results = db_query_get_count(count);
  sqlite3_free(count);

  if (qp->results < 0)
    return -1;

  /* Get index clause */
  ret = db_build_query_index_clause(qp, &idx);
  if (ret < 0)
    return -1;

  if (idx && qp->filter)
    query = sqlite3_mprintf("SELECT DISTINCT %s FROM songs WHERE data_kind = 0 AND %s != ''"
			    " AND %s %s;", field, field, qp->filter, idx);
  else if (idx)
    query = sqlite3_mprintf("SELECT DISTINCT %s FROM songs WHERE data_kind = 0 AND %s != ''"
			    " %s;", field, field, idx);
  else if (qp->filter)
    query = sqlite3_mprintf("SELECT DISTINCT %s FROM songs WHERE data_kind = 0 AND %s != ''"
			    " AND %s;", field, field, qp->filter);
  else
    query = sqlite3_mprintf("SELECT DISTINCT %s FROM songs WHERE data_kind = 0 AND %s != ''",
			    field, field);

  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");
      return -1;
    }

  *q = query;

  return 0;
}

int
db_query_start(struct query_params *qp)
{
  char *query;
  int ret;

  qp->stmt = NULL;

  switch (qp->type)
    {
      case Q_ITEMS:
	ret = db_build_query_items(qp, &query);
	break;

      case Q_PL:
	ret = db_build_query_pls(qp, &query);
	break;

      case Q_PLITEMS:
	ret = db_build_query_plitems(qp, &query);
	break;

      case Q_BROWSE_ALBUMS:
	ret = db_build_query_browse(qp, "album", &query);
	break;

      case Q_BROWSE_ARTISTS:
	ret = db_build_query_browse(qp, "artist", &query);
	break;

      case Q_BROWSE_GENRES:
	ret = db_build_query_browse(qp, "genre", &query);
	break;

      case Q_BROWSE_COMPOSERS:
	ret = db_build_query_browse(qp, "composer", &query);
	break;

      default:
	DPRINTF(E_LOG, L_DB, "Unknown query type\n");
	return -1;
    }

  if (ret < 0)
    return -1;

  DPRINTF(E_DBG, L_DB, "Starting query '%s'\n", query);

  ret = sqlite3_prepare_v2(hdl, query, -1, &qp->stmt, NULL);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Could not prepare statement: %s\n", sqlite3_errmsg(hdl));

      sqlite3_free(query);
      return -1;
    }

  sqlite3_free(query);

  return 0;
}

void
db_query_end(struct query_params *qp)
{
  if (!qp->stmt)
    return;

  qp->results = -1;

  sqlite3_finalize(qp->stmt);
  qp->stmt = NULL;
}

int
db_query_fetch_file(struct query_params *qp, struct db_media_file_info *dbmfi)
{
  int ncols;
  char **strcol;
  int i;
  int ret;

  memset(dbmfi, 0, sizeof(struct db_media_file_info));

  if (!qp->stmt)
    {
      DPRINTF(E_LOG, L_DB, "Query not started!\n");
      return -1;
    }

  if ((qp->type != Q_ITEMS) && (qp->type != Q_PLITEMS))
    {
      DPRINTF(E_LOG, L_DB, "Not an items or playlist items query!\n");
      return -1;
    }

  ret = sqlite3_step(qp->stmt);
  if (ret == SQLITE_DONE)
    {
      DPRINTF(E_INFO, L_DB, "End of query results\n");
      dbmfi->id = NULL;
      return 0;
    }
  else if (ret != SQLITE_ROW)
    {
      DPRINTF(E_LOG, L_DB, "Could not step: %s\n", sqlite3_errmsg(hdl));	
      return -1;
    }

  ncols = sqlite3_column_count(qp->stmt);

  if (sizeof(dbmfi_cols_map) / sizeof(dbmfi_cols_map[0]) != ncols)
    {
      DPRINTF(E_LOG, L_DB, "BUG: dbmfi column map out of sync with schema\n");
      return -1;
    }

  for (i = 0; i < ncols; i++)
    {
      strcol = (char **) ((char *)dbmfi + dbmfi_cols_map[i]);

      *strcol = (char *)sqlite3_column_text(qp->stmt, i);
    }

  return 0;
}

int
db_query_fetch_pl(struct query_params *qp, struct db_playlist_info *dbpli)
{
  int ncols;
  char **strcol;
  int i;
  int ret;

  memset(dbpli, 0, sizeof(struct db_playlist_info));

  if (!qp->stmt)
    {
      DPRINTF(E_LOG, L_DB, "Query not started!\n");
      return -1;
    }

  if (qp->type != Q_PL)
    {
      DPRINTF(E_LOG, L_DB, "Not a playlist query!\n");
      return -1;
    }

  ret = sqlite3_step(qp->stmt);
  if (ret == SQLITE_DONE)
    {
      DPRINTF(E_INFO, L_DB, "End of query results\n");
      dbpli->id = NULL;
      return 0;
    }
  else if (ret != SQLITE_ROW)
    {
      DPRINTF(E_LOG, L_DB, "Could not step: %s\n", sqlite3_errmsg(hdl));	
      return -1;
    }

  ncols = sqlite3_column_count(qp->stmt);

  if (sizeof(dbpli_cols_map) / sizeof(dbpli_cols_map[0]) != ncols)
    {
      DPRINTF(E_LOG, L_DB, "BUG: dbpli column map out of sync with schema\n");
      return -1;
    }

  for (i = 0; i < ncols; i++)
    {
      strcol = (char **) ((char *)dbpli + dbpli_cols_map[i]);

      *strcol = (char *)sqlite3_column_text(qp->stmt, i);
    }

  return 0;
}

int
db_query_fetch_string(struct query_params *qp, char **string)
{
  int ret;

  *string = NULL;

  if (!qp->stmt)
    {
      DPRINTF(E_LOG, L_DB, "Query not started!\n");
      return -1;
    }

  if (!(qp->type & Q_F_BROWSE))
    {
      DPRINTF(E_LOG, L_DB, "Not a browse query!\n");
      return -1;
    }

  ret = sqlite3_step(qp->stmt);
  if (ret == SQLITE_DONE)
    {
      DPRINTF(E_INFO, L_DB, "End of query results\n");
      *string = NULL;
      return 0;
    }
  else if (ret != SQLITE_ROW)
    {
      DPRINTF(E_LOG, L_DB, "Could not step: %s\n", sqlite3_errmsg(hdl));	
      return -1;
    }

  *string = (char *)sqlite3_column_text(qp->stmt, 0);

  return 0;
}


/* Files */
int
db_files_get_count(int *count)
{
  char *query = "SELECT COUNT(*) FROM songs;";
  sqlite3_stmt *stmt;
  int ret;

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  ret = sqlite3_prepare_v2(hdl, query, strlen(query) + 1, &stmt, NULL);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Could not prepare statement: %s\n", sqlite3_errmsg(hdl));

      return -1;
    }

  ret = sqlite3_step(stmt);
  if (ret != SQLITE_ROW)
    {
      DPRINTF(E_LOG, L_DB, "Could not step: %s\n", sqlite3_errmsg(hdl));

      sqlite3_finalize(stmt);
      return -1;
    }

  *count = sqlite3_column_int(stmt, 0);

  sqlite3_finalize(stmt);

  return 0;
}

void
db_file_inc_playcount(int id)
{
#define Q_TMPL "UPDATE songs SET play_count = play_count + 1, time_played = %" PRIi64 " WHERE id = %d;"
  char *query;
  char *errmsg;
  int ret;

  query = sqlite3_mprintf(Q_TMPL, (int64_t)time(NULL), id);
  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");

      return;
    }

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  errmsg = NULL;
  ret = sqlite3_exec(hdl, query, NULL, NULL, &errmsg);
  if (ret != SQLITE_OK)
    DPRINTF(E_LOG, L_DB, "Error incrementing play count on %d: %s\n", id, errmsg);

  sqlite3_free(errmsg);
  sqlite3_free(query);

#undef Q_TMPL
}

void
db_file_ping(int id)
{
#define Q_TMPL "UPDATE songs SET db_timestamp = %" PRIi64 " WHERE id = %d;"
  char *query;
  char *errmsg;
  int ret;

  query = sqlite3_mprintf(Q_TMPL, (int64_t)time(NULL), id);
  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");

      return;
    }

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  errmsg = NULL;
  ret = sqlite3_exec(hdl, query, NULL, NULL, &errmsg);
  if (ret != SQLITE_OK)
    DPRINTF(E_LOG, L_DB, "Error pinging file %d: %s\n", id, errmsg);

  sqlite3_free(errmsg);
  sqlite3_free(query);

#undef Q_TMPL
}

int
db_file_id_bypath(char *path, int *id)
{
#define Q_TMPL "SELECT id FROM songs WHERE path = '%q';"
  char *query;
  sqlite3_stmt *stmt;
  int ret;

  query = sqlite3_mprintf(Q_TMPL, path);
  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");

      return -1;
    }

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  ret = sqlite3_prepare_v2(hdl, query, strlen(query) + 1, &stmt, NULL);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Could not prepare statement: %s\n", sqlite3_errmsg(hdl));

      sqlite3_free(query);
      return -1;
    }

  ret = sqlite3_step(stmt);
  if (ret != SQLITE_ROW)
    {
      if (ret == SQLITE_DONE)
	DPRINTF(E_INFO, L_DB, "No results\n");
      else
	DPRINTF(E_LOG, L_DB, "Could not step: %s\n", sqlite3_errmsg(hdl));	

      sqlite3_finalize(stmt);
      sqlite3_free(query);
      return -1;
    }

  *id = sqlite3_column_int(stmt, 0);

  sqlite3_finalize(stmt);
  sqlite3_free(query);

  return 0;

#undef Q_TMPL
}

static struct media_file_info *
db_file_fetch_byquery(char *query)
{
  struct media_file_info *mfi;
  sqlite3_stmt *stmt;
  int ncols;
  char *cval;
  uint32_t *ival;
  uint64_t *i64val;
  char **strval;
  int i;
  int ret;

  if (!query)
    return NULL;

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  mfi = (struct media_file_info *)malloc(sizeof(struct media_file_info));
  if (!mfi)
    {
      DPRINTF(E_LOG, L_DB, "Could not allocate struct media_file_info, out of memory\n");
      return NULL;
    }
  memset(mfi, 0, sizeof(struct media_file_info));

  ret = sqlite3_prepare_v2(hdl, query, -1, &stmt, NULL);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Could not prepare statement: %s\n", sqlite3_errmsg(hdl));

      free(mfi);
      return NULL;
    }

  ret = sqlite3_step(stmt);

  if (ret != SQLITE_ROW)
    {
      if (ret == SQLITE_DONE)
	DPRINTF(E_INFO, L_DB, "No results\n");
      else
	DPRINTF(E_LOG, L_DB, "Could not step: %s\n", sqlite3_errmsg(hdl));

      sqlite3_finalize(stmt);
      free(mfi);
      return NULL;
    }

  ncols = sqlite3_column_count(stmt);

  if (sizeof(mfi_cols_map) / sizeof(mfi_cols_map[0]) != ncols)
    {
      DPRINTF(E_LOG, L_DB, "BUG: mfi column map out of sync with schema\n");

      sqlite3_finalize(stmt);
      /* Can't risk free()ing what's inside the mfi in this case... */
      free(mfi);
      return NULL;
    }

  for (i = 0; i < ncols; i++)
    {
      switch (mfi_cols_map[i].type)
	{
	  case DB_TYPE_CHAR:
	    cval = (char *)mfi + mfi_cols_map[i].offset;

	    *cval = sqlite3_column_int(stmt, i);
	    break;

	  case DB_TYPE_INT:
	    ival = (uint32_t *) ((char *)mfi + mfi_cols_map[i].offset);

	    *ival = sqlite3_column_int(stmt, i);
	    break;

	  case DB_TYPE_INT64:
	    i64val = (uint64_t *) ((char *)mfi + mfi_cols_map[i].offset);

	    *i64val = sqlite3_column_int64(stmt, i);
	    break;

	  case DB_TYPE_STRING:
	    strval = (char **) ((char *)mfi + mfi_cols_map[i].offset);

	    cval = (char *)sqlite3_column_text(stmt, i);
	    if (cval)
	      *strval = strdup(cval);
	    break;

	  default:
	    DPRINTF(E_LOG, L_DB, "BUG: Unknown type %d in mfi column map\n", mfi_cols_map[i].type);

	    free_mfi(mfi, 0);
	    sqlite3_finalize(stmt);
	    return NULL;
	}
    }

  sqlite3_finalize(stmt);

  return mfi;
}

struct media_file_info *
db_file_fetch_byid(int id)
{
#define Q_TMPL "SELECT * FROM songs WHERE id = %d;"
  struct media_file_info *mfi;
  char *query;

  query = sqlite3_mprintf(Q_TMPL, id);
  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");

      return NULL;
    }

  mfi = db_file_fetch_byquery(query);

  sqlite3_free(query);

  return mfi;

#undef Q_TMPL
}

struct media_file_info *
db_file_fetch_bypath(char *path)
{
#define Q_TMPL "SELECT * FROM songs WHERE path = '%q';"
  struct media_file_info *mfi;
  char *query;

  query = sqlite3_mprintf(Q_TMPL, path);
  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");

      return NULL;
    }

  mfi = db_file_fetch_byquery(query);

  sqlite3_free(query);

  return mfi;

#undef Q_TMPL
}

int
db_file_add(struct media_file_info *mfi)
{
#define Q_TMPL "INSERT INTO songs (id, path, fname, title, artist, album, genre, comment, type, composer," \
               " orchestra, conductor, grouping, url, bitrate, samplerate, song_length, file_size, year, track," \
               " total_tracks, disc, total_discs, bpm, compilation, rating, play_count, data_kind, item_kind," \
               " description, time_added, time_modified, time_played, db_timestamp, disabled, sample_count," \
               " force_update, codectype, idx, has_video, contentrating, bits_per_sample, album_artist)" \
               " VALUES (NULL, '%q', '%q', %Q, %Q, %Q, %Q, %Q, %Q, %Q," \
               " %Q, %Q, %Q, %Q, %d, %d, %d, %" PRIi64 ", %d, %d," \
               " %d, %d, %d, %d, %d, %d, %d, %d, %d," \
               " %Q, %" PRIi64 ", %" PRIi64 ", %" PRIi64 ", %" PRIi64 ", %d, %" PRIi64 "," \
               " %d, %Q, %d, %d, %d, %d, %Q);"
  char *query;
  char *errmsg;
  int ret;


  if (mfi->id != 0)
    {
      DPRINTF(E_WARN, L_DB, "Trying to update file with id > 0; use db_file_update()?\n");
      return -1;
    }

  mfi->db_timestamp = (uint64_t)time(NULL);
  mfi->time_added = mfi->db_timestamp;

  if (mfi->time_modified == 0)
    mfi->time_modified = mfi->db_timestamp;

  query = sqlite3_mprintf(Q_TMPL,
			  STR(mfi->path), STR(mfi->fname), mfi->title, mfi->artist, mfi->album,
			  mfi->genre, mfi->comment, mfi->type, mfi->composer,
			  mfi->orchestra, mfi->conductor, mfi->grouping, mfi->url, mfi->bitrate,
			  mfi->samplerate, mfi->song_length, mfi->file_size, mfi->year, mfi->track,
			  mfi->total_tracks, mfi->disc, mfi->total_discs, mfi->bpm, mfi->compilation,
			  mfi->rating, mfi->play_count, mfi->data_kind, mfi->item_kind,
			  mfi->description, (int64_t)mfi->time_added, (int64_t)mfi->time_modified,
			  (int64_t)mfi->time_played, (int64_t)mfi->db_timestamp, mfi->disabled, mfi->sample_count,
			  mfi->force_update, mfi->codectype, mfi->index, mfi->has_video,
			  mfi->contentrating, mfi->bits_per_sample, mfi->album_artist);
  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");
      return -1;
    }

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  errmsg = NULL;
  ret = sqlite3_exec(hdl, query, NULL, NULL, &errmsg);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Query error: %s\n", errmsg);

      sqlite3_free(errmsg);
      sqlite3_free(query);
      return -1;
    }

  sqlite3_free(query);

  return 0;

#undef Q_TMPL
}

int
db_file_update(struct media_file_info *mfi)
{
#define Q_TMPL "UPDATE songs SET path = '%q', fname = '%q', title = %Q, artist = %Q, album = %Q, genre = %Q," \
               " comment = %Q, type = %Q, composer = %Q, orchestra = %Q, conductor = %Q, grouping = %Q," \
               " url = %Q, bitrate = %d, samplerate = %d, song_length = %d, file_size = %" PRIi64 "," \
               " year = %d, track = %d, total_tracks = %d, disc = %d, total_discs = %d, bpm = %d," \
               " compilation = %d, rating = %d, data_kind = %d, item_kind = %d," \
               " description = %Q, time_modified = %" PRIi64 "," \
               " db_timestamp = %" PRIi64 ", disabled = %d, sample_count = %d," \
               " force_update = %d, codectype = %Q, idx = %d, has_video = %d," \
               " bits_per_sample = %d, album_artist = %Q WHERE id = %d;"
  char *query;
  char *errmsg;
  int ret;

  if (mfi->id == 0)
    {
      DPRINTF(E_WARN, L_DB, "Trying to update file with id 0; use db_file_add()?\n");
      return -1;
    }

  mfi->db_timestamp = (uint64_t)time(NULL);

  if (mfi->time_modified == 0)
    mfi->time_modified = mfi->db_timestamp;

  query = sqlite3_mprintf(Q_TMPL,
			  STR(mfi->path), STR(mfi->fname), mfi->title, mfi->artist, mfi->album, mfi->genre,
			  mfi->comment, mfi->type, mfi->composer, mfi->orchestra, mfi->conductor, mfi->grouping, 
			  mfi->url, mfi->bitrate, mfi->samplerate, mfi->song_length, mfi->file_size,
			  mfi->year, mfi->track, mfi->total_tracks, mfi->disc, mfi->total_discs, mfi->bpm,
			  mfi->compilation, mfi->rating, mfi->data_kind, mfi->item_kind,
			  mfi->description, (int64_t)mfi->time_modified,
			  (int64_t)mfi->db_timestamp, mfi->disabled, mfi->sample_count,
			  mfi->force_update, mfi->codectype, mfi->index, mfi->has_video,
			  mfi->bits_per_sample, mfi->album_artist, mfi->id);

  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");
      return -1;
    }

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  errmsg = NULL;
  ret = sqlite3_exec(hdl, query, NULL, NULL, &errmsg);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Query error: %s\n", errmsg);

      sqlite3_free(errmsg);
      sqlite3_free(query);
      return -1;
    }

  sqlite3_free(query);

  return 0;

#undef Q_TMPL
}


/* Playlists */
int
db_pl_get_count(int *count)
{
  char *query = "SELECT COUNT(*) FROM playlists;";
  sqlite3_stmt *stmt;
  int ret;

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  ret = sqlite3_prepare_v2(hdl, query, strlen(query) + 1, &stmt, NULL);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Could not prepare statement: %s\n", sqlite3_errmsg(hdl));

      return -1;
    }

  ret = sqlite3_step(stmt);
  if (ret != SQLITE_ROW)
    {
      DPRINTF(E_LOG, L_DB, "Could not step: %s\n", sqlite3_errmsg(hdl));

      sqlite3_finalize(stmt);
      return -1;
    }

  *count = sqlite3_column_int(stmt, 0);

  sqlite3_finalize(stmt);

  return 0;
}

void
db_pl_ping(int id)
{
#define Q_TMPL "UPDATE playlists SET db_timestamp = %" PRIi64 " WHERE id = %d;"
  char *query;
  char *errmsg;
  int ret;

  query = sqlite3_mprintf(Q_TMPL, (int64_t)time(NULL), id);
  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");

      return;
    }

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  errmsg = NULL;
  ret = sqlite3_exec(hdl, query, NULL, NULL, &errmsg);
  if (ret != SQLITE_OK)
    DPRINTF(E_LOG, L_DB, "Error pinging playlist %d: %s\n", id, errmsg);

  sqlite3_free(errmsg);
  sqlite3_free(query);

#undef Q_TMPL
}

static struct playlist_info *
db_pl_fetch_byquery(char *query)
{
  struct playlist_info *pli;
  sqlite3_stmt *stmt;
  int ncols;
  char *cval;
  uint32_t *ival;
  char **strval;
  int i;
  int ret;

  if (!query)
    return NULL;

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  pli = (struct playlist_info *)malloc(sizeof(struct playlist_info));
  if (!pli)
    {
      DPRINTF(E_LOG, L_DB, "Could not allocate struct playlist_info, out of memory\n");
      return NULL;
    }
  memset(pli, 0, sizeof(struct playlist_info));

  ret = sqlite3_prepare_v2(hdl, query, -1, &stmt, NULL);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Could not prepare statement: %s\n", sqlite3_errmsg(hdl));

      free(pli);
      return NULL;
    }

  ret = sqlite3_step(stmt);
  if (ret != SQLITE_ROW)
    {
      if (ret == SQLITE_DONE)
	DPRINTF(E_INFO, L_DB, "No results\n");
      else
	DPRINTF(E_LOG, L_DB, "Could not step: %s\n", sqlite3_errmsg(hdl));

      sqlite3_finalize(stmt);
      free(pli);
      return NULL;
    }

  ncols = sqlite3_column_count(stmt);

  if (sizeof(pli_cols_map) / sizeof(pli_cols_map[0]) != ncols)
    {
      DPRINTF(E_LOG, L_DB, "BUG: pli column map out of sync with schema\n");

      sqlite3_finalize(stmt);
      /* Can't risk free()ing what's inside the pli in this case... */
      free(pli);
      return NULL;
    }

  for (i = 0; i < ncols; i++)
    {
      switch (pli_cols_map[i].type)
	{
	  case DB_TYPE_INT:
	    ival = (uint32_t *) ((char *)pli + pli_cols_map[i].offset);

	    *ival = sqlite3_column_int(stmt, i);
	    break;

	  case DB_TYPE_STRING:
	    strval = (char **) ((char *)pli + pli_cols_map[i].offset);

	    cval = (char *)sqlite3_column_text(stmt, i);
	    if (cval)
	      *strval = strdup(cval);
	    break;

	  default:
	    DPRINTF(E_LOG, L_DB, "BUG: Unknown type %d in pli column map\n", pli_cols_map[i].type);

	    sqlite3_finalize(stmt);
	    free_pli(pli, 0);
	    return NULL;
	}
    }

  sqlite3_finalize(stmt);

  /* Playlist 1: all files */
  if (pli->id == 1)
    {
      ret = db_files_get_count(&i);
      if (ret < 0)
	{
	  DPRINTF(E_LOG, L_DB, "Couldn't get song count for playlist 1\n");
	  i = 0;
	}

      pli->items = i;
    }

  return pli;
}

struct playlist_info *
db_pl_fetch_bypath(char *path)
{
#define Q_TMPL "SELECT * FROM playlists WHERE path = '%q';"
  struct playlist_info *pli;
  char *query;

  query = sqlite3_mprintf(Q_TMPL, path);
  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");

      return NULL;
    }

  pli = db_pl_fetch_byquery(query);

  sqlite3_free(query);

  return pli;

#undef Q_TMPL
}

int
db_pl_add(char *title, char *path, int *id)
{
#define QDUP_TMPL "SELECT COUNT(*) FROM playlists WHERE title = '%q' OR path = '%q';"
#define QADD_TMPL "INSERT INTO playlists (title, type, items, query, db_timestamp, path, idx)" \
                  " VALUES ('%q', 0, 0, NULL, %" PRIi64 ", '%q', 0);"
  char *query;
  char *errmsg;
  sqlite3_stmt *stmt;
  int ret;

  /* Check duplicates */
  query = sqlite3_mprintf(QDUP_TMPL, title, path);
  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");
      return -1;
    }

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  ret = sqlite3_prepare_v2(hdl, query, -1, &stmt, NULL);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Could not prepare statement: %s\n", sqlite3_errmsg(hdl));

      sqlite3_free(query);
      return -1;
    }

  ret = sqlite3_step(stmt);
  if (ret != SQLITE_ROW)
    {
      DPRINTF(E_LOG, L_DB, "Could not step: %s\n", sqlite3_errmsg(hdl));

      sqlite3_finalize(stmt);
      sqlite3_free(query);
      return -1;
    }

  ret = sqlite3_column_int(stmt, 0);

  sqlite3_finalize(stmt);
  sqlite3_free(query);

  if (ret > 0)
    {
      DPRINTF(E_WARN, L_DB, "Duplicate playlist with title '%s' path '%s'\n", title, path);
      return -1;
    }

  /* Add */
  query = sqlite3_mprintf(QADD_TMPL, title, (int64_t)time(NULL), path);
  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");
      return -1;
    }

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  errmsg = NULL;
  ret = sqlite3_exec(hdl, query, NULL, NULL, &errmsg);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Query error: %s\n", errmsg);

      sqlite3_free(errmsg);
      sqlite3_free(query);
      return -1;
    }

  sqlite3_free(query);

  *id = (int)sqlite3_last_insert_rowid(hdl);
  if (*id == 0)
    {
      DPRINTF(E_LOG, L_DB, "Successful insert but no last_insert_rowid!\n");
      return -1;
    }

  DPRINTF(E_DBG, L_DB, "Added playlist %s (path %s) with id %d\n", title, path, *id);

  return 0;

#undef QDUP_TMPL
#undef QADD_TMPL
}

int
db_pl_add_item(int plid, int mfid)
{
#define QPL_TMPL "SELECT title FROM playlists WHERE id = %d;"
#define QMF_TMPL "SELECT fname FROM songs WHERE id = %d;"
#define QADD_TMPL "INSERT INTO playlistitems (playlistid, songid) VALUES (%d, %d);"
  char *query;
  char *str;
  char *errmsg;
  sqlite3_stmt *stmt;
  int ret;

  /* Check playlist */
  query = sqlite3_mprintf(QPL_TMPL, plid);
  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");
      return -1;
    }

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  ret = sqlite3_prepare_v2(hdl, query, -1, &stmt, NULL);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Could not prepare statement: %s\n", sqlite3_errmsg(hdl));

      sqlite3_free(query);
      return -1;
    }

  ret = sqlite3_step(stmt);
  if (ret != SQLITE_ROW)
    {
      DPRINTF(E_LOG, L_DB, "Playlist id %d not found\n", plid);

      sqlite3_finalize(stmt);
      sqlite3_free(query);
      return -1;
    }

  str = (char *)sqlite3_column_text(stmt, 0);

  DPRINTF(E_DBG, L_DB, "Found playlist id %d ('%s')\n", plid, str);

  sqlite3_finalize(stmt);
  sqlite3_free(query);

  /* Check media file */
  query = sqlite3_mprintf(QMF_TMPL, mfid);
  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");
      return -1;
    }

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  ret = sqlite3_prepare_v2(hdl, query, -1, &stmt, NULL);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Could not prepare statement: %s\n", sqlite3_errmsg(hdl));

      sqlite3_free(query);
      return -1;
    }

  ret = sqlite3_step(stmt);
  if (ret != SQLITE_ROW)
    {
      DPRINTF(E_LOG, L_DB, "Media file id %d not found\n", mfid);

      sqlite3_finalize(stmt);
      sqlite3_free(query);
      return -1;
    }

  str = (char *)sqlite3_column_text(stmt, 0);

  DPRINTF(E_DBG, L_DB, "Found media file id %d (%s)\n", mfid, str);

  sqlite3_finalize(stmt);
  sqlite3_free(query);

  /* Add */
  query = sqlite3_mprintf(QADD_TMPL, plid, mfid);
  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");
      return -1;
    }

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  errmsg = NULL;
  ret = sqlite3_exec(hdl, query, NULL, NULL, &errmsg);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Query error: %s\n", errmsg);

      sqlite3_free(errmsg);
      sqlite3_free(query);
      return -1;
    }

  sqlite3_free(query);

  DPRINTF(E_DBG, L_DB, "Added media file id %d to playlist id %d\n", mfid, plid);

  return 0;

#undef QPL_TMPL
#undef QMF_TMPL
#undef QADD_TMPL
}

void
db_pl_update(int id)
{
#define QPL1_TMPL "UPDATE playlists SET items = %d WHERE id = 1;"
#define Q_TMPL "UPDATE playlists SET items = (SELECT COUNT(*) FROM playlistitems WHERE playlistid = %d) WHERE id = %d;"
  char *query;
  char *errmsg;
  int nsongs;
  int ret;

  if (id == 1)
    {
      ret = db_files_get_count(&nsongs);
      if (ret < 0)
	{
	  DPRINTF(E_LOG, L_DB, "Could not get file count\n");
	  return;
	}

      query = sqlite3_mprintf(QPL1_TMPL, nsongs);
    }
  else
    query = sqlite3_mprintf(Q_TMPL, id, id);

  if (!query)
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");
      return;
    }

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  errmsg = NULL;
  ret = sqlite3_exec(hdl, query, NULL, NULL, &errmsg);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Query error: %s\n", errmsg);

      sqlite3_free(errmsg);
      sqlite3_free(query);
      return;
    }

  sqlite3_free(query);

#undef QPL1_TMPL
#undef Q_TMPL
}

void
db_pl_update_all(void)
{
  char *query = "SELECT id FROM playlists;";
  sqlite3_stmt *stmt;
  int plid;
  int ret;

  DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query);

  ret = sqlite3_prepare_v2(hdl, query, strlen(query) + 1, &stmt, NULL);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Could not prepare statement: %s\n", sqlite3_errmsg(hdl));
      return;
    }

  while ((ret = sqlite3_step(stmt)) == SQLITE_ROW)
    {
      plid = sqlite3_column_int(stmt, 0);
      db_pl_update(plid);
    }

  if (ret != SQLITE_DONE)
    {
      DPRINTF(E_LOG, L_DB, "Could not step: %s\n", sqlite3_errmsg(hdl));

      sqlite3_finalize(stmt);
      return;
    }

  sqlite3_finalize(stmt);
}

void
db_pl_delete(int id)
{
#define Q1_TMPL "DELETE FROM playlists WHERE id = %d;"
#define Q2_TMPL "DELETE FROM playlistitems WHERE playlistid = %d;"
  char *query[2];
  char *errmsg;
  int i;
  int ret;

  if (id == 1)
    return;

  query[0] = sqlite3_mprintf(Q1_TMPL, id);
  query[1] = sqlite3_mprintf(Q2_TMPL, id);

  if (!query[0] || !query[1])
    {
      DPRINTF(E_LOG, L_DB, "Out of memory for query string\n");

      if (query[0])
	sqlite3_free(query[0]);
      if (query[1])
	sqlite3_free(query[1]);
      return;
    }

  for (i = 0; i < (sizeof(query) / sizeof(query[0])); i++)
    {
      DPRINTF(E_DBG, L_DB, "Running query '%s'\n", query[i]);

      errmsg = NULL;
      ret = sqlite3_exec(hdl, query[i], NULL, NULL, &errmsg);
      if (ret != SQLITE_OK)
	{
	  DPRINTF(E_LOG, L_DB, "Query error: %s\n", errmsg);
	  sqlite3_free(errmsg);
	}

      sqlite3_free(query[i]);
    }

#undef Q1_TMPL
#undef Q2_TMPL
}


int
db_perthread_init(void)
{
  int ret;

  ret = sqlite3_open(DB_PATH, &hdl);
  if (ret != SQLITE_OK)
    {
      DPRINTF(E_LOG, L_DB, "Could not open database: %s\n", sqlite3_errmsg(hdl));

      sqlite3_close(hdl);
      return -1;
    }

  return 0;
}

void
db_perthread_deinit(void)
{
  sqlite3_stmt *stmt;

  /* Tear down anything that's in flight */
  while ((stmt = sqlite3_next_stmt(hdl, 0)))
    sqlite3_finalize(stmt);

  sqlite3_close(hdl);
}


#define T_ADMIN					\
  "CREATE TABLE IF NOT EXISTS admin("		\
  "   key   VARCHAR(32) NOT NULL,"		\
  "   value VARCHAR(32) NOT NULL"		\
  ");"

#define T_SONGS					\
  "CREATE TABLE IF NOT EXISTS songs ("			\
  "   id              INTEGER PRIMARY KEY NOT NULL,"	\
  "   path            VARCHAR(4096) NOT NULL,"		\
  "   fname           VARCHAR(255) NOT NULL,"		\
  "   title           VARCHAR(1024) DEFAULT NULL,"	\
  "   artist          VARCHAR(1024) DEFAULT NULL,"	\
  "   album           VARCHAR(1024) DEFAULT NULL,"	\
  "   genre           VARCHAR(255) DEFAULT NULL,"	\
  "   comment         VARCHAR(4096) DEFAULT NULL,"	\
  "   type            VARCHAR(255) DEFAULT NULL,"	\
  "   composer        VARCHAR(1024) DEFAULT NULL,"	\
  "   orchestra       VARCHAR(1024) DEFAULT NULL,"	\
  "   conductor       VARCHAR(1024) DEFAULT NULL,"	\
  "   grouping        VARCHAR(1024) DEFAULT NULL,"	\
  "   url             VARCHAR(1024) DEFAULT NULL,"	\
  "   bitrate         INTEGER DEFAULT 0,"		\
  "   samplerate      INTEGER DEFAULT 0,"		\
  "   song_length     INTEGER DEFAULT 0,"		\
  "   file_size       INTEGER DEFAULT 0,"		\
  "   year            INTEGER DEFAULT 0,"		\
  "   track           INTEGER DEFAULT 0,"		\
  "   total_tracks    INTEGER DEFAULT 0,"		\
  "   disc            INTEGER DEFAULT 0,"		\
  "   total_discs     INTEGER DEFAULT 0,"		\
  "   bpm             INTEGER DEFAULT 0,"		\
  "   compilation     INTEGER DEFAULT 0,"		\
  "   rating          INTEGER DEFAULT 0,"		\
  "   play_count      INTEGER DEFAULT 0,"		\
  "   data_kind       INTEGER DEFAULT 0,"		\
  "   item_kind       INTEGER DEFAULT 0,"		\
  "   description     INTEGER DEFAULT 0,"		\
  "   time_added      INTEGER DEFAULT 0,"		\
  "   time_modified   INTEGER DEFAULT 0,"		\
  "   time_played     INTEGER DEFAULT 0,"		\
  "   db_timestamp    INTEGER DEFAULT 0,"		\
  "   disabled        INTEGER DEFAULT 0,"		\
  "   sample_count    INTEGER DEFAULT 0,"		\
  "   force_update    INTEGER DEFAULT 0,"		\
  "   codectype       VARCHAR(5) DEFAULT NULL,"		\
  "   idx             INTEGER NOT NULL,"		\
  "   has_video       INTEGER DEFAULT 0,"		\
  "   contentrating   INTEGER DEFAULT 0,"		\
  "   bits_per_sample INTEGER DEFAULT 0,"		\
  "   album_artist    VARCHAR(1024)"			\
  ");"

#define T_PL					\
  "CREATE TABLE IF NOT EXISTS playlists ("		\
  "   id             INTEGER PRIMARY KEY NOT NULL,"	\
  "   title          VARCHAR(255) NOT NULL,"		\
  "   type           INTEGER NOT NULL,"			\
  "   items          INTEGER NOT NULL,"			\
  "   query          VARCHAR(1024),"			\
  "   db_timestamp   INTEGER NOT NULL,"			\
  "   path           VARCHAR(4096),"			\
  "   idx            INTEGER NOT NULL"			\
  ");"

#define T_PLITEMS				\
  "CREATE TABLE IF NOT EXISTS playlistitems ("		\
  "   id             INTEGER PRIMARY KEY NOT NULL,"	\
  "   playlistid     INTEGER NOT NULL,"			\
  "   songid         INTEGER NOT NULL"			\
  ");"

#define Q_PL1								\
  "INSERT INTO playlists (id, title, type, items, query, db_timestamp, path, idx)" \
  " VALUES(1, 'Library', 1, 0, '1', 0, '', 0);"

#define I_PATH							\
  "CREATE INDEX IF NOT EXISTS idx_path ON songs(path, idx);"

#define I_FILEID							\
  "CREATE INDEX IF NOT EXISTS idx_fileid ON playlistitems(songid ASC);"

#define I_PLITEMID							\
  "CREATE INDEX IF NOT EXISTS idx_playlistid ON playlistitems(playlistid, songid);"

struct db_init_query {
  char *query;
  char *desc;
};

static struct db_init_query db_init_queries[] =
  {
    { T_ADMIN,     "create table admin" },
    { T_SONGS,     "create table songs" },
    { T_PL,        "create table playlists" },
    { T_PLITEMS,   "create table playlistitems" },

    { I_PATH,      "create file path index" },
    { I_FILEID,    "create file id index" },
    { I_PLITEMID,  "create playlist id index" },
  };

static int
db_create_tables(void)
{
  char *errmsg;
  int i;
  int ret;

  for (i = 0; i < (sizeof(db_init_queries) / sizeof(db_init_queries[0])); i++)
    {
      DPRINTF(E_DBG, L_DB, "DB init query: %s\n", db_init_queries[i].desc);

      ret = sqlite3_exec(hdl, db_init_queries[i].query, NULL, NULL, &errmsg);
      if (ret != SQLITE_OK)
	{
	  DPRINTF(E_FATAL, L_DB, "DB init error: %s\n", errmsg);

	  sqlite3_free(errmsg);
	  return -1;
	}
    }

  ret = db_query_get_count("SELECT COUNT(*) FROM playlists WHERE id = 1;");
  if (ret != 1)
    {
      DPRINTF(E_DBG, L_DB, "Creating default playlist\n");

      ret = sqlite3_exec(hdl, Q_PL1, NULL, NULL, &errmsg);
      if (ret != SQLITE_OK)
	{
	  DPRINTF(E_FATAL, L_DB, "Could not add default playlist: %s\n", errmsg);

	  sqlite3_free(errmsg);
	  return -1;
	}
    }

  return 0;
}

int
db_init(void)
{
  int files;
  int pls;
  int ret;

  if (!sqlite3_threadsafe())
    {
      DPRINTF(E_FATAL, L_DB, "The SQLite3 library is not built with a threadsafe configuration\n");
      return -1;
    }

  ret = db_perthread_init();
  if (ret < 0)
    return ret;

  ret = db_create_tables();
  if (ret < 0)
    {
      DPRINTF(E_FATAL, L_DB, "Could not create tables\n");
      db_perthread_deinit();
      return -1;
    }

  ret = db_files_get_count(&files);
  if (ret < 0)
    {
      DPRINTF(E_FATAL, L_DB, "Could not fetch file count\n");
      db_perthread_deinit();
      return -1;
    }

  ret = db_pl_get_count(&pls);
  if (ret < 0)
    {
      DPRINTF(E_FATAL, L_DB, "Could not fetch playlist count\n");
      db_perthread_deinit();
      return -1;
    }

  db_perthread_deinit();

  DPRINTF(E_INFO, L_DB, "Database OK with %d files and %d playlists\n", files, pls);

  return 0;
}
