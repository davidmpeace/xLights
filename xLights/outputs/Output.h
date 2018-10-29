#ifndef OUTPUT_H
#define OUTPUT_H

#include "Controller.h"
#include <wx/window.h>
#include <wx/time.h>

class OutputManager;

#pragma region Output Constants
// These are used to identify each output type
#define OUTPUT_E131 "E131"
#define OUTPUT_ARTNET "ArtNet"
#define OUTPUT_DDP "DDP"
#define OUTPUT_DMX "DMX"
#define OUTPUT_NULL "NULL"
#define OUTPUT_PIXELNET "Pixelnet"
#define OUTPUT_OPENPIXELNET "Pixelnet-Open"
#define OUTPUT_LOR "LOR"
#define OUTPUT_LOR_OPT "LOR Optimised"
#define OUTPUT_DLIGHT "D-Light"
#define OUTPUT_RENARD "Renard"
#define OUTPUT_OPENDMX "OpenDMX"
#pragma endregion Output Constants

typedef enum
{
    PING_OK,
    PING_WEBOK,
    PING_OPEN,
    PING_OPENED,
    PING_ALLFAILED,
    PING_UNAVAILABLE,
    PING_UNKNOWN
} PINGSTATE;

class Output
{
protected:

#pragma region Member Variables
    bool _dirty;
    std::string _description;
    std::string _ip;
    std::string _commPort;
    long _channels;
    int _baudRate;
    int _universe;
    bool _enabled;
    bool _suspend;
    Controller* _controller;
    int _outputNumber; // cached ordinal of this output ... may change when reordered or other output are changed
    int _nullNumber; // cached ordinal of null controllers ... may change when reordered or other output are changed
    long _startChannel; // cached start channel of this output ... may change when reordered or other output are changed
    long _timer_msec;
    bool _ok;
    bool _suppressDuplicateFrames;
    wxLongLong _lastOutputTime;
    int _skippedFrames;
    bool _changed; // set to true when something in the packed has changed
    #pragma endregion Member Variables

    virtual void Save(wxXmlNode* node);

public:

    #pragma region Constructors and Destructors
    Output(wxXmlNode* node);
    Output(Output* output);
    Output();
    virtual ~Output() {};
    #pragma endregion Constructors and Destructors

    #pragma region Static Functions
    static Output* Create(wxXmlNode* node);
    static std::list<Output*> Discover() { return std::list<Output*>(); } // Discovers controllers supporting this protocol
    #pragma endregion Static Functions

    #pragma region Getters and Setters
    virtual std::list<Output*> GetOutputs() const { std::list<Output*> res; return res; }
    bool IsDirty() const { return _dirty; }
    void ClearDirty() { _dirty = false; }
    long GetStartChannel() const { return _startChannel; }
    long GetActualEndChannel() const { return _startChannel + _channels - 1; }
    void Suspend(bool suspend) { _suspend = suspend; }
    virtual long GetEndChannel() const { return _startChannel + _channels - 1; }
    std::string GetDescription() const { return _description; }
    void SetDescription(const std::string& description) { _description = description; _dirty = true; }
    void SetSuppressDuplicateFrames(const bool suppressDuplicateFrames) { _suppressDuplicateFrames = suppressDuplicateFrames; _dirty = true; }
    std::string GetIP() const { return _ip; }
    void SetIP(const std::string& ip);
    std::string GetCommPort() const { return _commPort; }
    void SetCommPort(const std::string& commPort) { _commPort = commPort; _dirty = true; }
    long GetChannels() const { return _channels; }
    void SetChannels(long channels) { _channels = channels; _dirty = true; }
    int GetUniverse() const { return _universe; }
    void SetUniverse(int universe) { _universe = universe; _dirty = true; }
    virtual std::string GetUniverseString() const { return wxString::Format(wxT("%i"), GetUniverse()).ToStdString(); }
    virtual std::string GetBaudRateString() const { return wxString::Format(wxT("%i"), GetBaudRate()).ToStdString(); }
    virtual int GetUniverses() const { return 1; }
    int GetBaudRate() const;
    void SetBaudRate(int baudRate) { _baudRate = baudRate; _dirty = true; }
    bool IsEnabled() const { return _enabled; }
    void Enable(bool enable) { _enabled = enable; _dirty = true; }
    void SetController(const std::string& id);
    Controller* GetController() const { return _controller; }
    int GetOutputNumber() const { return _outputNumber; }
    virtual void SetTransientData(int on, long startChannel, int nullnumber);
    long GetTimer() const { return _timer_msec; }
    bool IsOk() const { return _ok; }
    virtual std::string GetType() const = 0;
    virtual std::string GetLongDescription() const = 0;
    virtual std::string GetPingDescription() const = 0;
    virtual bool IsIpOutput() const = 0;
    virtual bool IsSerialOutput() const = 0;
    virtual bool IsOutputable() const { return true; }
    virtual Output* GetActualOutput(long startChannel) { return this; }
    virtual bool IsOutputCollection() const { return false; }
    virtual std::string GetChannelMapping(long ch) const = 0;
    virtual int GetMaxChannels() const = 0;
    virtual bool IsValidChannelCount(long channelCount) const = 0;
    virtual size_t TxNonEmptyCount() const { return 0; }
    virtual bool TxEmpty() const { return true; }
    bool IsSuppressDuplicateFrames() const { return _suppressDuplicateFrames; }
    virtual PINGSTATE Ping() const = 0;
    virtual bool CanPing() const = 0;
    #pragma endregion Getters and Setters

    #pragma region Operators
    bool operator==(const Output& output) const;
    #pragma endregion Operators

    virtual wxXmlNode* Save();

    #pragma region Start and Stop
    virtual bool Open();
    virtual void Close() = 0;
    #pragma endregion Start and Stop

    #pragma region Frame Handling
    virtual void StartFrame(long msec) { _timer_msec = msec; }
    virtual void EndFrame(int suppressFrames) = 0;
    virtual void ResetFrame() {}
    void FrameOutput();
    void SkipFrame() { _skippedFrames++; }
    bool NeedToOutput(int suppressFrames) const { return !IsSuppressDuplicateFrames() || _skippedFrames >= suppressFrames; }
    #pragma endregion Frame Handling

    #pragma region Data Setting
    virtual void SetOneChannel(long channel, unsigned char data) = 0;
    virtual void SetManyChannels(long channel, unsigned char data[], long size);
    virtual void AllOff() = 0;
    #pragma endregion Data Setting

    virtual void SendHeartbeat() const {}

    #pragma region UI
#ifndef EXCLUDENETWORKUI
    // returns nullptr if cancelled
    // retruns a pointer to a new output if mutated ... otherwise it returns this
    virtual Output* Configure(wxWindow* parent, OutputManager* outputManager) = 0;
#endif
    #pragma endregion UI
};

 #endif
