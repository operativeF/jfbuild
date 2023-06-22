/*
 * File Tokeniser/Parser/Whatever
 * by Jonathon Fowler
 * Remixed completely by Ken Silverman
 * See the included license file "BUILDLIC.TXT" for license info.
 */

#include "build.hpp"
#include "scriptfile.hpp"
#include "cache1d.hpp"
#include "string_utils.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace {

constexpr bool is_whitespace(auto ch) {
	return (ch == ' ') || (ch == '\t') || (ch == '\r') || (ch == '\n');
};

void skipoverws(scriptfile *sf) { if ((sf->textptr < sf->eof) && (!sf->textptr[0])) sf->textptr++; }
void skipovertoken(scriptfile *sf) { while ((sf->textptr < sf->eof) && (sf->textptr[0])) sf->textptr++; }

} // namespace

char *scriptfile_gettoken(scriptfile *sf)
{
	skipoverws(sf);

	if (sf->textptr >= sf->eof) {
		return nullptr;
	}

	char* start = sf->ltextptr = sf->textptr;
	skipovertoken(sf);
	return start;
}

char *scriptfile_peektoken(const scriptfile *sf)
{
	scriptfile dupe = *sf;

	skipoverws(&dupe);
	if (dupe.textptr >= dupe.eof) return nullptr;
	return dupe.textptr;
}

int scriptfile_getstring(scriptfile *sf, std::string& retst)
{
	retst = scriptfile_gettoken(sf);
	if (retst.empty())
	{
		buildprintf("Error on line {}:{}: unexpected eof\n", sf->filename, scriptfile_getlinum(sf,sf->textptr));
		return(-2);
	}
	return(0);
}

namespace {

int scriptfile_getnumber_radix(scriptfile *sf, int *num, int radix)
{
	skipoverws(sf);
	if (sf->textptr >= sf->eof)
	{
		buildprintf("Error on line {}:{}: unexpected eof\n", sf->filename, scriptfile_getlinum(sf,sf->textptr));
		return -1;
	}

	while ((sf->textptr[0] == '0') && (sf->textptr[1] >= '0') && (sf->textptr[1] <= '9'))
		sf->textptr++; //hack to treat octal numbers like decimal
	
	sf->ltextptr = sf->textptr;

	if(radix == 0) {
		radix = 10;
	}

	std::string_view txtv{(sf->textptr)};

	auto [ptr, ec] = std::from_chars(txtv.data(), txtv.data() + txtv.size(), *num, radix);
	sf->textptr = sf->textptr + txtv.size();
	
	if (!is_whitespace(*sf->textptr) && *sf->textptr || (ec != std::errc{})) {
		const char *p = sf->textptr;
		skipovertoken(sf);
		buildprintf("Error on line {}:{}: expecting int, got \"{}\"\n",sf->filename,scriptfile_getlinum(sf,sf->ltextptr), txtv);
		return -2;
	}

	return 0;
}

} // namespace

int scriptfile_getnumber(scriptfile *sf, int *num)
{
	return scriptfile_getnumber_radix(sf, num, 0);
}

int scriptfile_gethex(scriptfile *sf, int *num)
{
	return scriptfile_getnumber_radix(sf, num, 16);
}

int scriptfile_getbool(scriptfile* sf, bool* b)
{
	const auto* boolean_val = scriptfile_gettoken(sf);

	if (boolean_val == nullptr)
	{
		buildprintf("Error on line {}:{}: unexpected eof\n",sf->filename,scriptfile_getlinum(sf, sf->textptr));
		return -2;
	}

	std::string_view boolean_strv{boolean_val};

	if(boolean_strv == "true")
		*b = true;
	else if(boolean_strv == "false")
		*b = false;
	else {
		buildprintf("Error on line {}:{}: expecting bool (true / false), got \"{}\"\n",sf->filename, scriptfile_getlinum(sf, sf->textptr), boolean_strv);
		return -2;
	}

	return 0;	
}

namespace {

double parsedouble(char *ptr, char **end)
{	

	int negative{0};
	char* p{ptr};

	if (*p == '-') {
		negative = 1;
		p++;
	}
	else if (*p == '+')
		p++;

	bool beforedecimal{true};
	int expo{0};
	int exposgn{0};

	double decpl{0.1};
	double num{0.0};

	for (;; p++) {
		if (*p >= '0' && *p <= '9') {
			const int dig = *p - '0';
			if (beforedecimal)
				num = num * 10.0 + dig;
			else if (exposgn)
				expo = expo*10 + dig;
			else {
				num += (double)dig * decpl;
				decpl /= 10.0;
			}
		}
		else if (*p == '.') {
			if (beforedecimal)
				beforedecimal = false;
			else
				break;
		}
		else if ((*p == 'E') || (*p == 'e')) {
			exposgn = 1;
			if (p[1] == '-') {
				exposgn = -1;
				p++;
			}
			else if (p[1] == '+')
				p++;
		}
		else
			break;
	}
	
	if (end)
		*end = p;

	if (exposgn)
		num *= pow(10.0,(double)(expo*exposgn));

	return negative ? -num : num;
}

} // namespace

