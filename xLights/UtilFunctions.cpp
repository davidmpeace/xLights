#include <wx/filename.h>
#include <wx/config.h>
#include <wx/regex.h>
#include <wx/sckaddr.h>
#include <wx/dir.h>
#include <wx/socket.h>

std::string DecodeIPError(wxSocketError err)
{
    switch (err)
    {
    case wxSOCKET_NOERROR:
        return "No Error";
    case wxSOCKET_INVOP:
        return "Invalid Operation";
    case wxSOCKET_IOERR:
        return "IO Error";
    case wxSOCKET_INVADDR:
        return "Invalid Address";
    case wxSOCKET_INVSOCK:
        return "Invalid Socket";
    case wxSOCKET_NOHOST:
        return "No Host";
    case wxSOCKET_INVPORT:
        return "Invalid Port";
    case wxSOCKET_WOULDBLOCK:
        return "Would Block";
    case wxSOCKET_TIMEDOUT:
        return "Timeout";
    case wxSOCKET_MEMERR:
        return "Memory Error";
    case wxSOCKET_OPTERR:
        return "Option Error";
    default:
        return "God knows what happened";
    }
}

#include "UtilFunctions.h"

#include <log4cpp/Category.hh>

std::string DecodeMidi(int midi)
{
    int n = midi % 12;
    int o = midi / 12 - 1;
    // dont go below zero ... if so move it up an octave ... the user will never know
    while (o < 0)
    {
        o++;
    }

    bool sharp = false;
    char note = '?';
    switch (n)
    {
    case 9:
        note = 'A';
        break;
    case 10:
        note = 'A';
        sharp = true;
        break;
    case 11:
        note = 'B';
        break;
    case 0:
        note = 'C';
        break;
    case 1:
        note = 'C';
        sharp = true;
        break;
    case 2:
        note = 'D';
        break;
    case 3:
        note = 'D';
        sharp = true;
        break;
    case 4:
        note = 'E';
        break;
    case 5:
        note = 'F';
        break;
    case 6:
        note = 'F';
        sharp = true;
        break;
    case 7:
        note = 'G';
        break;
    case 8:
        note = 'G';
        sharp = true;
        break;
    default:
        break;
    }

    if (sharp)
    {
        return wxString::Format("%c#%d", note, o).ToStdString();
    }
    return wxString::Format("%c%d", note, o).ToStdString();
}

bool IsFileInShowDir(const wxString& showDir, const std::string filename)
{
    wxString fixedFile = FixFile(showDir, filename, true);

    return fixedFile.StartsWith(showDir);
}

wxString FixFile(const wxString& ShowDir, const wxString& file, bool recurse)
{
    static log4cpp::Category &logger_base = log4cpp::Category::getInstance(std::string("log_base"));

    // This is cheating ... saves me from having every call know the showdir as long as an early one passes it in
	static wxString RememberShowDir;

    wxString sd;
	if (ShowDir == "")
	{
		sd = RememberShowDir;
	}
	else
	{
		if (!recurse)
		{
			if (ShowDir != RememberShowDir)
			{
				RememberShowDir = ShowDir;
			}
		}
		sd = ShowDir;
	}

    if (file == "")
    {
        return file;
    }

	if (wxFileExists(file))
	{
		return file;
	}

#ifndef __WXMSW__
    wxFileName fnUnix(file, wxPATH_UNIX);
    wxFileName fn3(sd, fnUnix.GetFullName());
    if (fn3.Exists()) {
        return fn3.GetFullPath();
    }
#endif
    wxFileName fnWin(file, wxPATH_WIN);
    wxFileName fn4(sd, fnWin.GetFullName());
    if (fn4.Exists()) {
        return fn4.GetFullPath();
    }


    wxString sdlc = sd;
    sdlc.LowerCase();
    wxString flc = file;
    flc.LowerCase();

    wxString path;
    wxString fname;
    wxString ext;
    wxFileName::SplitPath(sd, &path, &fname, &ext);
    wxArrayString parts = wxSplit(path, '\\', 0);
    if (fname == "")
    {
        // no subdirectory
        return file;
    }

    wxString showfolder = fname;
    wxString sflc = showfolder;
    sflc.LowerCase();
    wxString newLoc = sd;

    bool appending = false;
    for (size_t x = 0; x < fnWin.GetDirs().size(); x++) {
        if (fnWin.GetDirs()[x].Lower() == sflc) {
            appending = true;
        } else if (appending) {
            newLoc += wxFileName::GetPathSeparator();
            newLoc += fnWin.GetDirs()[x];
        }
    }
    if (appending) {
        newLoc += wxFileName::GetPathSeparator();
        newLoc += fnWin.GetFullName();
        if (wxFileExists(newLoc)) {
            return newLoc;
        }
    }
#ifndef __WXMSW__
    newLoc = sd;
    appending = false;
    for (int x = 0; x < fnUnix.GetDirs().size(); x++) {
        if (fnUnix.GetDirs()[x].Lower() == sflc) {
            appending = true;
        } else if (appending) {
            newLoc += wxFileName::GetPathSeparator();
            newLoc += fnWin.GetDirs()[x];
        }
    }
    if (appending) {
        newLoc += wxFileName::GetPathSeparator();
        newLoc += fnUnix.GetFullName();
        if (wxFileExists(newLoc)) {
            return newLoc;
        }
    }
#endif


    if (flc.Contains(sflc))
    {
        int offset = flc.Find(sflc) + showfolder.Length();
        wxString relative = file.SubString(offset, file.Length());

        if (fnWin.GetDirs().size() > 0) {

        }
        wxFileName sdFn =  wxFileName::DirName(sd);

        if (wxFileExists(relative))
        {
            logger_base.debug("File location fixed: " + file + " -> " + relative);
            return relative;
        }
    }
#ifndef __WXMSW__
    if (ShowDir == "" && fnUnix.GetDirCount() > 0) {
        return FixFile(sd + "/" + fnUnix.GetDirs().Last(), file, true);
    }
#endif
    if (ShowDir == "" && fnWin.GetDirCount() > 0) {
        return FixFile(sd + "\\" + fnWin.GetDirs().Last(), file, true);
    }
   	return file;
}

