// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WIN32_IE 0x0600
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <shlobj.h>
#endif

#include "compat.hpp"

#ifdef __APPLE__
# include "osxbits.h"
#endif

#if defined(__WATCOMC__)
# include <direct.h>
#elif !defined(_MSC_VER)
# include <dirent.h>
#endif

#if defined(__linux) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#  include <libgen.h> // for dirname()
#endif

#if defined(__FreeBSD__)
#  include <sys/sysctl.h> // for sysctl() to get path to executable
#endif

#if defined(__HAIKU__)
#  include <kernel/image.h>
#  include <libgen.h>
#endif

#include <algorithm>
#include <cctype>

#if !defined(_WIN32)
char *strlwr(char *s)
{
    char *t = s;
    if (!s) return s;
    while (*t) { *t = std::tolower(*t); t++; }
    return s;
}

char *strupr(char *s)
{
    char *t = s;
    if (!s) return s;
    while (*t) { *t = std::toupper(*t); t++; }
    return s;
}
#endif


int Bvasprintf(char **ret, const char *format, va_list ap)
{
    va_list app;

    va_copy(app, ap);
    int len = vsnprintf(nullptr, 0, format, app);
    va_end(app);

    if (len < 0)
		return -1;

    if ((*ret = static_cast<char*>(std::malloc(len + 1))) == nullptr)
		return -1;

    va_copy(app, ap);
    len = vsnprintf(*ret, len + 1, format, app);
    va_end(app);

    return len;
}


/**
 * Get the location of the user's home/profile data directory.
 * The caller must free the string when done with it.
 * @return nullptr if it could not be determined
 */
char *Bgethomedir()
{
    char *dir = nullptr;

#ifdef _WIN32
	TCHAR appdata[MAX_PATH];

	if (SUCCEEDED(::SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appdata))) {
		dir = strdup(appdata);
    }

#elif defined __APPLE__
    dir = osx_gethomedir();
    
#else
	char *e = getenv("HOME");
	if (e) {
        dir = strdup(e);
    }
#endif

	return dir;
}

/**
 * Get the location of the application directory.
 * On OSX this is the .app bundle resource directory.
 * On Windows this is the directory the executable was launched from.
 * On Linux/BSD it's the executable's directory
 * The caller must free the string when done with it.
 * @return nullptr if it could not be determined
 */
