//$Id: aux_utils.h 5847 2009-01-21 16:09:27Z antonn $
#ifndef _AUX_UTILS_H_INCLUDED_
#define _AUX_UTILS_H_INCLUDED_

#include <ctype.h>

#if defined(WIN32)
#pragma warning(push,3)
#pragma warning(disable:4242)
#endif

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <stdexcept>

#if defined(WIN32)
#pragma warning(pop)
#endif

using namespace std;

//GCC has a bug related to use of toupper/tolower
//in transform.
//See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=11108
//This is a workaround for the bug.
static int toupper_gcc_workaround(int c) 
    {return toupper(c);}
static int tolower_gcc_workaround(int c) 
    {return tolower(c);}

inline string strToUpper(string str)
{
    std::transform(str.begin(), str.end(), str.begin(), toupper_gcc_workaround);
    return str;
}

inline string strToLower(string str)
{
    std::transform(str.begin(), str.end(), str.begin(), tolower_gcc_workaround);
    return str;
}

inline int stringToint( const string& str )
{
    std::istringstream s( str );
    int ret(0);
    s >> ret;

    return (s.rdstate() & std::ios::failbit) ? 0 : ret;
}

inline string intTostring( int value )
{
    std::ostringstream s;
    s << value;
    return s.str();
}

inline string GetFileSystemPathSeparator()
{
#ifdef WIN32
    return string("\\");
#elif defined(LINUX)
    return string("/");
#else
#error Unknown operating system
#endif
};

class CStrTokenizer
{
public:
    class iterator
    {
        friend class CStrTokenizer;
        public:
            iterator( const iterator& other )
                : m_tokenizer(other.m_tokenizer)
                , m_token(other.m_token)
                , m_pos(other.m_pos)
            {}
            const string& get() const
            { return m_token; }
            iterator& operator = (const iterator& other);
            iterator& operator ++();
            operator bool() const;
        private:
            iterator(
                const CStrTokenizer& tokenizer,
                const string& separator,
                size_t position );
            mutable string m_separator;
            const CStrTokenizer& m_tokenizer;
            string m_token;
            size_t m_pos;
    };
    friend class iterator;
    CStrTokenizer( const string& str ) 
        : m_string(str)
    {}
    iterator begin( const string& separator ) const
    {
        return iterator( *this, separator, 0);
    }
private:
    bool operator != (const CStrTokenizer& other) const
    {
        return this != &other;
    }
    string m_string;
};

void GetDirectoryContents(const string& strDir, string strFileMask, 
                          vector<string>& Result, bool fFilesOnly);

string GetCurrDir();

class exc_basic : public exception
{
 public:
  virtual ~exc_basic() throw() {
  }
  
  virtual const char *what() const throw() {
    return m_str.c_str();
  }
  
protected:
  string m_str;
};

class exc_assert : public exc_basic
{
public:
  exc_assert (const char* str, 
              const char *fname  = NULL, 
              int         str_no = 0) {
    ostringstream ss;
    ss << "Assertion failed : " << str;
    if (fname) {
      ss << fname << " : ";
    }
    if (str_no) {
      ss << str_no << " : ";
    }
    m_str = ss.str();
  }
};

#define EXC_ASSERT(expr)                                        \
  {                                                             \
    if (!(expr)) {                                              \
      throw exc_assert(#expr, __FILE__, __LINE__);              \
    }                                                           \
  }

#endif //_AUX_UTILS_H_INCLUDED_

