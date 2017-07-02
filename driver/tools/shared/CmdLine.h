//$Id: CmdLine.h 5808 2009-01-18 13:37:27Z antonn $
#ifndef _CMDLINE_H_INCLUDED_
#define _CMDLINE_H_INCLUDED_

#if defined(WIN32)
#pragma warning(push,3)
#endif

#include <string>
#include <utility>

#if defined(WIN32)
#pragma warning(pop)
#endif


using namespace std;

class CCmdLine
{
private:
    const int m_argc;
    char * const *m_argv;
    char* isCmdLineParamInternal(const string& paramName);
    static const char CMD_LINE_DELIMITER;
    const CCmdLine& operator=(const CCmdLine& other); //Do not allow assignments
public:
    typedef std::pair<string, string> ParamName;

    CCmdLine(int argc, char* const argv[]);

    bool isCmdLineParam(const CCmdLine::ParamName& paramName);
    string getParamValue(const CCmdLine::ParamName& paramName);
    int getIntParamValue(const CCmdLine::ParamName& paramName);
};


#endif //_CMDLINE_H_INCLUDED_