int scriptfile_getdouble(scriptfile *sf, double *num)
{
	skipoverws(sf);
	if (sf->textptr >= sf->eof)
	{
		buildprintf("Error on line {}:{}: unexpected eof\n",sf->filename,scriptfile_getlinum(sf,sf->textptr));
		return -1;
	}
	
	sf->ltextptr = sf->textptr;

	// On Linux, locale settings interfere with interpreting x.y format numbers
	//(*num) = strtod((const char *)sf->textptr,&sf->textptr);
	(*num) = parsedouble(sf->textptr, &sf->textptr);
	
	if (!is_whitespace(*sf->textptr) && *sf->textptr) {
		char *p = sf->textptr;
		skipovertoken(sf);
		buildprintf("Error on line {}:{}: expecting float, got \"{}\"\n",sf->filename,scriptfile_getlinum(sf,sf->ltextptr),p);
		return -2;
	}
	return 0;
}

int scriptfile_getsymbol(scriptfile *sf, int *num)
{
	char* t = scriptfile_gettoken(sf);

	if (!t)
		return -1;

	const std::string_view tok{t};
	int val{0};
	auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), val);

	if (ec != std::errc{}) {
		// looks like a string, so find it in the symbol table
		if (scriptfile_getsymbolvalue(t, num)) return 0;
		buildprintf("Error on line {}:{}: expecting symbol, got \"{}\"\n",sf->filename, scriptfile_getlinum(sf,sf->ltextptr),t);
		return -2;   // not found
	}

	*num = val;

	return 0;
}

int scriptfile_getbraces(scriptfile *sf, char **braceend)
{
	skipoverws(sf);

	if (sf->textptr >= sf->eof)
	{
		buildprintf("Error on line {}:{}: unexpected eof\n",sf->filename,scriptfile_getlinum(sf,sf->textptr));
		return -1;
	}

	if (sf->textptr[0] != '{') {
		buildprintf("Error on line {}:{}: expecting '{'\n",sf->filename,scriptfile_getlinum(sf,sf->textptr));
		return -1;
	}

	char* bracestart = ++sf->textptr;
	int bracecnt{1};

	while (1)
	{
		if (sf->textptr >= sf->eof)
			return 0;

		if (sf->textptr[0] == '{')
			bracecnt++;

		if (sf->textptr[0] == '}') {
			bracecnt--;
			if (!bracecnt)
				break;
		}

		sf->textptr++;
	}

	(*braceend) = sf->textptr;
	sf->textptr = bracestart;

	return 0;
}

int scriptfile_getlinum (scriptfile *sf, char *ptr)
{
	//for(i=0;i<sf->lineoffs.size();i++) if (sf->lineoffs[i] >= ind) return(i+1); //brute force algo

	const ptrdiff_t ind = ((intptr_t)ptr) - ((intptr_t)sf->txbuffer.data());

	int stp{1};
	for(; stp + stp < sf->lineoffs.size(); stp += stp); //stp = highest power of 2 less than sf->linenum
	
	int i{0};
	for(; stp; stp >>= 1)
		if ((i + stp < sf->lineoffs.size()) && (sf->lineoffs[i + stp] < ind))
			i += stp;
	
	return i + 1; //i = index to highest lineoffs which is less than ind; convert to 1-based line numbers
}

namespace {

void scriptfile_preparse(scriptfile *sf, std::string tx, size_t flen)
{
	//Count number of lines
	int numcr{1};

	int cr{0};
	for(std::size_t i{0}; i < flen; ++i)
	{
			//detect all 4 types of carriage return (\r, \n, \r\n, \n\r :)
		cr = 0;
		
		if (tx[i] == '\r') {
			i += (tx[i+1] == '\n');
			cr = 1;
		}
		else if (tx[i] == '\n') {
			i += (tx[i+1] == '\r');
			cr = 1;
		}

		if (cr) {
			numcr++;
			continue;
		}
	}

	sf->lineoffs.reserve(numcr);

	//Preprocess file for comments (// and /*...*/, and convert all whitespace to single spaces)
	int nflen{0};
	int ws{0};
	int cs{0};
	int inquote{0};
	
	for(int i{}; i < flen; ++i)
	{
			//detect all 4 types of carriage return (\r, \n, \r\n, \n\r :)
		cr = 0;
		
		if (tx[i] == '\r') { i += (tx[i+1] == '\n');
			cr = 1; }
		else if (tx[i] == '\n') { i += (tx[i+1] == '\r');
			cr = 1; }

		if (cr)
		{
				//Remember line numbers by storing the byte index at the start of each line
				//Line numbers can be retrieved by doing a binary search on the byte index :)
			sf->lineoffs.push_back(nflen);
			
			if (cs == 1)
				cs = 0;
			ws = 1;
			continue; //strip CR/LF
		}

		if ((!inquote) && ((tx[i] == ' ') || (tx[i] == '\t'))) {
			ws = 1;
			continue;
		} //strip Space/Tab
		
		if ((tx[i] == ';') && (!cs))
			cs = 1;	// ; comment

		if ((tx[i] == '/') && (tx[i+1] == '/') && (!cs))
			cs = 1;

		if ((tx[i] == '/') && (tx[i+1] == '*') && (!cs)) {
			ws = 1;
			cs = 2;
		}

		if ((tx[i] == '*') && (tx[i+1] == '/') && (cs == 2)) {
			cs = 0;
			i++;
			continue;
		}

		if (cs)
			continue;

		if (ws) {
			tx[nflen++] = 0;
			ws = 0;
		}

			//quotes inside strings: \"
		if ((tx[i] == '\\') && (tx[i+1] == '\"')) {
			i++;
			tx[nflen++] = '\"';
			continue;
		}

		if (tx[i] == '\"') {
			inquote ^= 1;
			continue;
		}
		
		tx[nflen++] = tx[i];
	}

	tx[nflen++] = 0;
	sf->lineoffs.push_back(nflen);
	tx[nflen++] = 0;

#if 0
		//for debugging only:
	std::printf("pre-parsed file:flen={},nflen={}\n",flen,nflen);
	for(i=0;i<nflen;i++) { if (tx[i] < 32) std::printf("_"); else std::printf("{}",tx[i]); }
	std::printf("[eof]\nnumlines={}\n",sf->lineoffs.size());
	for(i=0;i<sf->lineoffs.size();i++) std::printf("line {} = byte {}\n",i,sf->lineoffs[i]);
#endif
	flen = nflen;

	sf->txbuffer = tx;

	sf->textptr = sf->txbuffer.data();
	sf->eof = &sf->txbuffer[nflen - 1];
}

} // namespace

