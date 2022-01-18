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

#include <mysql/mysql.h>
#include "sma_struct.h"

extern MYSQL *conn;
extern MYSQL_RES *res;

extern void OpenMySqlDatabase(char *, char *, char *, char * );
extern void CloseMySqlDatabase();
extern int DoQuery(char *);
extern int install_mysql_tables( ConfType *, FlagType *,  char * );
extern void update_mysql_tables( ConfType *, FlagType *  );
extern int check_schema( ConfType *, FlagType *,  char * );
extern void live_mysql( ConfType *, FlagType *, LiveDataType *, int );
