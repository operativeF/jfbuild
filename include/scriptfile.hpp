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

char *scriptfile_gettoken(std::unique_ptr<scriptfile>& sf);
char *scriptfile_peektoken(const std::unique_ptr<scriptfile>& sf);
std::optional<int> scriptfile_getnumber(std::unique_ptr<scriptfile>& sf);
std::optional<bool> scriptfile_getbool(std::unique_ptr<scriptfile>& sf);
std::optional<int> scriptfile_gethex(std::unique_ptr<scriptfile>& sf);    // For reading specifically hex without requiring an 0x prefix
std::optional<double> scriptfile_getdouble(std::unique_ptr<scriptfile>& sf);
std::optional<std::string_view> scriptfile_getstring(std::unique_ptr<scriptfile>& sf);
int scriptfile_getsymbol(std::unique_ptr<scriptfile>& sf, int *num);
int scriptfile_getlinum(std::unique_ptr<scriptfile>& sf, char *ptr);
int scriptfile_getbraces(std::unique_ptr<scriptfile>& sf, char **braceend);

std::unique_ptr<scriptfile> scriptfile_fromfile(const std::string& fn);
std::unique_ptr<scriptfile> scriptfile_fromstring(const std::string& str);
int scriptfile_eof(std::unique_ptr<scriptfile>& sf);

int scriptfile_getsymbolvalue(const char *name, int *val);
int scriptfile_addsymbolvalue(const char *name, int val);
void scriptfile_clearsymbols();

#endif // __scriptfile_h__