std::unique_ptr<scriptfile> scriptfile_fromfile(const std::string& fn)
{
	const int fp = kopen4load(fn.c_str(), 0);

	if (fp < 0)
		return nullptr;

	const unsigned int flen = kfilelength(fp);
	std::string tx;
	tx.resize(flen + 2);

	auto sf = std::make_unique<scriptfile>();

	kread(fp, &tx[0], flen);
	tx[flen] = 0;
	tx[flen+1] = 0;

	kclose(fp);

	scriptfile_preparse(sf.get(), tx, flen);
	sf->filename = fn;

	return sf;
}

std::unique_ptr<scriptfile> scriptfile_fromstring(const std::string& str)
{
	if (str.empty())
		return nullptr;

	const auto flen = str.length();

	std::string tx;
	tx.resize(flen + 2);

	auto sf = std::make_unique<scriptfile>();

	if (!sf) {
		return nullptr;
	}

	tx = str;

	tx[flen] = 0;
	tx[flen + 1] = 0;

	scriptfile_preparse(sf.get(), tx, flen);
	sf->filename.clear();

	return sf;
}

int scriptfile_eof(scriptfile *sf)
{
	skipoverws(sf);

	if (sf->textptr >= sf->eof)
		return 1;

	return 0;
}

namespace {

constexpr std::size_t SYMBTABSTARTSIZE{256};
size_t symbtablength=0, symbtaballoclength=0;
char *symbtab = nullptr;

char* getsymbtabspace(size_t reqd)
{
	if (symbtablength + reqd > symbtaballoclength)
	{
		auto i = std::max(symbtaballoclength, SYMBTABSTARTSIZE);
		
		for(; symbtablength + reqd > i; i <<= 1);
		
		auto* np = (char *) std::realloc(symbtab, i);
		
		if (!np)
			return nullptr;

		symbtab = np;
		symbtaballoclength = i;
	}

	char* pos = &symbtab[symbtablength];
	symbtablength += reqd;
	
	return pos;
}

} // namespace

int scriptfile_getsymbolvalue(const char *name, int *val)
{
	char* scanner = symbtab;

	if (!symbtab)
		return 0;
	
	while (scanner - symbtab < (ptrdiff_t)symbtablength) {
		if (IsSameAsNoCase(name, scanner)) {
			*val = *(int*)(scanner + std::strlen(scanner) + 1);
			return 1;
		}

		scanner += std::strlen(scanner) + 1 + sizeof(int);
	}

	return 0;
}

int scriptfile_addsymbolvalue(const char *name, int val)
{
	//int x;
	//if (scriptfile_getsymbolvalue(name, &x)) return -1;   // already exists

	if (symbtab) {
		char *scanner = symbtab;
		while (scanner - symbtab < (ptrdiff_t)symbtablength) {
			if (IsSameAsNoCase(name, scanner)) {
				*(int*)(scanner + std::strlen(scanner) + 1) = val;
				return 1;
			}

			scanner += std::strlen(scanner) + 1 + sizeof(int);
		}
	}
	
	auto* sp = getsymbtabspace(std::strlen(name) + 1 + sizeof(int));

	if (!sp)
		return 0;

	std::strcpy(sp, name);
	sp += std::strlen(name)+1;
	*(int*)sp = val;

	return 1;   // added
}

void scriptfile_clearsymbols()
{
	if (symbtab)
		std::free(symbtab);

	symbtab = nullptr;
	symbtablength = 0;
	symbtaballoclength = 0;
}
