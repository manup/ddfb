#include <time.h>


U_time U_TimeNow(void)
{
    U_time result;
    struct timespec ts;

    /* timespec_get() since C11 */
#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 201112L && !defined(_WIN32)
    if (timespec_get(&ts, TIME_UTC) == 0)
    {
        U_ASSERT(0 && "timespec_get error");
        return 0;
    }
#else
    /* POSIX.1-2008 */
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return 0;
#endif

    result = ts.tv_sec;
    result *= 1000;

    ts.tv_nsec /= 1000000; /* nano- to milliseconds */
    result += ts.tv_nsec;

    return result;
}

/* Output format: YYYY-MM-DDTHH:MM:SS.164Z

   2022-07-16T12:39:33.164Z
*/
int U_TimeToISO8601_UTC(U_time time, void *buf, unsigned len)
{
    time_t t;
    struct tm tmt;
    U_SStream ss;
    int millisec;

    if (len)
        ((char*)buf)[0] = '\0';

    if (len < 25)
        return 0;

    t = time / 1000;
    millisec = time % 1000;

    errno = 0;
    if (!gmtime_r(&t, &tmt) ||  errno)
        return 0;

    U_sstream_init(&ss, (char*)buf, len);
    U_sstream_put_i32(&ss, tmt.tm_year + 1900);

    U_sstream_put_str(&ss, "-");
    if (tmt.tm_mon + 1 < 10)
        U_sstream_put_str(&ss, "0");
    U_sstream_put_i32(&ss, tmt.tm_mon + 1);

    U_sstream_put_str(&ss, "-");
    if (tmt.tm_mday < 10)
        U_sstream_put_str(&ss, "0");
    U_sstream_put_i32(&ss, tmt.tm_mday);

    U_sstream_put_str(&ss, "T");
    if (tmt.tm_hour < 10)
        U_sstream_put_str(&ss, "0");
    U_sstream_put_i32(&ss, tmt.tm_hour);

    U_sstream_put_str(&ss, ":");
    if (tmt.tm_min < 10)
        U_sstream_put_str(&ss, "0");
    U_sstream_put_i32(&ss, tmt.tm_min);

    U_sstream_put_str(&ss, ":");
    if (tmt.tm_sec < 10)
        U_sstream_put_str(&ss, "0");
    U_sstream_put_i32(&ss, tmt.tm_sec);

    U_sstream_put_str(&ss, ".");
    if (millisec < 10)
        U_sstream_put_str(&ss, "00");
    else if (millisec < 100)
        U_sstream_put_str(&ss, "0");
    U_sstream_put_i32(&ss, millisec);

    U_sstream_put_str(&ss, "Z");

    return 1;
}

/* ISO 8601 extended format

    Supported formats:

    YYYY-MM-DDTHH:MM:SS.sssZ
    YYYY-MM-DDTHH:MM:SSZ
    YYYY-MM-DDTHH:MMZ
    YYYY-MM-DDTHHZ
    YYYY-MM-DD

    2022-07-16T12:39:33.164Z
    2022-07-16T12:39:33.164
    2022-07-16T12:39:33Z
    2022-07-16T12:39Z
    2022-07-16T12Z

    Timezones:
      Z
      ±hh:mm
      ±hhmm
      ±hh
*/
#ifdef _WIN32
  /* https://stackoverflow.com/questions/16647819/timegm-cross-platform */
  #define timegm _mkgmtime
#endif

