#include "Output.h"

#include <wx/xml/xml.h>
#include <log4cpp/Category.hh>

#include "E131Output.h"
#include "ArtNetOutput.h"
#include "DDPOutput.h"
#include "NullOutput.h"
#include "LOROutput.h"
#include "LOROptimisedOutput.h"
#include "DLightOutput.h"
#include "DMXOutput.h"
#include "PixelNetOutput.h"
#include "RenardOutput.h"
#include "OpenPixelNetOutput.h"
#include "OpenDMXOutput.h"
#include "../UtilFunctions.h"
#include "OutputManager.h"

#pragma region Constructors and Destructors
Output::Output(Output* output)
{
    _suspend = false;
    _changed = false;
    _timer_msec = 0;
    _outputNumber = -1;
    _nullNumber = -1;
    _startChannel = -1;
    _ok = true;
    _baudRate = 0;
    _universe = 0;
    _lastOutputTime = 0;
    _skippedFrames = 9999;

    _suppressDuplicateFrames = output->IsSuppressDuplicateFrames();
    _dirty = output->IsDirty();
    _enabled = output->IsEnabled();
    _description = output->GetDescription();
    _channels = output->GetChannels();
    _controller = output->GetController();
}

Output::Output(wxXmlNode* node)
{
    _suspend = false;
    _changed = false;
    _timer_msec = 0;
    _outputNumber = -1;
    _nullNumber = -1;
    _startChannel = -1;
    _ip = "";
    _universe = 0;
    _baudRate = 0;
    _commPort = "";
    _controller = nullptr;
    _dirty = false;
    _ok = true;
    _lastOutputTime = 0 ;
    _skippedFrames = 9999;

    _enabled = (node->GetAttribute("Enabled", "Yes") == "Yes");
    _suppressDuplicateFrames = (node->GetAttribute("SuppressDuplicates", "No") == "Yes");
    _description = UnXmlSafe(node->GetAttribute("Description"));
    _channels = wxAtoi(node->GetAttribute("MaxChannels"));
    std::string controller = UnXmlSafe(node->GetAttribute("Controller").ToStdString());
    if (controller != "")
    {
        _controller = Controller::GetController(controller);
    }
}

Output::Output()
{
    _suspend = false;
    _changed = false;
    _timer_msec = 0;
    _outputNumber = -1;
    _nullNumber = -1;
    _startChannel = -1;
    _universe = 0;
    _baudRate = 0;
    _commPort = "";
    _controller = nullptr;
    _enabled = true;
    _description = "";
    _dirty = true;
    _channels = 0;
    _ip = "";
    _ok = true;
    _suppressDuplicateFrames = false;
    _lastOutputTime = 0;
    _skippedFrames = 9999;
}
#pragma endregion Constructors and Destructors

void Output::Save(wxXmlNode* node)
{
    node->AddAttribute("NetworkType", wxString(GetType().c_str()));
    if (!_enabled)
    {
        node->AddAttribute("Enabled", "No");
    }

    if (_suppressDuplicateFrames)
    {
        node->AddAttribute("SuppressDuplicates", "Yes");
    }

    if (_description != "")
    {
        node->AddAttribute("Description", XmlSafe(_description));
    }

    if (_controller != nullptr)
    {
        node->AddAttribute("Controller", XmlSafe(_controller->GetId()));
    }

    node->AddAttribute("MaxChannels", wxString::Format("%ld", _channels));

    _dirty = false;
}

wxXmlNode* Output::Save()
{
    wxXmlNode* node = new wxXmlNode(wxXML_ELEMENT_NODE, "network");
    Save(node);

    return node;
}

#pragma region Static Functions
Output* Output::Create(wxXmlNode* node)
{
    static log4cpp::Category &logger_base = log4cpp::Category::getInstance(std::string("log_base"));
    std::string type = node->GetAttribute("NetworkType", "").ToStdString();

    if (type == OUTPUT_E131)
    {
        return new E131Output(node);
    }
    else if (type == OUTPUT_NULL)
    {
        return new NullOutput(node);
    }
    else if (type == OUTPUT_ARTNET)
    {
        return new ArtNetOutput(node);
    }
    else if (type == OUTPUT_DDP)
    {
        return new DDPOutput(node);
    }
    else if (type == OUTPUT_DMX)
    {
        return new DMXOutput(node);
    }
    else if (type == OUTPUT_PIXELNET)
    {
        return new PixelNetOutput(node);
    }
    else if (type == OUTPUT_LOR)
    {
        return new LOROutput(node);
    }
    else if (type == OUTPUT_LOR_OPT)
    {
        return new LOROptimisedOutput(node);
    }
    else if (type == OUTPUT_DLIGHT)
    {
        return new DLightOutput(node);
    }
    else if (type == OUTPUT_RENARD)
    {
        return new RenardOutput(node);
    }
    else if (type == OUTPUT_OPENDMX)
    {
        return new OpenDMXOutput(node);
    }
    else if (type == OUTPUT_OPENPIXELNET)
    {
        return new OpenPixelNetOutput(node);
    }

    logger_base.warn("Unknown network type %s ignored.", (const char *)type.c_str());
    return nullptr;
}
#pragma endregion Static Functions

#pragma region Getters and Setters
void Output::SetController(const std::string& id)
{
    if (id == "")
    {
        _controller = nullptr;
    }
    else
    {
        _controller = Controller::GetController(id);
    }

    _dirty = true;
}

void Output::SetTransientData(int on, long startChannel, int nullnumber)
{
    _outputNumber = on;
    _startChannel = startChannel;
    if (nullnumber > 0) _nullNumber = nullnumber;
}

void Output::SetIP(const std::string& ip)
{
    _ip = IPOutput::CleanupIP(ip);
    _dirty = true;
}

int Output::GetBaudRate() const
{
    if (_baudRate == 0)
        return 115200;
    else
        return _baudRate;
}
#pragma endregion Getters and Setters

#pragma region Operators
bool Output::operator==(const Output& output) const
{
    if (GetType() != output.GetType()) return false;

    if (IsIpOutput())
    {
        return _universe == output.GetUniverse() && _ip == output.GetIP();
    }
    else
    {
        return _commPort == output.GetCommPort();
    }
}
#pragma endregion Operators

#pragma region Start and Stop
bool Output::Open()
{
    _changed = false;
    _skippedFrames = 9999;
    _lastOutputTime = 0;

    return true;
}
#pragma endregion Start and Stop

#pragma region Data Setting
// channel here is 0 based
void Output::SetManyChannels(long channel, unsigned char data[], long size)
{
#ifdef _MSC_VER
    long chs = min(size, _channels - channel);
#else
    long chs = std::min(size, GetMaxChannels() - channel);
#endif

    for (long i = 0; i < chs; i++)
    {
        SetOneChannel(channel + i, data[i]);
    }
}
#pragma endregion Data Setting

void Output::FrameOutput()
{
    _lastOutputTime = wxGetUTCTimeMillis();
    _skippedFrames = 0;
    _changed = false;
    OutputManager::RegisterSentPacket();
}
