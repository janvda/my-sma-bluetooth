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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sma_struct.h"
#include <time.h>

MYSQL *conn;
MYSQL_RES *res;

void OpenMySqlDatabase (char *server, char *user, char *password, char *database)
{
  conn = mysql_init(NULL);
  // Connect to database
  if (!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0)) {
    fprintf(stderr, "%s\n", mysql_error(conn));
    exit(1);
  }
}

void CloseMySqlDatabase()
{
  /* Release memory used to store results and close connection */
  mysql_free_result(res);
  mysql_close(conn);
}

void DoQuery (char *query){
  /* execute query */

  if (mysql_real_query(conn, query, strlen(query))) {
    fprintf(stderr, "ERROR: %s\n", mysql_error(conn));
    exit(1);
  }
  res = mysql_store_result(conn);
}

int install_mysql_tables( ConfType * conf, FlagType * flag, char *SCHEMA )
/*  Do initial mysql table creationsa */
{
  int found=0;
  MYSQL_ROW row;
  char SQLQUERY[1000];

  OpenMySqlDatabase( conf->MySqlHost, conf->MySqlUser, conf->MySqlPwd, "mysql");
  //Get Start of day value
  sprintf(SQLQUERY,"SHOW DATABASES" );
  if (flag->debug == 1) printf("%s\n",SQLQUERY);
  DoQuery(SQLQUERY);
  while ((row = mysql_fetch_row(res))) { //if there is a result, update the row
    if( strcmp( row[0], conf->MySqlDatabase ) == 0 )
    {
      found=1;
      printf( "Database already exists - exiting" );
    }
  }
  if( found == 0 ) {
    // Create the database structure
    sprintf( SQLQUERY,"CREATE DATABASE IF NOT EXISTS %s", conf->MySqlDatabase );
    if (flag->debug == 1) printf("%s\n",SQLQUERY);
    DoQuery(SQLQUERY);

    sprintf( SQLQUERY,"USE  %s", conf->MySqlDatabase );
    if (flag->debug == 1) printf("%s\n",SQLQUERY);
    DoQuery(SQLQUERY);

    sprintf( SQLQUERY,"CREATE TABLE `Almanac` ( `id` bigint(20) NOT NULL \
      AUTO_INCREMENT, \
      `date` date NOT NULL,\
      `sunrise` datetime DEFAULT NULL,\
      `sunset` datetime DEFAULT NULL,\
      `CHANGETIME` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, \
       PRIMARY KEY (`id`),\
       UNIQUE KEY `date` (`date`)\
       ) ENGINE=MyISAM" );
    if (flag->debug == 1) printf("%s\n",SQLQUERY);
    DoQuery(SQLQUERY);

    sprintf( SQLQUERY, "CREATE TABLE `DayData` ( \
      `DateTime` datetime NOT NULL, \
      `Inverter` varchar(30) NOT NULL, \
      `Serial` varchar(40) NOT NULL, \
      `CurrentPower` int(11) DEFAULT NULL, \
      `ETotalToday` DECIMAL(10,3) DEFAULT NULL, \
      `Voltage` DECIMAL(10,3) DEFAULT NULL, \
      `PVOutput` datetime DEFAULT NULL, \
      `CHANGETIME` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE CURRENT_TIMESTAMP, \
      PRIMARY KEY (`DateTime`,`Inverter`,`Serial`) \
      ) ENGINE=MyISAM" );
    if (flag->debug == 1) printf("%s\n",SQLQUERY);
    DoQuery(SQLQUERY);

    sprintf( SQLQUERY, "CREATE TABLE `LiveData` ( \
      `id` bigint(20) NOT NULL  AUTO_INCREMENT, \
      `DateTime` datetime NOT NULL, \
      `Inverter` varchar(30) NOT NULL, \
      `Serial` varchar(40) NOT NULL, \
      `Description` varchar(30) NOT NULL, \
      `Value` varchar(30) NOT NULL, \
      `Units` varchar(20) DEFAULT NULL, \
      `CHANGETIME` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE CURRENT_TIMESTAMP, \
      PRIMARY KEY (`id`), \
      UNIQUE KEY 'DateTime'(`DateTime`,`Inverter`,`Serial`,`Description`) \
      ) ENGINE=MyISAM" );
    if (flag->debug == 1) printf("%s\n",SQLQUERY);
    DoQuery(SQLQUERY);

    sprintf( SQLQUERY, "CREATE TABLE `settings` ( \
      `value` varchar(128) NOT NULL, \
      `data` varchar(500) NOT NULL, \
      PRIMARY KEY (`value`) \
      ) ENGINE=MyISAM" );
    if (flag->debug == 1) printf("%s\n",SQLQUERY);
    DoQuery(SQLQUERY);
     
    sprintf( SQLQUERY, "INSERT INTO `settings` SET `value` = \'schema\', `data` = \'%s\' ", SCHEMA );
    if (flag->debug == 1) printf("%s\n",SQLQUERY);
    DoQuery(SQLQUERY);
  }
  mysql_close(conn);
  return found;
}