char *Bgetappdir()
{
    char *dir = nullptr;
    
#ifdef _WIN32
	TCHAR appdir[MAX_PATH];
    
	if (::GetModuleFileName(nullptr, appdir, MAX_PATH) > 0) {
		// trim off the filename
		char *slash = strrchr(appdir, '\\');
		if (slash) slash[0] = 0;
		dir = strdup(appdir);
    }

#elif defined __APPLE__
    dir = osx_getappdir();
    
#elif defined(__linux) || defined(__NetBSD__) || defined(__OpenBSD__)
    char buf[PATH_MAX] = {0};
    char buf2[PATH_MAX] = {0};
#  ifdef __linux
    snprintf(buf, sizeof(buf), "/proc/%d/exe", getpid());
#  else // the BSDs.. except for FreeBSD which has a sysctl
    snprintf(buf, sizeof(buf), "/proc/%d/file", getpid());
#  endif
    int len = readlink(buf, buf2, sizeof(buf2));
    if (len != -1) {
        // remove executable name with dirname(3)
        // on Linux, dirname() will modify buf2 (cutting off executable name) and return it
        // on FreeBSD it seems to use some internal buffer instead.. anyway, just strdup()
        dir = strdup(dirname(buf2));
    }
#elif defined(__FreeBSD__)
    // the sysctl should also work when /proc/ is not mounted (which seems to
    // be common on FreeBSD), so use it..
    char buf[PATH_MAX] = {0};
    int name[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    size_t len = sizeof(buf)-1;
    int ret = sysctl(name, sizeof(name)/sizeof(name[0]), buf, &len, nullptr, 0);
    if(ret == 0 && buf[0] != '\0') {
        // again, remove executable name with dirname()
        // on FreeBSD dirname() seems to use some internal buffer
        dir = strdup(dirname(buf));
    }
#elif defined(__HAIKU__)
    image_info info;
    int32 cookie = 0;
    while (get_next_image_info(0, &cookie, &info) == B_OK) {
        if (info.type != B_APP_IMAGE) continue;
        dir = strdup(dirname(info.name));
        break;
    }
#endif

    return dir;
}

/**
 * Get the location for global or user-local support files.
 * The caller must free the string when done with it.
 * @return nullptr if it could not be determined
 */
char *Bgetsupportdir(int global)
{
    char *dir = nullptr;
    
#ifdef __APPLE__
    dir = osx_getsupportdir(global);

#else
    if (!global) {
        dir = Bgethomedir();
    }
#endif
    
	return dir;
}

int Bcorrectfilename(char *filename, int removefn)
{
	char *fn;
	constexpr int MAXTOKARR{ 64 };
	char *tokarr[64];
	char *first;
	char *next;
	char *token;
	int i;
	int ntok = 0;
	int leadslash = 0;
	int trailslash = 0;
	
	fn = strdup(filename);
	if (!fn) return -1;
	
	// find the end of the string
	for (first=fn; *first; first++) {
#ifdef _WIN32
		// translating backslashes to forwardslashes on the way
		if (*first == '\\') *first = '/';
#endif
	}
	leadslash = (*fn == '/');
	trailslash = (first>fn && first[-1] == '/');
	
	// carve up the string into pieces by directory, and interpret
	// the . and .. components
	first = fn;
	do {
		token = Bstrtoken(first, "/", &next, 1);
		first = nullptr;
		if (!token) break;
		else if (token[0] == 0) continue;
		else if (token[0] == '.' && token[1] == 0) continue;
		else if (token[0] == '.' && token[1] == '.' && token[2] == 0) ntok = std::max(0, ntok - 1);
		else tokarr[ntok++] = token;
	} while (ntok < MAXTOKARR);
	
	if (!trailslash && removefn) {
		ntok = std::max(0, ntok - 1);
		trailslash = 1;
	}

	if (ntok == 0 && trailslash && leadslash) {
		trailslash = 0;
	}
	
	// rebuild the filename
	first = filename;
	if (leadslash) *(first++) = '/';
	for (i=0; i<ntok; i++) {
		if (i>0) *(first++) = '/';
		for (token=tokarr[i]; *token; token++)
			*(first++) = *token;
	}
	if (trailslash) *(first++) = '/';
	*(first++) = 0;
	
	std::free(fn);

	return 0;
}

int Bcanonicalisefilename(char *filename, int removefn)
{
	char cwd[BMAX_PATH];
	char fn[BMAX_PATH];
	char* p{ nullptr };
	char* fnp{ filename };
#ifdef _WIN32
	int drv = 0;
#endif
	
#ifdef _WIN32
	{
		if (filename[0] && filename[1] == ':') {
			// filename is prefixed with a drive
			drv = std::toupper(filename[0])-'A' + 1;
			fnp += 2;
		}
		if (!_getdcwd(drv, cwd, sizeof(cwd))) return -1;
		for (p=cwd; *p; p++) if (*p == '\\') *p = '/';
	}
#else
	if (!getcwd(cwd,sizeof(cwd))) return -1;
#endif
	p = strrchr(cwd,'/'); if (!p || p[1]) std::strcat(cwd, "/");
	
	std::strcpy(fn, fnp);
#ifdef _WIN32
	for (p=fn; *p; p++) if (*p == '\\') *p = '/';
#endif
	
	if (fn[0] != '/') {
		// we are dealing with a path relative to the current directory
		std::strcpy(filename, cwd);
		std::strcat(filename, fn);
	} else {
#ifdef _WIN32
		filename[0] = cwd[0];
		filename[1] = ':';
		filename[2] = 0;
#else
		filename[0] = 0;
#endif
		std::strcat(filename, fn);
	}
	fnp = filename;
#ifdef _WIN32
	fnp += 2;	// skip the drive
#endif
	
	return Bcorrectfilename(fnp,removefn);
}

char *Bgetsystemdrives()
{
#ifdef _WIN32
	char *str;
	char *p;
	DWORD drv;
	DWORD mask;
	int number=0;
	
	drv = ::GetLogicalDrives();
	if (drv == 0) return nullptr;

	for (mask=1; mask<0x8000000L; mask<<=1) {
		if ((drv&mask) == 0) continue;
		number++;
	}

	str = p = (char *)std::malloc(1 + (3*number));
	if (!str) return nullptr;

	number = 0;
	for (mask=1; mask<0x8000000L; mask<<=1, number++) {
		if ((drv&mask) == 0) continue;
		*(p++) = 'A' + number;
		*(p++) = ':';
		*(p++) = 0;
	}
	*(p++) = 0;

	return str;
#else
	// Perhaps have Unix OS's put /, /home/user, and /mnt/* in the "drives" list?
	return nullptr;
#endif
}


off_t Bfilelength(int fd)
{
#ifdef _MSC_VER
	return (off_t)_filelength(fd);
#else
	struct stat st;
	if (fstat(fd, &st) < 0) return -1;
	return(st.st_size);
#endif
}


struct BDIR_real {
#ifdef _MSC_VER
	HANDLE hfind;
	WIN32_FIND_DATA fid;
#else
	DIR *dir;
#endif
	struct Bdirent info;
	int status;
};

BDIR* Bopendir(const char *name)
{
	BDIR_real *dirr;

#ifdef _MSC_VER
	char *tname, *tcurs;
#endif

	dirr = (BDIR_real*)std::malloc(sizeof(BDIR_real));
	if (!dirr) {
		return nullptr;
	}
	
	std::memset(dirr, 0, sizeof(BDIR_real));

#ifdef _MSC_VER
	tname = (char*)std::malloc(std::strlen(name) + 4 + 1);
	if (!tname) {
		std::free(dirr);
		return nullptr;
	}

	std::strcpy(tname, name);
	for (tcurs = tname; *tcurs; tcurs++) ;	// Find the end of the string.
	tcurs--;	// Step back off the null char.
	while (*tcurs == ' ' && tcurs>tname) tcurs--;	// Remove any trailing whitespace.
	if (*tcurs != '/' && *tcurs != '\\') *(++tcurs) = '/';
	*(++tcurs) = '*';
	*(++tcurs) = '.';
	*(++tcurs) = '*';
	*(++tcurs) = 0;
	
	dirr->hfind = ::FindFirstFile(tname, &dirr->fid);
	std::free(tname);
	if (dirr->hfind == INVALID_HANDLE_VALUE) {
		std::free(dirr);
		return nullptr;
	}
#else
	dirr->dir = opendir(name);
	if (dirr->dir == nullptr) {
		std::free(dirr);
		return nullptr;
	}
#endif

	dirr->status = 0;

	return (BDIR*)dirr;
}

struct Bdirent*	Breaddir(BDIR *dir)
{
	BDIR_real *dirr = (BDIR_real*)dir;

#ifdef _MSC_VER
	LARGE_INTEGER tmp;

	if (dirr->status > 0) {
		if (::FindNextFile(dirr->hfind, &dirr->fid) == 0) {
			dirr->status = -1;
			return nullptr;
		}
	}
	dirr->info.namlen = std::strlen(dirr->fid.cFileName);
	dirr->info.name = (char *)dirr->fid.cFileName;

	dirr->info.mode = 0;
	if (dirr->fid.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) dirr->info.mode |= S_IFDIR;
	else dirr->info.mode |= S_IFREG;
	if (dirr->fid.dwFileAttributes & FILE_ATTRIBUTE_READONLY) dirr->info.mode |= S_IREAD;
	else dirr->info.mode |= S_IREAD|S_IWRITE|S_IEXEC;

	tmp.HighPart = dirr->fid.nFileSizeHigh;
	tmp.LowPart = dirr->fid.nFileSizeLow;
	dirr->info.size = (off_t)tmp.QuadPart;

	tmp.HighPart = dirr->fid.ftLastWriteTime.dwHighDateTime;
	tmp.LowPart = dirr->fid.ftLastWriteTime.dwLowDateTime;
	tmp.QuadPart -= INT64_C(116444736000000000);
	dirr->info.mtime = (time_t)(tmp.QuadPart / 10000000);

	dirr->status++;

#else
	struct dirent *de;
	struct stat st;

	de = readdir(dirr->dir);
	if (de == nullptr) {
		dirr->status = -1;
		return nullptr;
	} else {
		dirr->status++;
	}

	dirr->info.namlen = std::strlen(de->d_name);
	dirr->info.name   = de->d_name;
	dirr->info.mode = 0;
	dirr->info.size = 0;
	dirr->info.mtime = 0;

	if (!fstatat(dirfd(dirr->dir), de->d_name, &st, 0)) {
		dirr->info.mode = st.st_mode;
		dirr->info.size = st.st_size;
 		dirr->info.mtime = st.st_mtime;
	}
#endif

	return &dirr->info;
}

int Bclosedir(BDIR *dir)
{
	BDIR_real *dirr = (BDIR_real*)dir;

#ifdef _MSC_VER
	FindClose(dirr->hfind);
#else
	closedir(dirr->dir);
#endif
	std::free(dirr);

	return 0;
}


char *Bstrtoken(char *s, const char* delim, char **ptrptr, int chop)
{
	char *p;
	char *start;

	if (!ptrptr) return nullptr;
	
	if (s) p = s;
	else p = *ptrptr;

	if (!p) return nullptr;

	while (*p != 0 && std::strchr(delim, *p)) p++;
	if (*p == 0) {
		*ptrptr = nullptr;
		return nullptr;
	}
	start = p;
	while (*p != 0 && !std::strchr(delim, *p)) p++;
	if (*p == 0) *ptrptr = nullptr;
	else {
		if (chop) *(p++) = 0;
		*ptrptr = p;
	}

	return start;
}


	//Brute-force case-insensitive, slash-insensitive, * and ? wildcard matcher
	//Given: string i and string j. string j can have wildcards
	//Returns: 1:matches, 0:doesn't match
int Bwildmatch (const char *i, const char *j)
{
	const char *k;
	char c0;
	char c1;

	if (!*j) return(1);
	do
	{
		if (*j == '*')
		{
			for(k=i,j++;*k;k++) if (Bwildmatch(k,j)) return(1);
			continue;
		}
		if (!*i) return(0);
		if (*j == '?') { i++; j++; continue; }
		c0 = *i; if ((c0 >= 'a') && (c0 <= 'z')) c0 -= 32;
		c1 = *j; if ((c1 >= 'a') && (c1 <= 'z')) c1 -= 32;
#ifdef _WIN32
		if (c0 == '/') c0 = '\\';
		if (c1 == '/') c1 = '\\';
#endif
		if (c0 != c1) return(0);
		i++; j++;
	} while (*j);
	return(!*i);
}


//
// getsysmemsize() -- gets the amount of system memory in the machine
//
size_t Bgetsysmemsize()
{
#ifdef _WIN32
	size_t siz{0x7fffffff};
	
        MEMORYSTATUSEX memst;
        memst.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memst)) {
            siz = (size_t)std::min(DWORDLONG(0x7fffffff), memst.ullTotalPhys);
        }
	
	return siz;
#elif (defined(_SC_PAGE_SIZE) || defined(_SC_PAGESIZE)) && defined(_SC_PHYS_PAGES)
	size_t siz = 0x7fffffff;
	long scpagesiz, scphyspages;

#ifdef _SC_PAGE_SIZE
	scpagesiz = sysconf(_SC_PAGE_SIZE);
#else
	scpagesiz = sysconf(_SC_PAGESIZE);
#endif
	scphyspages = sysconf(_SC_PHYS_PAGES);
	if (scpagesiz >= 0 && scphyspages >= 0)
		siz = (size_t)std::min(INT64_C(0x7fffffff), (int64_t)scpagesiz * (int64_t)scphyspages);

	//buildprintf("Bgetsysmemsize(): %d pages of %d bytes, %d bytes of system memory\n",
	//		scphyspages, scpagesiz, siz);

	return siz;
#else
	return 0x7fffffff;
#endif
}



