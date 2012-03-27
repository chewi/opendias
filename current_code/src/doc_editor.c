/*
 * doc_editor.c
 * Copyright (C) Clearscene Ltd 2008 <wbooth@essentialcollections.co.uk>
 * 
 * doc_editor.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * doc_editor.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "config.h"
#include "db.h"
#include "dbaccess.h"
#include "doc_editor.h"
#ifdef CAN_SPEAK
#include "speak.h"
#endif // CAN_SPEAK //
#include "main.h"
#include "utils.h"
#include "debug.h"
#ifdef CAN_READODF
#include "read_odf.h"
#endif // CAN_READODF //

#include <sys/types.h>
#include <sys/dir.h>
#include <sys/param.h>
 
//#define FALSE 0
//#define TRUE !FALSE

extern char *doDelete (char *documentId) {

  int pages, i;
  char *docTemplate, *docPath;


  char *sql = o_printf("SELECT pages FROM docs WHERE docid = %s", documentId);
  struct simpleLinkedList *rSet = runquery_db(sql);
  if( rSet != NULL ) {
    char *pages_s = o_strdup(readData_db(rSet, "pages"));
    pages = atoi(pages_s);
    o_log(INFORMATION, "%s", pages_s);
    free(pages_s);
  } else {
    o_log(ERROR, "Could not select record.");
    free_recordset( rSet );
    free(sql);
    return NULL;
  }
  free_recordset( rSet );
  free(sql);

  docTemplate = o_strdup("%s/scans/%s_%i.jpg");
  for(i = 1 ; i <= pages ; i++) {
    docPath = o_printf(docTemplate, BASE_DIR, documentId, i);
    o_log(INFORMATION, "%s", docPath);
    unlink(docPath);
    free(docPath);
  }
  free(docTemplate);

  removeDocTags(documentId);
  removeDoc(documentId);

  return o_strdup("<?xml version='1.0' encoding='iso-8859-1'?>\n<Response><DeleteDoc><status>OK</status></DeleteDoc></Response>");;
}


#ifdef CAN_SPEAK
void readTextParser () {

  char *textToRead = "";

  textToRead = o_strdup("TEST TEXT");
  readText(textToRead);
  free(textToRead);

}
#endif // CAN_SPEAK //






extern char *getDocDetail (char *documentId) {

  struct simpleLinkedList *rSet;
  char *sql, *tags, *tagsTemplate, *title, *humanReadableDate,
      *returnXMLtemplate, *returnXML;

  // Validate document id
  //
  sql = o_printf("SELECT docid FROM docs WHERE docid = %s", documentId);
  rSet = runquery_db(sql);
  if( rSet == NULL ) {
    o_log(ERROR, "Could not select record %s.", documentId);
    free_recordset( rSet );
    free(sql);
    return NULL;
  }
  free_recordset( rSet );
  free(sql);



  // Get a list of tags
  //
  tags = o_strdup("");
  tagsTemplate = o_strdup("<tag>%s</tag>");
  sql = o_printf(
    "SELECT tagname \
    FROM tags JOIN \
    (SELECT * \
    FROM doc_tags \
    WHERE docid=%s) dt \
    ON tags.tagid = dt.tagid \
    ORDER BY tagname", documentId);

  rSet = runquery_db(sql);
  if( rSet ) {
    do  {
      o_concatf(&tags, tagsTemplate, readData_db(rSet, "tagname") );
    } while ( nextRow( rSet ) );
  }
  free_recordset( rSet );
  free(sql);
  free(tagsTemplate);



  // Get docinformation
  //
  sql = o_printf("SELECT * FROM docs WHERE docid = %s", documentId);
  rSet = runquery_db(sql);
  if( rSet == NULL ) {
    o_log(ERROR, "Could not select record.");
    free_recordset( rSet );
    free(sql);
    return NULL;
  }

  // Build Human Readable
  //
  title = o_strdup(readData_db(rSet, "title"));
  if( 0 == strcmp(title, "NULL") ) {
    free(title);
    title = o_strdup("New (untitled) document.");
  }

  humanReadableDate = dateHuman( o_strdup(readData_db(rSet, "docdatey")),
                                 o_strdup(readData_db(rSet, "docdatem")),
                                 o_strdup(readData_db(rSet, "docdated")) );



  // Build Response
  //
  //use UTF-8 encoding to be inline with webpage encoding. returnXMLtemplate = o_strdup("<?xml version='1.0' encoding='iso-8859-1'?>
  returnXMLtemplate = o_strdup("<?xml version='1.0' encoding='UTF-8'?>\
<Response>\
 <DocDetail>\
  <docid>%s</docid>\
  <title><![CDATA[%s]]></title>\
  <scanDate>%s</scanDate>\
  <type>%s</type>\
  <docDate>%s</docDate>\
  <pages>%s</pages>\
  <extractedText><![CDATA[%s]]></extractedText>\
  <x>%s</x>\
  <y>%s</y>\
  <Tags>%s</Tags>\
  <hardcopyKept>%s</hardcopyKept>\
  <actionrequired>%s</actionrequired>\
 </DocDetail>\
</Response>");
  returnXML = o_printf(returnXMLtemplate, 
                            documentId, title, readData_db(rSet, "entrydate"), readData_db(rSet, "filetype"), 
                            humanReadableDate, readData_db(rSet, "pages"), readData_db(rSet, "ocrtext"), 
                            readData_db(rSet, "ppl"), readData_db(rSet, "lines"), tags, readData_db(rSet, "hardcopyKept"),
			    readData_db(rSet, "actionrequired") );

  free_recordset(rSet);
  free(sql);
  free(returnXMLtemplate);
  free(title);
  free(humanReadableDate);
  free(tags);

  return returnXML;
}



extern char *updateDocDetails(char *docid, char *kkey, char *vvalue) {

  int rc = 0;

  if( 0 == strcmp(kkey, "docDate") ) {

    struct dateParts *dp = dateStringToDateParts(vvalue);

    // Save Year
    rc = updateDocValue(docid, "docdatey", dp->year);
    free(dp->year);

    // Save Month
    if(rc==0) {
      rc = updateDocValue(docid, "docdatem", dp->month);
      free(dp->month);
    }

    // Save Day
    if(rc==0) {
      rc = updateDocValue(docid, "docdated", dp->day);
      free(dp->day);
    }
    free(dp);
  } 

  else if ( 0 == strcmp(kkey, "actionrequired") ) {
    if( vvalue && 0 == strcmp(vvalue, "true") ) {
      rc = updateDocValue_int(docid, kkey, 1);
    }
    else {
      rc = updateDocValue_int(docid, kkey, 0);
    }
  } 

	else if ( 0 == strcmp(kkey, "hardcopyKept") ) {
		if ( vvalue && 0 == strcmp(vvalue,"true") ) {
			rc = updateDocValue_int(docid,kkey ,1);
		} else {
			rc = updateDocValue_int(docid,kkey, 0);
		}
	}

  else 
    rc = updateDocValue(docid, kkey, vvalue);

  if(rc) return NULL;
  else return o_strdup("<?xml version='1.0' encoding='iso-8859-1'?>\n<Response><UpdateDocDetails><status>OK</status></UpdateDocDetails></Response>");
}

extern char *updateTagLinkage(char *docid, char *tag, char *subaction) {

  int rc = 0;

  // Check if their is a tag of this name, if not add one
  char *tagid = getTagId( tag );

  if(0 == strcmp(subaction, "addTag")) {
    rc = addTagToDoc(docid, tagid);
  }
  else if(0 == strcmp(subaction, "removeTag")) {
    rc = removeTagFromDoc (docid, tagid);
    if( 0 == countDocsWithTag( tagid ) )
      deleteTag( tagid );
  }
  else {
    o_log(ERROR, "Unknown subaction.");
    rc = 1;
  }

  free(tagid);
  if(rc == 1) return NULL;
  else return o_strdup("<?xml version='1.0' encoding='iso-8859-1'?>\n<Response><MoveTag><status>OK</status></MoveTag></Response>");
}


