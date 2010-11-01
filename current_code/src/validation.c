/*
 * validation.c
 * Copyright (C) Clearscene Ltd 2008 <wbooth@essentialcollections.co.uk>
 * 
 * db.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * validation.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include "validation.h"
#include "debug.h"
#include "utils.h"

extern char *getPostData(gpointer post_hash, char *key) {
  struct post_data_struct *data_struct = (struct post_data_struct *)g_hash_table_lookup(post_hash, key);
  if(data_struct == NULL && data_struct->data == NULL)
    return NULL;
  else
    return data_struct->data;
}

static int checkVitals(char *vvalue, int doLengthChecking, int doEscaping) {

  // We actually have a value
  if ( vvalue == NULL ) {
    debug_message("http post data contained a key with no value", ERROR);
    return 1;
  }

  // The value contains something
  if ( 0 != strcmp(vvalue, "") ) {
    debug_message("http post data contained a key with no value", ERROR);
    return 1;
  }

  // Got a decent length
  if( doLengthChecking && 6 > strlen(vvalue) && strlen(vvalue) < 20 ) {
    debug_message("http post data value outside specified length", ERROR);
    return 1;
  }

  // Escape
  if( doEscaping ) {
    vvalue[strcspn ( vvalue, "\n" )] = '\0';
  }

  return 0;
}

extern int basicValidation(GHashTable *postdata) {

  GHashTableIter iter;
  gpointer key, value;

  // Check thet we have a main request param
  char *action = getPostData(postdata, "action");
  if ( action == NULL ) {
    debug_message( "no requested 'action' given.", ERROR);
    return 1;
  }

  // Check the main request param vitals
  if ( checkVitals( action, 1, 1 ) ) {
    return 1;
  }

  // Check the main request param is sane
  if ( 0 != strcmp(action, "getDocList") 
    && 0 != strcmp(action, "getDocDetail") 
    && 0 != strcmp(action, "getScannerList") 
    && 0 != strcmp(action, "doScan") 
    && 0 != strcmp(action, "getScanningProgress") 
    && 0 != strcmp(action, "nextPageReady") 
    && 0 != strcmp(action, "updateDocDetails") 
    && 0 != strcmp(action, "moveTag") 
    && 0 != strcmp(action, "filter") 
    && 0 != strcmp(action, "deletedoc") 
    && 0 != strcmp(action, "getAudio") ) {
    debug_message( "requested 'action' is not available.", ERROR);
    return 1;
  }

  g_hash_table_iter_init (&iter, postdata);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    if( checkVitals( key, 1, 0 )  
     || checkVitals( getPostData(postdata, key), 0, 0 ) ) {
      return 1;
    }
  }

  return 0;
}




/***********************************************
 * Generic Checks
 */

// Check trhe hashtable to ensure it only contains the specified keys
static int checkOnlyKeys(GHashTable *postdata, char *keyList) {
  // TODO
  return 0;
}

// Ensure the value is effectivly an int
static int checkStringIsInt(char *StrInt) {
  // TODO
  return 0;  
}

static int checkSaneRange(char *StrInt, int low, int high) {
  int i = atoi(StrInt);
  if(i >= low && i <= high) return 0;
  debug_message("Validation failed: not a sane range", ERROR);
  return 1;
}



/***********************************************
 * Checks on each field
 */

static int checkUUID(char *val) {
  return 0;
}

//
static int checkDocId(char *val) {
  if(checkStringIsInt(val)) return 1;
  if(checkSaneRange(val, 1, 9999999)) return 1;
  return 0;
}

//
static int checkAddRemove(char *val) {
  if ( 0 != strcmp(val, "add")
    || 0 != strcmp(val, "remove" ) ) {
    return 0;
  }
  debug_message("Validation failed: add/remove check", ERROR);
  return 1;
}

//
static int checkCheckbox(char *val) {
  if ( 0 != strcmp(val, "")
    || 0 != strcmp(val, "on" ) ) {
    return 0;
  }
  debug_message("Validation failed: checkbox check", ERROR);
  return 1;
}

//
static int checkDate(char *val) {
  struct dateParts *dp = dateStringToDateParts(val);
  int x=0;

  if(checkStringIsInt(dp->year)) x++;
  else
    if(checkSaneRange(dp->year, 1850, 2038)) x++;
  free(dp->year);

  if(checkStringIsInt(dp->month)) x++;
  else
    if(checkSaneRange(dp->month, 1, 12)) x++;
  free(dp->year);

  if(checkStringIsInt(dp->day)) x++;
  else
    if(checkSaneRange(dp->day, 1, 31)) x++;
  free(dp->year);

  free(dp);
  if( x > 0 ) debug_message("Validation failed: date check", ERROR);
  return x;
}