void update_mysql_tables( ConfType * conf, FlagType * flag )
/*  Do mysql table schema updates */
{
  int schema_value=0, result;
  MYSQL_ROW row;
  char SQLQUERY[1000];

  OpenMySqlDatabase( conf->MySqlHost, conf->MySqlUser, conf->MySqlPwd, "mysql");
  sprintf( SQLQUERY,"USE  %s", conf->MySqlDatabase );
  if (flag->debug == 1) printf("%s\n",SQLQUERY);
  DoQuery(SQLQUERY);

  /*Check current schema value*/
  sprintf(SQLQUERY,"SELECT data FROM settings WHERE value=\'schema\' " );
  if (flag->debug == 1) printf("%s\n",SQLQUERY);
  DoQuery(SQLQUERY);
  if ((row = mysql_fetch_row(res))) {  //if there is a result, update the row
    schema_value=atoi(row[0]);
  }
  mysql_free_result(res);
  if( schema_value == 1 ) { //Upgrade from 1 to 2
    sprintf(SQLQUERY,"ALTER TABLE `DayData` CHANGE `ETotalToday` `ETotalToday` DECIMAL(10,3) NULL DEFAULT NULL" );
    if (flag->debug == 1) printf("%s\n",SQLQUERY);
    DoQuery(SQLQUERY);      
    if (flag->debug == 1) printf("SQL res = \n",res);

    sprintf( SQLQUERY, "UPDATE `settings` SET `value` = \'schema\', `data` = 2 " );
    if (flag->debug == 1) printf("%s\n",SQLQUERY);
    DoQuery(SQLQUERY);
  }

  /*Check current schema value*/
  sprintf(SQLQUERY,"SELECT data FROM settings WHERE value=\'schema\' " );
  if (flag->debug == 1) printf("%s\n",SQLQUERY);
  DoQuery(SQLQUERY);
  if ((row = mysql_fetch_row(res))) {  //if there is a result, update the row
    schema_value=atoi(row[0]);
  }
  mysql_free_result(res);
  if( schema_value == 2 ) { //Upgrade from 2 to 3
      sprintf(SQLQUERY,"CREATE TABLE `LiveData` ( \
        `id` BIGINT NOT NULL AUTO_INCREMENT , \
        `DateTime` datetime NOT NULL, \
        `Inverter` varchar(10) NOT NULL, \
        `Serial` varchar(40) NOT NULL, \
        `Description` char(20) NOT NULL , \
        `Value` INT NOT NULL , \
        `Units` char(20) NOT NULL , \
        `CHANGETIME` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE CURRENT_TIMESTAMP, \
        UNIQUE KEY (`DateTime`,`Inverter`,`Serial`,`Description`), \
        PRIMARY KEY ( `id` ) \
        ) ENGINE = MYISAM" );
      if (flag->debug == 1) printf("%s\n",SQLQUERY);
      DoQuery(SQLQUERY);
      sprintf( SQLQUERY, "UPDATE `settings` SET `value` = \'schema\', `data` = 3 " );
      if (flag->debug == 1) printf("%s\n",SQLQUERY);
      DoQuery(SQLQUERY);
  }

  /*Check current schema value*/
  sprintf(SQLQUERY,"SELECT data FROM settings WHERE value=\'schema\' " );
  if (flag->debug == 1) printf("%s\n",SQLQUERY);
  DoQuery(SQLQUERY);
  if ((row = mysql_fetch_row(res))) {  //if there is a result, update the row
    schema_value=atoi(row[0]);
  }
  mysql_free_result(res);
  if( schema_value == 3 ) { //Upgrade from 3 to 4
    sprintf(SQLQUERY,"ALTER TABLE `DayData` CHANGE `Inverter` `Inverter` varchar(30) NOT NULL, CHANGE `Serial` `Serial` varchar(40) NOT NULL" );
    if (flag->debug == 1) printf("%s\n",SQLQUERY);
    DoQuery(SQLQUERY);
    sprintf(SQLQUERY,"ALTER TABLE `LiveData` CHANGE `Inverter` `Inverter` varchar(30) NOT NULL, CHANGE `Serial` `Serial` varchar(40) NOT NULL, CHANGE `Description` `Description` varchar(30) NOT NULL, CHANGE `Value` `Value` varchar(30), CHANGE `Units` `Units` varchar(20) NULL DEFAULT NULL " );
    if (flag->debug == 1) printf("%s\n",SQLQUERY);
    DoQuery(SQLQUERY);
    sprintf( SQLQUERY, "UPDATE `settings` SET `value` = \'schema\', `data` = 4 " );
    if (flag->debug == 1) printf("%s\n",SQLQUERY);
    DoQuery(SQLQUERY);
  }
  printf("Database schema up to date (version 4)\n");
  mysql_close(conn);
}

