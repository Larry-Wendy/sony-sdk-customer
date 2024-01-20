#ifndef CAMERADEVICE_H
#define CAMERADEVICE_H

#if defined(__linux__)
// Use errno_t if available, define it otherwise
#ifndef __STDC_LIB_EXT1__
typedef int errno_t;
#else
#define  __STDC_WANT_LIB_EXT1__ 1
#endif

// End Linux definitions
#endif

#include <atomic>
#include <cstdint>
#include "CRSDK/CameraRemote_SDK.h"
#include "CRSDK/IDeviceCallback.h"
#include "ConnectionInfo.h"
#include "PropertyValueTable.h"
#include "Text.h"
#include "MessageDefine.h"

namespace cli
{

class CRFolderInfos
{
public:
    CRFolderInfos(SCRSDK::CrMtpFolderInfo* info, int32_t nums)
        : pFolder(info)
        , numOfContents(nums)
    {};
    ~CRFolderInfos()
    {
        delete[] pFolder->folderName;
        pFolder->folderName = NULL;
        delete pFolder;
        pFolder = NULL;
    };
public:
    SCRSDK::CrMtpFolderInfo* pFolder;
    int32_t numOfContents;
};

typedef std::vector<CRFolderInfos*> MtpFolderList;
typedef std::vector<SCRSDK::CrMtpContentsInfo*> MtpContentsList;
typedef std::vector<SCRSDK::CrMediaProfileInfo*> MediaProfileList;

class CameraDevice : public SCRSDK::IDeviceCallback
{
public:
    CameraDevice() = delete;
    CameraDevice(std::int32_t no, SCRSDK::ICrCameraObjectInfo const* camera_info);
    ~CameraDevice();

    // Get fingerprint
    bool getfingerprint();

    // Try to connect to the device
    bool connect(SCRSDK::CrSdkControlMode openMode, SCRSDK::CrReconnectingSet reconnect);

    // Disconnect from the device
    bool disconnect();

    // Release from the device
    bool release();

    /*** Shooting operations ***/

    void capture_image() const;
    bool set_save_info() const;
    bool set_save_info(text path, text prefix, int startNumber) const;

    text get_id();

    CrInt32u get_sshsupport();
    bool is_getfingerprint() { return !m_fingerprint.empty(); };
    bool is_setpassword() { return !m_userPassword.empty(); };


public:
    // Inherited via IDeviceCallback
    virtual void OnConnected(SCRSDK::DeviceConnectionVersioin version) override;
    virtual void OnDisconnected(CrInt32u error) override;
    virtual void OnPropertyChanged() override;
    virtual void OnLvPropertyChanged() override;
    virtual void OnCompleteDownload(CrChar* filename, CrInt32u type) override;
    virtual void OnWarning(CrInt32u warning) override;
    virtual void OnWarningExt(CrInt32u warning, CrInt32 param1, CrInt32 param2, CrInt32 param3) override;
    virtual void OnError(CrInt32u error) override;
    virtual void OnPropertyChangedCodes(CrInt32u num, CrInt32u* codes) override;
    virtual void OnLvPropertyChangedCodes(CrInt32u num, CrInt32u* codes) override;
    virtual void OnNotifyContentsTransfer(CrInt32u notify, SCRSDK::CrContentHandle contentHandle, CrChar* filename) override;

private:
    std::int32_t m_number;
    SCRSDK::ICrCameraObjectInfo* m_info;
    std::int64_t m_device_handle;
    std::atomic<bool> m_connected;
    ConnectionType m_conn_type;
    NetworkInfo m_net_info;
    UsbInfo m_usb_info;
    PropertyValueTable m_prop;
    bool m_lvEnbSet;
    SCRSDK::CrSdkControlMode m_modeSDK;
    MtpFolderList   m_foldList;
    MtpContentsList m_contentList;
    bool m_spontaneous_disconnection;
    // DispStrList
    // std::vector<SCRSDK::CrDisplayStringType> m_dispStrTypeList; // Information returned as a result of GetDisplayStringTypes
    MediaProfileList m_mediaprofileList;
    std::string m_fingerprint;
    std::string m_userPassword;
};
} // namespace cli


inline errno_t MemCpyEx(void* result, const void* source, size_t type_size)
{
#if (defined(_WIN32) || defined(_WIN64))
    return memcpy_s(result, type_size, source, type_size);
#else
    std::memcpy(result, source, type_size);
    return 0;
#endif
}

#define MAC_MAX_PATH 255

#endif // !CAMERADEVICE_H