wxString FixEffectFileParameter(const wxString& paramname, const wxString& parametervalue, const wxString& ShowDir)
{
	int startparamname = parametervalue.Find(paramname);
	int endparamname = parametervalue.find("=", startparamname) - 1;
	int startvalue = endparamname + 2;
	int endvalue = parametervalue.find(",", startvalue) - 1;
	wxString file = parametervalue.SubString(startvalue, endvalue);
	wxString newfile = FixFile(ShowDir, file);
	wxString rc = parametervalue.Left(startvalue) + newfile + parametervalue.Right(parametervalue.Length() - endvalue - 1);
	return rc;
}

std::string UnXmlSafe(const wxString &res)
{
    if (res.Contains('&')) {
        wxString r2(res);
        for (int i = 0; i< 32; ++i)
        {
            wxString ss = wxString::Format("&#%d;", i);
            r2.Replace(ss, wxString::Format("%c", i));
        }
        r2.Replace("&lt;", "<");
        r2.Replace("&gt;", ">");
        r2.Replace("&apos;", "'");
        r2.Replace("&quot;", "\"");
        r2.Replace("&amp;", "&");
        return r2.ToStdString();
    }
    return res.ToStdString();
}

std::string XmlSafe(const std::string& s)
{
    std::string res = "";
    for (auto c = s.begin(); c != s.end(); ++c)
    {
        if ((int)(*c) < 32)
        {
            //res += wxString::Format("&#x%x;", (int)(*c));
            int cc = (int)*c;
            if (cc == 9 || cc == 10 || cc == 13)
            {
                res += wxString::Format("&#%d;", (int)(*c)).ToStdString();
            }
        }
        else if (*c == '&')
        {
            res += "&amp;";
        }
        else if (*c == '<')
        {
            res += "&lt;";
        }
        else if (*c == '>')
        {
            res += "&gt;";
        }
        else if (*c == '\'')
        {
            res += "&apos;";
        }
        else if (*c == '\"')
        {
            res += "&quot;";
        }
        else
        {
            res += (*c);
        }
    }

    return res;
}

inline bool is_base64(unsigned char c)
{
    return (isalnum(c) || (c == '+') || (c == '/'));
}

