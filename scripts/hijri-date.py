#!/usr/sbin/env python

from calverter import Calverter

import time
import sys


def wday_to_hday(wday):
    if wday == 0:
        return('دوشنبه')
    elif wday == 1:
        return('سه‌شنبه')
    elif wday == 2:
        return('چهارشنبه')
    elif wday == 3:
        return('پنج‌شنبه')
    elif wday == 4:
        return('جمعه')
    elif wday == 5:
        return('شنبه')
    else:
        return('یک‌شنبه')


def wday_to_gday(wday):
    if wday == 0:
        return('Mon')
    elif wday == 1:
        return('Tue')
    elif wday == 2:
        return('Wed')
    elif wday == 3:
        return('Thu')
    elif wday == 4:
        return('Fri')
    elif wday == 5:
        return('Sat')
    else:
        return('Sun')


def hijri_date():
    gr_date = time.strptime(time.ctime())
    cal = Calverter()
    jd_today = cal.gregorian_to_jd(
        gr_date.tm_year,
        gr_date.tm_mon,
        gr_date.tm_mday
    )
    jal_today = list(cal.jd_to_jalali(jd_today))
    list.append(jal_today, wday_to_hday(gr_date.tm_wday))
    print(
        str(gr_date.tm_hour) +
        ":" +
        str(gr_date.tm_min) +
        ":" +
        str(gr_date.tm_sec) +
        " " +
        str(jal_today[3]) +
        " " +
        str(jal_today[0]) +
        "/" +
        str(jal_today[1]) +
        "/" +
        str(jal_today[2])
    )


def gregorian_date():
    today = time.strptime(time.ctime())
    print(
        str(today.tm_year) +
        '/' +
        str(today.tm_mon) +
        '/' +
        str(today.tm_mday) +
        ' ' +
        wday_to_gday(today.tm_wday) +
        ' ' +
        str(today.tm_hour) +
        ':' +
        str(today.tm_min) +
        ':' +
        str(today.tm_sec)
    )


def main():
    if sys.argv[1] == "h":
        hijri_date()
    elif sys.argv[1] == "g":
        gregorian_date()
    else:
        return('Error!')

if __name__ == "__main__":
    main()
else:
    print('Error!')