U_time U_TimeFromISO8601(const char *str, unsigned len)
{
    U_time result;
    U_SStream ss;
    struct tm time;
    int millisec;

    U_bzero(&time, sizeof(time));
    millisec = 0;
    time.tm_isdst = -1;
    U_sstream_init(&ss, (char*)str, len);

    time.tm_year = U_sstream_get_i32(&ss, 10);
    if (ss.pos != 4 || U_sstream_peek_char(&ss) != '-' || time.tm_year < 1900)
        goto err;
    time.tm_year -= 1900;
    U_sstream_seek(&ss, ss.pos + 1);

    time.tm_mon = U_sstream_get_i32(&ss, 10);
    if (ss.pos != 7 || U_sstream_peek_char(&ss) != '-' || time.tm_mon < 1 || time.tm_mon > 12)
        goto err;
    time.tm_mon -= 1;
    U_sstream_seek(&ss, ss.pos + 1);

    time.tm_mday = U_sstream_get_i32(&ss, 10);
    if (ss.pos != 10 || time.tm_mday < 1 || time.tm_mday > 31) /* 32 (?) */
        goto err;

    if (U_sstream_peek_char(&ss) == 'T')
    {
        U_sstream_seek(&ss, ss.pos + 1);

        time.tm_hour = U_sstream_get_i32(&ss, 10);
        if (ss.pos != 13 || time.tm_hour < 0 || time.tm_hour > 23)
            goto err;

        if (U_sstream_peek_char(&ss) == ':') /* optional minutes */
        {
            U_sstream_seek(&ss, ss.pos + 1);

            time.tm_min = U_sstream_get_i32(&ss, 10);
            if (ss.pos != 16 || time.tm_min < 0 || time.tm_min > 59)
                goto err;

            if (U_sstream_peek_char(&ss) == ':') /* optional seconds */
            {
                U_sstream_seek(&ss, ss.pos + 1);

                time.tm_sec = U_sstream_get_i32(&ss, 10);
                if (ss.pos != 19 || time.tm_sec < 0 || time.tm_sec > 60)
                    goto err;

                if (time.tm_sec == 60)
                {
                    time.tm_sec -= 1;
                    /* todo leap second */
                }

                if (U_sstream_peek_char(&ss) == '.' || U_sstream_peek_char(&ss) == ',')
                {
                    U_sstream_seek(&ss, ss.pos + 1);
                    millisec = U_sstream_get_i32(&ss, 10);
                }
            }
        }
    }

    errno = 0;
    if (U_sstream_peek_char(&ss) == 'Z')
    {
        result = timegm(&time) * 1000;
    }
    else if (U_sstream_peek_char(&ss) == '+' || U_sstream_peek_char(&ss) == '-')
    {
        result = 0;
        U_ASSERT(0 && "timezone not supported yet");
        /* timezone offset
            +-hh:mm
            +-hh
        */
    }
    else
    {
        result = mktime(&time) * 1000;
    }

    if (errno)
    {
        result = 0;
    }
    else
    {
        result += millisec;
    }

    return result;

err:
    result = 0;

    return result;
}

void U_TestTime(void)
{
    int i;
    U_time ttt;
    char tbuf[32];

    const char *iso[] = {
        "2022-07-16T12:39:33.164Z",
        "2022-07-16T12:39:33Z",
        "2022-07-16T12:39Z",
        "2022-07-16T17Z",
        "2022-07-16Z",
        /* "2022-07-16T12:39+02:00", */
        NULL,
    };

    for (i = 0; iso[i]; i++)
    {
        ttt = U_TimeFromISO8601(iso[i], U_strlen(iso[i]));
        if (ttt == 0)
        {
            U_Printf("parse ISO 8601: '%s' [FAIL]\n", iso[i]);
        }
        else
        {
            U_Printf("parse ISO 8601: '%s' (%ld) [OK]\n", iso[i], ttt);
        }
        U_ASSERT(ttt != 0);
    }

    ttt = U_TimeNow();
    U_ASSERT(ttt != 0);
    if (U_TimeToISO8601_UTC(ttt, &tbuf[0], sizeof(tbuf)))
    {
        U_Printf("ISO 8601 NOW '%s'\n", tbuf);
    }

    /* convert back and forth yields same string */
    ttt = U_TimeFromISO8601(iso[0], U_strlen(iso[0]));
    U_TimeToISO8601_UTC(ttt, &tbuf[0], sizeof(tbuf));
    U_ASSERT(U_memcmp(&tbuf[0], iso[0], U_strlen(iso[0])) == 0);
}
