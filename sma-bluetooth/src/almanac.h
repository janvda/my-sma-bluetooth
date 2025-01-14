/* tool to read power production data for SMA solar power convertors 
   Copyright Wim Hofman 2010 
   Copyright Stephen Collier 2010,2011 
   Copyright Edwin Zuidema 2020, 2021

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include "sma_struct.h"

extern char *sunrise( ConfType *conf, int debug );
extern char *sunset( ConfType *conf, int debug );
extern int todays_almanac( ConfType *conf, int debug );
extern void update_almanac( ConfType *conf, char * sunrise, char * sunset, int debug );