//returns number of chars at the end that couldn't be decoded
int base64_decode(const wxString& encoded_string, std::vector<unsigned char> &data)
{
    static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    size_t in_len = encoded_string.size();
    int i = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
    {
        char_array_4[i++] = encoded_string[in_];
        in_++;
        if (i == 4)
        {
            for (i = 0; i <4; i++)
            {
                char_array_4[i] = base64_chars.find(char_array_4[i]);
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
            {
                data.resize(data.size() + 1);
                data[data.size() - 1] = char_array_3[i];
            }
            i = 0;
        }
    }

    if (i && encoded_string[in_] == '=')
    {
        for (int j = i; j <4; j++)
        {
            char_array_4[j] = 0;
        }

        for (int j = 0; j <4; j++)
        {
            char_array_4[j] = base64_chars.find(char_array_4[j]);
        }

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (int j = 0; (j < i - 1); j++)
        {
            data.resize(data.size() + 1);
            data[data.size() - 1] = char_array_3[j];
        }
    }
    return i;
}

bool IsVersionOlder(const std::string &compare, const std::string &version)
{
    wxArrayString compare_parts = wxSplit(compare, '.');
    wxArrayString version_parts = wxSplit(version, '.');
    if (wxAtoi(version_parts[0]) < wxAtoi(compare_parts[0])) return true;
    if (wxAtoi(version_parts[0]) > wxAtoi(compare_parts[0])) return false;
    if (wxAtoi(version_parts[1]) < wxAtoi(compare_parts[1])) return true;
    if (wxAtoi(version_parts[1]) > wxAtoi(compare_parts[1])) return false;
    // From 2016 versions only have 2 parts
    if (version_parts.Count() == 2 || compare_parts.Count() == 2)
    {
        if (version_parts.Count() > 2)
        {
            return true;
        }
        return false;
    }
    else
    {
        if (wxAtoi(version_parts[2]) < wxAtoi(compare_parts[2])) return true;
    }
    return false;
}

void SaveWindowPosition(const std::string tag, wxWindow* window)
{
    wxConfigBase* config = wxConfigBase::Get();
    if (window != nullptr)
    {
        wxPoint position = window->GetPosition();
        wxSize size = window->GetSize();
        config->Write(tag, wxString::Format("%d|%d|%d|%d", position.x, position.y, size.GetWidth(), size.GetHeight()));
    }
    else
    {
        config->DeleteEntry(tag);
    }
}

void LoadWindowPosition(const std::string tag, wxSize& size, wxPoint& position)
{
    wxConfigBase* config = wxConfigBase::Get();

    wxString pos = config->Read(tag, "");

    position.x = -1;
    position.y = -1;
    size.SetWidth(-1);
    size.SetHeight(-1);

    wxArrayString ss = wxSplit(pos, '|');

    if (ss.size() == 4)
    {
        position.x = wxAtoi(ss[0]);
        position.y = wxAtoi(ss[1]);
        size.SetWidth(wxAtoi(ss[2]));
        size.SetHeight(wxAtoi(ss[3]));
    }
}

// Extract all chars before the first number in the string ... strip it from the input string
std::string BeforeInt(std::string& s)
{
    int i = 0;
    while (i < s.size() && (s[i] > '9' || s[i] < '0')) {
        i++;
    }
    if (i == 0) {
        return "";
    }

    std::string res = s.substr(0, i);
    s = s.substr(i);
    return res;
}

// Extract any leading number ... strip it from the input string
int ExtractInt(std::string& s)
{
    int i = 0;
    while (i < s.size() && s[i] <= '9' && s[i] >= '0') {
        i++;
    }

    if (i == 0) {
        return -1;
    }

    int res = std::stoi(s.substr(0, i));
    s = s.substr(i);
    return res;
}

int NumberAwareStringCompare(const std::string &a, const std::string &b)
{
    std::string aa = a;
    std::string bb = b;

    while (true) {
        std::string abi = BeforeInt(aa);
        std::string bbi = BeforeInt(bb);

        if (abi == bbi) {
            int ia = ExtractInt(aa);
            int ib = ExtractInt(bb);

            if (ia == ib) {
                if (aa == bb) {
                    return 0;
                }
            } else {
                if (ia < ib) {
                    return -1;
                }
                return 1;
            }
        } else {
            if (abi < bbi) {
                return -1;
            }
            return 1;
        }
    }
}

#ifdef __WXOSX__
double xlOSXGetMainScreenContentScaleFactor();
#endif

double GetSystemContentScaleFactor() {
#ifdef __WXOSX__
    return xlOSXGetMainScreenContentScaleFactor();
#else
    return double(wxScreenDC().GetPPI().y) / 96.0;
#endif
}

double ScaleWithSystemDPI(double val) {
#ifdef __WXOSX__
    //OSX handles all the scaling itself
    return val;
#else
    return ScaleWithSystemDPI(GetSystemContentScaleFactor(), val);
#endif
}
double UnScaleWithSystemDPI(double val) {
#ifdef __WXOSX__
    //OSX handles all the scaling itself
    return val;
#else
    return UnScaleWithSystemDPI(GetSystemContentScaleFactor(), val);
#endif
}

double ScaleWithSystemDPI(double scalingFactor, double val) {
#ifdef __WXOSX__
    //OSX handles all the scaling itself
    return val;
#else
    return val * scalingFactor;
#endif
}

double UnScaleWithSystemDPI(double scalingFactor, double val) {
#ifdef __WXOSX__
    //OSX handles all the scaling itself
    return val;
#else
    return val / scalingFactor;
#endif
}

bool DeleteDirectory(std::string directory)
{
    static log4cpp::Category &logger_base = log4cpp::Category::getInstance(std::string("log_base"));

    if (wxDirExists(directory))
    {
        wxArrayString files;
        wxDir::GetAllFiles(directory, &files, wxEmptyString, wxDIR_FILES);
        for (auto it = files.begin(); it != files.end(); ++it)
        {
            if (!wxRemoveFile(*it))
            {
                logger_base.debug("    Could not delete file %s.", (const char *)it->c_str());
                return false;
            }
        }

        files.clear();
        wxDir::GetAllFiles(directory, &files, wxEmptyString, wxDIR_FILES | wxDIR_DIRS);
        for (auto it = files.begin(); it != files.end(); ++it)
        {
            DeleteDirectory(*it);
        }

        if (!wxRmdir(directory))
        {
            logger_base.debug("    Could not delete folder %s.", (const char *)directory.c_str());
        }
    }
    else
    {
        return false;
    }

    return true;
}

std::string Ordinal(int i)
{
    wxString ii = wxString::Format("%d", i);

    if (ii.EndsWith("11") || ii.EndsWith("12") || ii.EndsWith("12"))
    {
        return (ii + "th").ToStdString();
    }
    else if (ii.EndsWith("1"))
    {
        return (ii + "st").ToStdString();
    }
    else if (ii.EndsWith("2"))
    {
        return (ii + "nd").ToStdString();
    }
    else if (ii.EndsWith("3"))
    {
        return (ii + "rd").ToStdString();
    }
    else
    {
        return (ii + "th").ToStdString();
    }
}

bool IsIPValid(const std::string &ip)
{
    wxString ips = wxString(ip).Trim(false).Trim(true);
    if (ips == "")
    {
        return false;
    }
    else
    {
        static wxRegEx regxIPAddr("^(([0-9]{1}|[0-9]{2}|[0-1][0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]{1}|[0-9]{2}|[0-1][0-9]{2}|2[0-4][0-9]|25[0-5])$");

        if (regxIPAddr.Matches(ips))
        {
            return true;
        }
    }

    return false;
}

bool IsIPValidOrHostname(const std::string &ip, bool iponly)
{
    if (IsIPValid(ip)) {
        return true;
    }

    bool hasChar = false;
    bool hasDot = false;
    //hostnames need at least one char in it if fully qualified
    //if not fully qualified (no .), then the hostname only COULD be just numeric
    for (int y = 0; y < ip.length(); y++) {
        char x = ip[y];
        if ((x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z') || x == '-') {
            hasChar = true;
        }
        if (x == '.') {
            hasDot = true;
        }
    }
    if (hasChar || (!hasDot && !hasChar)) {
        if (iponly) return true;
        wxIPV4address addr;
        addr.Hostname(ip);
        wxString ipAddr = addr.IPAddress();
        if (ipAddr != "0.0.0.0") {
            return true;
        }
    }
    return false;
}

int GetxFadePort(int xfp)
{
    if (xfp == 0) return 0;
    return xfp + 49912;
}

wxString xLightsRequest(int xFadePort, wxString message, wxString ipAddress)
{
    static log4cpp::Category &logger_base = log4cpp::Category::getInstance(std::string("log_base"));

    wxSocketClient socket;
    wxIPV4address addr;
    addr.Hostname(ipAddress);
    addr.Service(GetxFadePort(xFadePort));

    if (socket.Connect(addr))
    {
        logger_base.debug("Sending xLights message %s", (const char *)message.c_str());
        socket.WriteMsg(message.c_str(), message.size() + 1);
        wxByte buffer[1534];
        memset(buffer, 0x00, sizeof(buffer));
        int read = 0;
        while (read == 0)
        {
            socket.ReadMsg(buffer, sizeof(buffer) - 1);
            read = socket.LastReadCount();
            if (read == 0) wxMilliSleep(2);
        }
        wxString msg((char *)buffer);
        logger_base.debug("xLights sent response %s", (const char *)msg.c_str());
        return msg;
    }
    else
    {
        logger_base.warn("Unable to connect to xLights on port %d", GetxFadePort(xFadePort));
        return "ERROR_UNABLE_TO_CONNECT";
    }
}