int check_schema( ConfType * conf, FlagType * flag, char *SCHEMA )
/*  Check if using the correct database schema */
{
  int found=0;
  MYSQL_ROW row;
  char SQLQUERY[200];
  char DB_SCHEMA[20];

  OpenMySqlDatabase( conf->MySqlHost, conf->MySqlUser, conf->MySqlPwd, conf->MySqlDatabase);
  //Get Start of day value
  sprintf(SQLQUERY,"SELECT data FROM settings WHERE value=\'schema\' " );
  if (flag->debug == 1) printf("%s\n",SQLQUERY);
  DoQuery(SQLQUERY);
  if ((row = mysql_fetch_row(res))) { //if there is a result, update the row
    strcpy(DB_SCHEMA, row[0]);
    if( strcmp( DB_SCHEMA, SCHEMA ) == 0 )
      found=1;
  }
  mysql_free_result(res);
  mysql_close(conn);
  if( found != 1 ) {
    printf( "Please Update database schema by using --UPDATE (DB scheme = %s, application scheme = %s)\n", DB_SCHEMA, SCHEMA );
  }
  return found;
}


void live_mysql( ConfType conf, FlagType flag, LiveDataType *livedatalist, int livedatalen )
/* Live inverter values mysql update */
{
  struct tm *utctime;
  char SQLQUERY[2000];
  char datetime[40];
  int day,month,year,hour,minute,second;
  int live_data=1;
  int i;
  MYSQL_ROW row;

  OpenMySqlDatabase( conf.MySqlHost, conf.MySqlUser, conf.MySqlPwd, conf.MySqlDatabase);
  for( i=0; i<livedatalen; i++ ) {
	// Storing in Inverter timezone (mostly set to UTC)
    utctime = gmtime(&((livedatalist+i)->date));
    day = utctime->tm_mday;
    month = utctime->tm_mon +1;
    year = utctime->tm_year + 1900;
    hour = utctime->tm_hour;
    minute = utctime->tm_min;
    sprintf( datetime, "%04d-%02d-%02d %02d:%02d:00", year, month, day, hour, minute );
    if( flag.debug == 1 ) printf( "utc datetime = %s\n", datetime);    
	sprintf(SQLQUERY,"INSERT INTO LiveData ( DateTime, Inverter, Serial, Description, Value, Units ) VALUES (\'%s\', \'%s\', %lld, \'%s\', \'%s\', \'%s\'  ) ON DUPLICATE KEY UPDATE DateTime=Datetime, Inverter=VALUES(Inverter), Serial=VALUES(Serial), Description=VALUES(Description), Description=VALUES(Description), Value=VALUES(Value), Units=VALUES(Units)", datetime, (livedatalist+i)->inverter, (livedatalist+i)->serial, (livedatalist+i)->Description, (livedatalist+i)->Value, (livedatalist+i)->Units);
	if (flag.debug == 1) printf("Live Data SQL query: %s\n",SQLQUERY);
	DoQuery(SQLQUERY);
  }
  mysql_close(conn);
  if (flag.debug == 1) printf("End live_mysql\n");
}
