#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef enum LogType
{
   Warning,
   Error,
   Info
} LogType;

static const char* RedColor =  "\033[31m";
static const char* YellowColor = "\033[34m";
static const char* GreenColor = "\033[32m";
static const char* DefaultColor = "\033[0m";

// I guess, it should be made variadic
static inline void Log(LogType log_lvl, const char* fmt, ...)
{
    // Not much experience with variadic macros here, did long time ago and forgot
#ifdef DEBUG
    va_list args,args2;
    va_start(args,fmt);
    va_copy(args2,args);
    char buffer[512] = {'\0'};
    switch (log_lvl)
    {
        case Warning : {
            strcpy(buffer,YellowColor);
            strcpy(buffer+strlen(YellowColor),"[Warning] : ");
            break;
        }
        case Error : {
            strcpy(buffer,RedColor);
            strcpy(buffer+strlen(buffer), "[Error]   : ");
            break;
        }
        case Info : {
            strcpy(buffer,GreenColor);
            strcpy(buffer+strlen(buffer), "[Info]    : ");
            break;
        }
    }
    strcpy(buffer+strlen(buffer),DefaultColor);

    unsigned len = vsnprintf(NULL,0,fmt,args2);
    vsnprintf(buffer + strlen(buffer),512 - strlen(buffer),fmt,args);
    va_end(args);
    fprintf(stdout,"%s\n",buffer);
#endif
}

#endif // LOG_H_
