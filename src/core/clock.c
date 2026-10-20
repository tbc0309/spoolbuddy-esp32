// SPDX-License-Identifier: AGPL-3.0-or-later
#include "clock.h"
#include <stdio.h>
#include <string.h>
static int leap(int year) {return year%4==0 && (year%100!=0 || year%400==0);}
int64_t sb_http_date_epoch(const char *date) {
    char weekday[4],month[4];int day,year,hour,minute,second,n=0;
    if(!date || sscanf(date,"%3s, %d %3s %d %d:%d:%d GMT%n",weekday,&day,month,&year,&hour,&minute,&second,&n)!=7 ||
       date[n] || year<2020 || year>2100 || hour<0 || hour>23 || minute<0 || minute>59 || second<0 || second>59)return -1;
    const char *months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};int m=0;
    while(m<12 && strcmp(months[m],month))m++;
    if(m==12 || day<1 || day>days[m]+(m==1 && leap(year)))return -1;
    int64_t total=0;
    for(int y=1970;y<year;y++)total+=365+leap(y);
    for(int i=0;i<m;i++)total+=days[i]+(i==1 && leap(year));
    return ((total+day-1)*24+hour)*3600+minute*60+second;
}
