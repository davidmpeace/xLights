#ifndef LOROUTPUT_H
#define LOROUTPUT_H

#include "SerialOutput.h"

// Should be called with: 0 <= chindex <= 3839 (max channels=240*16)


class LOROutput : public SerialOutput
{
protected:
    #pragma region LOR Constants
    static const unsigned int LOR_PACKET_LEN = 256;
    static const unsigned int LOR_MAX_CHANNELS = 8192;
    #pragma endregion LOR Constants

    #pragma region Member Variables
    long _lastheartbeat;
    wxByte _data[LOR_PACKET_LEN];
    wxByte _lastSent[LOR_MAX_CHANNELS];
    #pragma endregion Member Variables

public:

    #pragma region Constructors and Destructors
    LOROutput(wxXmlNode* node);
    LOROutput(SerialOutput* output);
    LOROutput();
    virtual ~LOROutput() override {};
    #pragma endregion Constructors and Destructors

    #pragma region Getters and Setters
    virtual std::string GetType() const override { return OUTPUT_LOR; }
    virtual int GetMaxChannels() const override { return LOR_MAX_CHANNELS; }
    virtual bool IsValidChannelCount(long channelCount) const override { return channelCount > 0 && channelCount <= LOR_MAX_CHANNELS; }
    virtual std::string GetSetupHelp() const override;
    #pragma endregion Getters and Setters

    #pragma region Start and Stop
    virtual bool Open() override;
    #pragma endregion Start and Stop

    #pragma region Frame Handling
    virtual void EndFrame(int suppressFrames) override;
    virtual void ResetFrame() override;
    #pragma endregion Frame Handling

    #pragma region Data Setting
    virtual void SetOneChannel(long channel, unsigned char data) override;
    virtual void AllOff() override;
    #pragma endregion Data Setting

    virtual void SendHeartbeat() const override;
};

 #endif
