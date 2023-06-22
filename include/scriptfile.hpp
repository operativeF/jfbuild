#ifndef __scriptfile_h__
#define __scriptfile_h__

#include <memory>
#include <optional>
#include <string>
#include <vector>

struct scriptfile {
    char *ltextptr;     // pointer to start of the last token fetched (use this for line numbers)
    char *textptr;
    char *eof;
    
    std::string filename;
    std::string txbuffer;
    std::vector<int> lineoffs;
};

char *scriptfile_gettoken(scriptfile *sf);
char *scriptfile_peektoken(const scriptfile *sf);
int scriptfile_getnumber(scriptfile *sf, int *num);
std::optional<bool> scriptfile_getbool(scriptfile* sf);
int scriptfile_gethex(scriptfile *sf, int *num);    // For reading specifically hex without requiring an 0x prefix
std::optional<double> scriptfile_getdouble(scriptfile *sf);
int scriptfile_getstring(scriptfile *sf, std::string& st);
int scriptfile_getsymbol(scriptfile *sf, int *num);
int scriptfile_getlinum(scriptfile *sf, char *ptr);
int scriptfile_getbraces(scriptfile *sf, char **braceend);

std::unique_ptr<scriptfile> scriptfile_fromfile(const std::string& fn);
std::unique_ptr<scriptfile> scriptfile_fromstring(const std::string& str);
int scriptfile_eof(scriptfile *sf);

int scriptfile_getsymbolvalue(const char *name, int *val);
int scriptfile_addsymbolvalue(const char *name, int val);
void scriptfile_clearsymbols();

#endif // __scriptfile_h__