static int checkDeviceId(char *val) {
  return 0;
}

//
static int checkFormat(char *val) {
  if ( 0 != strcmp(val, "grey scale") ) return 0;
  debug_message("Validation failed: scan format check", ERROR);
  return 1;
}

//
static int checkPageLength(char *val) {
  if(checkStringIsInt(val)) return 1;
  if(checkSaneRange(val, 20, 100)) return 1;
  return 0;
}

//
static int checkPages(char *val) {
  if(checkStringIsInt(val)) return 1;
  if(checkSaneRange(val, 1, 10)) return 1;
  return 0;
}

//
static int checkResolution(char *val) {
  if(checkStringIsInt(val)) return 1;
  if(checkSaneRange(val, 50, 3000)) return 1;
  return 0;
}

//
static int checkSkew(char *val) {
  if(checkStringIsInt(val)) return 1;
  if(checkSaneRange(val, -50, 50)) return 1;
  return 0;
}

//
static int checkTag(char *val) {
  if(checkStringIsInt(val)) return 1;
  if(checkSaneRange(val, 1, 9999)) return 1;
  return 0;
}

static int checkKey(char *val) {
  return 0;
}

static int checkVal(char *val) {
  return 0;
}



/*************************************************8
 * Checks on each calling method
 */
extern int validate(GHashTable *postdata, char *action) {

  int ret = 0;

  if ( 0 == strcmp(action, "getDocList") ) {
    ret += checkOnlyKeys(postdata, "");
  }

  if ( 0 == strcmp(action, "getDocDetail") ) {
    ret += checkOnlyKeys(postdata, "docid");
    ret += checkDocId(getPostData(postdata, "docid"));
  }

  if ( 0 == strcmp(action, "getScannerList") ) {
    ret += checkOnlyKeys(postdata, "");
  }

  if ( 0 == strcmp(action, "doScan") ) {
    ret += checkOnlyKeys(postdata, "deviceid,format,skew,resolution,pages,ocr,pageLength");
    ret += checkDeviceId(getPostData(postdata, "deviceid"));
    ret += checkFormat(getPostData(postdata, "format"));
    ret += checkSkew(getPostData(postdata, "skew"));
    ret += checkResolution(getPostData(postdata, "resolution"));
    ret += checkPages(getPostData(postdata, "pages"));
    ret += checkCheckbox(getPostData(postdata, "ocr"));
    ret += checkPageLength(getPostData(postdata, "pagelength"));
  }

  if ( 0 == strcmp(action, "getScanningProgress") ) {
    ret += checkOnlyKeys(postdata, "scanprogressid");
    ret += checkUUID(getPostData(postdata, "scanprogressid"));
  }

  if ( 0 == strcmp(action, "nextPageReady") ) {
    ret += checkOnlyKeys(postdata, "scanprogressid");
    ret += checkUUID(getPostData(postdata, "scanprogressid"));
  }

  if ( 0 == strcmp(action, "updateDocDetails") ) {
    ret += checkOnlyKeys(postdata, "docid,kkey,vvalue");
    ret += checkDocId(getPostData(postdata, "docid"));
    ret += checkKey(getPostData(postdata, "kkey"));
    ret += checkVal(getPostData(postdata, "vvalue"));
  }

  if ( 0 == strcmp(action, "moveTag") ) {
    ret += checkOnlyKeys(postdata, "docid,tagid,add_remove");
    ret += checkDocId(getPostData(postdata, "docid"));
    ret += checkTag(getPostData(postdata, "tagid"));
    ret += checkAddRemove(getPostData(postdata, "add_remove"));
  }

  if ( 0 == strcmp(action, "filter") ) {
    ret += checkOnlyKeys(postdata, "textSearch,startDate,endDate");
    ret += checkVal(getPostData(postdata, "textSearch"));
    ret += checkDate(getPostData(postdata, "startDate"));
    ret += checkDate(getPostData(postdata, "endDate"));
  }

  if ( 0 == strcmp(action, "deletedoc") ) {
    ret += checkOnlyKeys(postdata, "docid");
    ret += checkDocId(getPostData(postdata, "docid"));
  }

  if ( 0 == strcmp(action, "getAudio") ) {
    ret += checkOnlyKeys(postdata, "");
  }

  return ret;
}
