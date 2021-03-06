/*
 * saneDispatcher.h
 * Copyright (C) Clearscene Ltd 2008 <wbooth@essentialcollections.co.uk>
 * 
 * main.h is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * saneDispatcher.h is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SANEDISPATCHER
#define SANEDISPATCHER

#include "config.h"

#ifdef CAN_SCAN

#define ADDRESS "/tmp/opendias"

struct doScanOpData {
  char *uuid;
  char *lang;
};
extern void dispatch_sane_work(int);
extern char *send_command(char *);
extern void freeSaneCache(void);
#endif /* CAN_SCAN */

#endif /* SANEDISPATCHER */
