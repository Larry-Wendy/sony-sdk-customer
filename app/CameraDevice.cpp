#if defined (_WIN32) || defined(_WIN64)

#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <memory>

// TODO: reference additional headers your program requires here
#pragma warning (push)
#pragma warning (disable:4819)	// warning C4819: The file contains a character that cannot be represented in the current code page (932). Save the file in Unicode format to prevent data loss
#pragma warning (pop)
#endif

#include "CameraDevice.h"
#include <chrono>
#if defined(__GNUC__) && __GNUC__ < 8
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
#include <fstream>
#include <thread>
#include "CRSDK/CrDeviceProperty.h"
#include "Text.h"


#if defined(__APPLE__) || defined(__linux__)
#include <sys/stat.h>
#include <vector>
#include <dirent.h>
#include <iomanip>
#endif


#if defined(__APPLE__) || defined(__linux__)
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#else
#include <conio.h>
#endif


// Enumerator
enum Password_Key {

#if defined(__APPLE__) || defined(__linux__)
    Password_Key_Back = 127,
    Password_Key_Enter = 10
#else
    Password_Key_Back = 8,
    Password_Key_Enter = 13
#endif

};

#if defined(__APPLE__) || defined(__linux__)
/* reads from keypress, doesn't echo */
int getch_for_Nix(void)
{
    struct termios oldattr, newattr;
    int iptCh;
    tcgetattr(STDIN_FILENO, &oldattr);
    newattr = oldattr;
    newattr.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newattr);
    iptCh = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);
    return iptCh;
}
#endif


namespace SDK = SCRSDK;
using namespace std::chrono_literals;

constexpr int const ImageSaveAutoStartNo = -1;

namespace cli
{
CameraDevice::CameraDevice(std::int32_t no, SCRSDK::ICrCameraObjectInfo const* camera_info)
    : m_number(no)
    , m_device_handle(0)
    , m_connected(false)
    , m_conn_type(ConnectionType::UNKNOWN)
    , m_net_info()
    , m_usb_info()
    , m_prop()
    , m_lvEnbSet(true)
    , m_modeSDK(SCRSDK::CrSdkControlMode_Remote)
    , m_spontaneous_disconnection(false)
    , m_fingerprint("")
    , m_userPassword("")
{
    m_info = SDK::CreateCameraObjectInfo(
        camera_info->GetName(),
        camera_info->GetModel(),
        camera_info->GetUsbPid(),
        camera_info->GetIdType(),
        camera_info->GetIdSize(),
        camera_info->GetId(),
        camera_info->GetConnectionTypeName(),
        camera_info->GetAdaptorName(),
        camera_info->GetPairingNecessity(),
        camera_info->GetSSHsupport()
    );

    m_conn_type = parse_connection_type(m_info->GetConnectionTypeName());
    switch (m_conn_type)
    {
    case ConnectionType::NETWORK:
        m_net_info = parse_ip_info(m_info->GetId(), m_info->GetIdSize());
        break;
    case ConnectionType::USB:
        m_usb_info.pid = m_info->GetUsbPid();
        break;
    case ConnectionType::UNKNOWN:
        [[fallthrough]];
    default:
        // Do nothing
        break;
    }
}

CameraDevice::~CameraDevice()
{
    if (m_info) m_info->Release();
}

bool CameraDevice::getfingerprint()
{
    CrInt32u fpLen = 0;
    char fpBuff[128] = { 0 };
    SDK::CrError err = SDK::GetFingerprint(m_info, fpBuff, &fpLen);

    if (CR_SUCCEEDED(err))
    {
        m_fingerprint = std::string(fpBuff, fpLen);
        return true;
    }
    return false;
}

bool CameraDevice::connect(SCRSDK::CrSdkControlMode openMode, SCRSDK::CrReconnectingSet reconnect)
{
    const char* inputId = "admin";
    char inputPassword[32] = { 0 };
    if (SDK::CrSSHsupportValue::CrSSHsupport_ON == get_sshsupport())
    {
        if (!is_getfingerprint())
        {
            bool resFp = getfingerprint();
            if (resFp)
            {
                tout << "fingerprint: \n" << m_fingerprint.c_str() << std::endl;
                tout << std::endl << "Are you sure you want to continue connecting ? (y/n) > ";
                text yesno;
                std::getline(cli::tin, yesno);
                if (yesno != TEXT("y"))
                {
                    m_fingerprint.clear();
                    return false;
                }
            }
        }
        if (!is_setpassword())
        {
            cli::tout << "Please SSH password > ";
            cli::text userPw;
 
            // Stores the password
            char maskPw = '*';
            char ch_ipt = {};

            // Until condition is true
            while (true) {

#if defined (_WIN32) || defined(_WIN64)
                ch_ipt = _getch();
#else
                ch_ipt = getch_for_Nix();
#endif

                // if the ch_ipt
                if (ch_ipt == Password_Key_Enter) {
                    tout << std::endl;
                    break;
                }
                else if (ch_ipt == Password_Key_Back
                    && userPw.length() != 0) {
                    userPw.pop_back();

                    // Cout statement is very
                    // important as it will erase
                    // previously printed character
                    tout << "\b \b";

                    continue;
                }

                // Without using this, program
                // will crash as \b can't be
                // print in beginning of line
                else if (ch_ipt == Password_Key_Back
                    && userPw.length() == 0) {
                    continue;
                }

                userPw.push_back(ch_ipt);
                tout << maskPw;
            }

#if defined(_WIN32) || (_WIN64)
            mbstate_t mbstate;
            size_t retPw;
            memset(&mbstate, 0, sizeof(mbstate_t));
            const wchar_t* wcsInStr = userPw.c_str();
            errno_t erno = wcsrtombs_s(&retPw, inputPassword, &wcsInStr, 32, &mbstate);
#else
            strncpy(inputPassword, (const char*)userPw.c_str(), userPw.size());
#endif
            m_userPassword = std::string(inputPassword, userPw.size());

        }
    }

    m_spontaneous_disconnection = false;
    auto connect_status = SDK::Connect(m_info, this, &m_device_handle, openMode, reconnect, inputId, m_userPassword.c_str(), m_fingerprint.c_str(), (CrInt32u)m_fingerprint.size());
    if (CR_FAILED(connect_status)) {
        text id(this->get_id());
        tout << std::endl << "Failed to connect: 0x" << std::hex << connect_status << std::dec << ". " << m_info->GetModel() << " (" << id.data() << ")\n";
        m_userPassword.clear();
        return false;
    }
    set_save_info();
    return true;
}

bool CameraDevice::disconnect()
{
    // m_fingerprint.clear();  // Use as needed
    // m_userPassword.clear(); // Use as needed
    m_spontaneous_disconnection = true;
    tout << "Disconnect from camera...\n";
    auto disconnect_status = SDK::Disconnect(m_device_handle);
    if (CR_FAILED(disconnect_status)) {
        tout << "Disconnect failed to initialize.\n";
        return false;
    }
    return true;
}

bool CameraDevice::release()
{
    tout << "Release camera...\n";
    auto finalize_status = SDK::ReleaseDevice(m_device_handle);
    m_device_handle = 0; // clear
    if (CR_FAILED(finalize_status)) {
        tout << "Finalize device failed to initialize.\n";
        return false;
    }
    return true;
}

CrInt32u CameraDevice::get_sshsupport()
{
    return m_info->GetSSHsupport();
}

void CameraDevice::capture_image() const
{
    tout << "Capture image...\n";
    tout << "Shutter down\n";
    SDK::SendCommand(m_device_handle, SDK::CrCommandId::CrCommandId_Release, SDK::CrCommandParam_Down);

    // Wait, then send shutter up
    std::this_thread::sleep_for(35ms);
    tout << "Shutter up\n";
    SDK::SendCommand(m_device_handle, SDK::CrCommandId::CrCommandId_Release, SDK::CrCommandParam_Up);
}

bool CameraDevice::set_save_info() const
{
#if defined(__APPLE__)
    text_char path[MAC_MAX_PATH]; /*MAX_PATH*/
    memset(path, 0, sizeof(path));
    if (NULL == getcwd(path, sizeof(path) - 1)) {
        tout << "Folder path is too long.\n";
        return false;
    }
    auto save_status = SDK::SetSaveInfo(m_device_handle
        , path, (char*)"", ImageSaveAutoStartNo);
#else
    text path = fs::current_path().native();
    tout << path.data() << '\n';

    auto save_status = SDK::SetSaveInfo(m_device_handle
        , const_cast<text_char*>(path.data()), TEXT(""), ImageSaveAutoStartNo);

#endif
    if (CR_FAILED(save_status)) {
        tout << "Failed to set save path.\n";
        return false;
    }
    return true;
}

bool CameraDevice::set_save_info(text path, text prefix, int startNumber) const
{
    //text path = fs::current_path().native();
    auto save_status = SDK::SetSaveInfo(m_device_handle, 
        const_cast<text_char*>(path.data()),
        const_cast<text_char*>(prefix.data()),
        startNumber);
    if (CR_FAILED(save_status)) {
        tout << "Failed to set save path.\n";
        return false;
    }
    return true;
}

text CameraDevice::get_id()
{
    if (ConnectionType::NETWORK == m_conn_type) {
        return m_net_info.mac_address;
    }
    else
        return text((TCHAR*)m_info->GetId());
}

void CameraDevice::OnConnected(SDK::DeviceConnectionVersioin version)
{
    m_connected.store(true);
    text id(this->get_id());
    tout << "Connected to " << m_info->GetModel() << " (" << id.data() << ")\n";
}

void CameraDevice::OnDisconnected(CrInt32u error)
{
    m_connected.store(false);
    text id(this->get_id());
    tout << "Disconnected from " << m_info->GetModel() << " (" << id.data() << ")\n";
    if ((false == m_spontaneous_disconnection) && (SDK::CrSdkControlMode_ContentsTransfer == m_modeSDK))
    {
        tout << "Please input '0' to return to the TOP-MENU\n";
    }
}

void CameraDevice::OnPropertyChanged()
{
    // tout << "Property changed.\n";
}

void CameraDevice::OnLvPropertyChanged()
{
    // tout << "LvProperty changed.\n";
}

void CameraDevice::OnCompleteDownload(CrChar* filename, CrInt32u type )
{
    text file(filename);
    switch (type)
    {
    case SCRSDK::CrDownloadSettingFileType_None:
        tout << "Complete download. File: " << file.data() << '\n';
        break;
    case SCRSDK::CrDownloadSettingFileType_Setup:
        tout << "Complete download. Camera Setting File: " << file.data() << '\n';
        break;
    default:
        break;
    }
}

void CameraDevice::OnNotifyContentsTransfer(CrInt32u notify, SDK::CrContentHandle contentHandle, CrChar* filename)
{
    // Start
    if (SDK::CrNotify_ContentsTransfer_Start == notify)
    {
        tout << "[START] Contents Handle: 0x " << std::hex << contentHandle << std::dec << std::endl;
    }
    // Complete
    else if (SDK::CrNotify_ContentsTransfer_Complete == notify)
    {
        text file(filename);
        tout << "[COMPLETE] Contents Handle: 0x" << std::hex << contentHandle << std::dec << ", File: " << file.data() << std::endl;
    }
    // Other
    else
    {
        text msg = get_message_desc(notify);
        if (msg.empty()) {
            tout << "[-] Content transfer failure. 0x" << std::hex << notify << ", handle: 0x" << contentHandle << std::dec << std::endl;
        } else {
            tout << "[-] Content transfer failure. handle: 0x" << std::hex << contentHandle  << std::dec << std::endl << "    -> ";
            tout << msg.data() << std::endl;
        }
    }
}

void CameraDevice::OnWarning(CrInt32u warning)
{
    text id(this->get_id());
    if (SDK::CrWarning_Connect_Reconnecting == warning) {
        tout << "Device Disconnected. Reconnecting... " << m_info->GetModel() << " (" << id.data() << ")\n";
        return;
    }
    switch (warning)
    {
    case SDK::CrWarning_ContentsTransferMode_Invalid:
    case SDK::CrWarning_ContentsTransferMode_DeviceBusy:
    case SDK::CrWarning_ContentsTransferMode_StatusError:
        tout << "\nThe camera is in a condition where it cannot transfer content.\n\n";
        tout << "Please input '0' to return to the TOP-MENU and connect again.\n";
        break;
    case SDK::CrWarning_ContentsTransferMode_CanceledFromCamera:
        tout << "\nContent transfer mode canceled.\n";
        tout << "If you want to continue content transfer, input '0' to return to the TOP-MENU and connect again.\n\n";
        break;
    case SDK::CrWarning_CameraSettings_Read_Result_OK:
        tout << "\nConfiguration file read successfully.\n\n";
        break;
    case SDK::CrWarning_CameraSettings_Read_Result_NG:
        tout << "\nFailed to load configuration file\n\n";
        break;
    case SDK::CrWarning_CameraSettings_Save_Result_NG:
        tout << "\nConfiguration file save request failed.\n\n";
        break;
    case SDK::CrWarning_RequestDisplayStringList_Success:
        tout << "\nRequest for DisplayStringList  successfully\n\n";        
        break;
    case SDK::CrWarning_RequestDisplayStringList_Error: 
        tout << "\nFailed to Request for DisplayStringList\n\n";
        break;
    case SDK::CrWarning_CustomWBCapture_Result_OK:
        tout << "\nCustom WB capture successful.\n\n";
        break;
    case SDK::CrWarning_CustomWBCapture_Result_Invalid:
    case SDK::CrWarning_CustomWBCapture_Result_NG:
        tout << "\nCustom WB capture failure.\n\n";
        break;
    case SDK::CrWarning_FocusPosition_Result_Invalid:
        tout << "\nFocus Position Result Invalid.\n\n";
        break;
    case SDK::CrWarning_FocusPosition_Result_OK:
        tout << "\nFocus Position Result OK.\n\n";
        break;
    case SDK::CrWarning_FocusPosition_Result_NG:
        tout << "\nFocus Position Result NG.\n\n";
        break;
    case SDK::CrWarning_ControlMonitoring_Result_Start_Failed:
        tout << "\nMonitoring Start Failed.\n\n";
        break;
    case SDK::CrWarning_ControlMonitoring_Result_Stop_Failed:
        tout << "\nMonitoring Stop Failed.\n\n";
        break;
    case SDK::CrWarning_ControlMonitoring_Result_Invalid:
    case SDK::CrWarning_ControlMonitoring_Result_SystemError:
    case SDK::CrWarning_ControlMonitoring_Result_MaximumNumberSimultaneousDeliveries:
    case SDK::CrWarning_ControlMonitoring_Result_ExclusiveError:
    case SDK::CrWarning_ControlMonitoring_Result_AlreadyStartedInDifferentType:
    case SDK::CrWarning_ControlMonitoring_Result_MonitoringStopped:
    case SDK::CrWarning_ControlMonitoring_Result_InvalidParameter:
        tout << "\nMonitoring Result NG.\n\n"; tout << warning;
        break;
    case SDK::CrWarning_ControlMonitoring_LostReceiving:
        tout << "\nMonitoring Lost Receiving.\n\n";
        break;
    default:
        return;
    }
}

void CameraDevice::OnWarningExt(CrInt32u warning, CrInt32 param1, CrInt32 param2, CrInt32 param3)
{
    tout << "<Receive>\n";
#if defined(_WIN64)
    printf_s("warning: 0x%08X\n", warning);
    printf_s(" param1: 0x%08X\n", param1);
    printf_s(" param2: 0x%08X\n", param2);
    printf_s(" param3: 0x%08X\n", param3);
#else // temporary
    printf("warning: 0x%08X\n", warning);
    printf(" param1: 0x%08X\n", param1);
    printf(" param2: 0x%08X\n", param2);
    printf(" param3: 0x%08X\n", param3);
#endif
    tout << "\n<warning>\n";
    tout << " 0x00060001: CrWarningExt_AFStatus\n";
    tout << "             <param1> Focus Indication\n";
    tout << " 0x00060002: CrWarningExt_OperationResults\n";
    tout << "             <param1> enum CrSdkApi\n";
    tout << "                      0x00000002: CrSdkApi_SetDeviceProperty\n";
    tout << "                      0x00000003: CrSdkApi_SendCommand\n";
    tout << "             <param2> CrDevicePropertyCode or CrCommandId\n";
    tout << "             <param3> enum CrWarningExt_OperationResultsParam\n";
    tout << "                      0x00000000: CrWarningExt_OperationResultsParam_Invalid\n";
    tout << "                      0x00000001: CrWarningExt_OperationResultsParam_OK\n";
    tout << "                      0x00000002: CrWarningExt_OperationResultsParam_NG\n";
}

void CameraDevice::OnPropertyChangedCodes(CrInt32u num, CrInt32u* codes)
{
    //tout << "Property changed.  num = " << std::dec << num;
    //tout << std::hex;
    //for (std::int32_t i = 0; i < num; ++i)
    //{
    //    tout << ", 0x" << codes[i];
    //}
    //tout << std::endl << std::dec;
}

void CameraDevice::OnLvPropertyChangedCodes(CrInt32u num, CrInt32u* codes)
{
    //tout << "LvProperty changed.  num = " << std::dec << num;
    //tout << std::hex;
    //for (std::int32_t i = 0; i < num; ++i)
    //{
    //    tout << ", 0x" << codes[i];
    //}
    //tout << std::endl;
#if 0 
    SDK::CrLiveViewProperty* lvProperty = nullptr;
    int32_t nprop = 0;
    SDK::CrError err = SDK::GetSelectLiveViewProperties(m_device_handle, num, codes, &lvProperty, &nprop);
    if (CR_SUCCEEDED(err) && lvProperty) {
        for (int32_t i=0 ; i<nprop ; i++) {
            auto prop = lvProperty[i];
            if (SDK::CrFrameInfoType::CrFrameInfoType_FocusFrameInfo == prop.GetFrameInfoType()) {
                int sizVal = prop.GetValueSize();
                int count = sizVal / sizeof(SDK::CrFocusFrameInfo);
                SDK::CrFocusFrameInfo* pFrameInfo = (SDK::CrFocusFrameInfo*)prop.GetValue();
                if (0 == sizVal || nullptr == pFrameInfo) {
                    tout << "  FocusFrameInfo nothing\n";
                }
                else {
                    for (std::int32_t frame = 0; frame < count; ++frame) {
                        auto lvprop = pFrameInfo[frame];
                        char buff[512];
                        memset(buff, 0, sizeof(buff));
#if defined(_WIN32) || (_WIN64)
                        sprintf_s(buff, "  FocusFrameInfo no[%d] type[%d] state[%d] w[%d] h[%d] Deno[%d-%d] Nume[%d-%d]",
                            frame + 1,
                            lvprop.type,
                            lvprop.state,
                            lvprop.width, lvprop.height,
                            lvprop.xDenominator, lvprop.yDenominator,
                            lvprop.xNumerator, lvprop.yNumerator);
#else
                        snprintf(buff, sizeof(buff), "  FocusFrameInfo no[%d] type[%d] state[%d] w[%d] h[%d] Deno[%d-%d] Nume[%d-%d]",
                            frame + 1,
                            lvprop.type,
                            lvprop.state,
                            lvprop.width, lvprop.height,
                            lvprop.xDenominator, lvprop.yDenominator,
                            lvprop.xNumerator, lvprop.yNumerator);
#endif                           
                        tout << buff << std::endl;
                    }
                }
            }
            else if (SDK::CrFrameInfoType::CrFrameInfoType_FaceFrameInfo == prop.GetFrameInfoType()) {
                int sizVal = prop.GetValueSize();
                int count = sizVal / sizeof(SDK::CrFaceFrameInfo);
                SDK::CrFaceFrameInfo* pFrameInfo = (SDK::CrFaceFrameInfo*)prop.GetValue();
                if (0 == sizVal || nullptr == pFrameInfo) {
                    tout << "  FaceFrameInfo nothing\n";
                }
                else {
                    for (std::int32_t frame = 0; frame < count; ++frame) {
                        auto lvprop = pFrameInfo[frame];
                        char buff[512];
                        memset(buff, 0, sizeof(buff));
#if defined(_WIN32) || (_WIN64)
                        sprintf_s(buff, "  FaceFrameInfo no[%d] type[%d] state[%d] w[%d] h[%d] Deno[%d-%d] Nume[%d-%d]",
                            frame + 1,
                            lvprop.type,
                            lvprop.state,
                            lvprop.width, lvprop.height,
                            lvprop.xDenominator, lvprop.yDenominator,
                            lvprop.xNumerator, lvprop.yNumerator);
#else
                        snprintf(buff, sizeof(buff), "  FaceFrameInfo no[%d] type[%d] state[%d] w[%d] h[%d] Deno[%d-%d] Nume[%d-%d]",
                            frame + 1,
                            lvprop.type,
                            lvprop.state,
                            lvprop.width, lvprop.height,
                            lvprop.xDenominator, lvprop.yDenominator,
                            lvprop.xNumerator, lvprop.yNumerator);
#endif                           
                        tout << buff << std::endl;
                    }
                }
            }
            else if (SDK::CrFrameInfoType::CrFrameInfoType_TrackingFrameInfo == prop.GetFrameInfoType()) {
                int sizVal = prop.GetValueSize();
                int count = sizVal / sizeof(SDK::CrTrackingFrameInfo);
                SDK::CrTrackingFrameInfo* pFrameInfo = (SDK::CrTrackingFrameInfo*)prop.GetValue();
                if (0 == sizVal || nullptr == pFrameInfo) {
                    tout << "  TrackingFrameInfo nothing\n";
                }
                else {
                    for (std::int32_t frame = 0; frame < count; ++frame) {
                        auto lvprop = pFrameInfo[frame];
                        char buff[512];
                        memset(buff, 0, sizeof(buff));
#if defined(_WIN32) || (_WIN64)
                        sprintf_s(buff, "  TrackingFrameInfo no[%d] type[%d] state[%d] w[%d] h[%d] Deno[%d-%d] Nume[%d-%d]",
                            frame + 1,
                            lvprop.type,
                            lvprop.state,
                            lvprop.width, lvprop.height,
                            lvprop.xDenominator, lvprop.yDenominator,
                            lvprop.xNumerator, lvprop.yNumerator);
#else
                        snprintf(buff, sizeof(buff), "  TrackingFrameInfo no[%d] type[%d] state[%d] w[%d] h[%d] Deno[%d-%d] Nume[%d-%d]",
                            frame + 1,
                            lvprop.type,
                            lvprop.state,
                            lvprop.width, lvprop.height,
                            lvprop.xDenominator, lvprop.yDenominator,
                            lvprop.xNumerator, lvprop.yNumerator);
#endif                           
                        tout << buff << std::endl;
                    }
                }
            }
            else if (SDK::CrFrameInfoType::CrFrameInfoType_Magnifier_Position == prop.GetFrameInfoType()) {
                int sizVal = prop.GetValueSize();
                int count = sizVal / sizeof(SDK::CrMagPosInfo);
                SDK::CrMagPosInfo* pMagPosInfo = (SDK::CrMagPosInfo*)prop.GetValue();
                if (0 == sizVal || nullptr == pMagPosInfo) {
                    tout << "  MagPosInfo nothing\n";
                }
                else {
                    char buff[512];
                    memset(buff, 0, sizeof(buff));
#if defined(_WIN32) || (_WIN64)
                    sprintf_s(buff, "  MagPosInfo w[%d] h[%d] Deno[%d-%d] Nume[%d-%d]",
                        pMagPosInfo->width, pMagPosInfo->height,
                        pMagPosInfo->xDenominator, pMagPosInfo->yDenominator,
                        pMagPosInfo->xNumerator, pMagPosInfo->yNumerator);
#else
                    snprintf(buff, sizeof(buff), "  MagPosInfo w[%d] h[%d] Deno[%d-%d] Nume[%d-%d]",
                        pMagPosInfo->width, pMagPosInfo->height,
                        pMagPosInfo->xDenominator, pMagPosInfo->yDenominator,
                        pMagPosInfo->xNumerator, pMagPosInfo->yNumerator);
#endif
                    tout << buff << std::endl;
                }
            }
        }
        SDK::ReleaseLiveViewProperties(m_device_handle, lvProperty);
    }
#endif
    tout << std::dec;
}

void CameraDevice::OnError(CrInt32u error)
{
    text id(this->get_id());
    text msg = get_message_desc(error);
    if (!msg.empty()) {
        // output is 2 line
        tout << std::endl << msg.data() << std::endl;
        tout << m_info->GetModel() << " (" << id.data() << ")" << std::endl;
        if (SDK::CrError_Connect_TimeOut == error) {
            // append 1 line
            tout << "Please input '0' after Connect camera" << std::endl;
            return;
        }
        if (SDK::CrError_Connect_Disconnected == error)
        {
            return;
        }
        if (SDK::CrError_Connect_SSH_ServerConnectFailed == error
            || SDK::CrError_Connect_SSH_InvalidParameter == error
            || SDK::CrError_Connect_SSH_ServerAuthenticationFailed == error
            || SDK::CrError_Connect_SSH_UserAuthenticationFailed == error
            || SDK::CrError_Connect_SSH_PortForwardFailed == error
            || SDK::CrError_Connect_SSH_GetFingerprintFailed == error)
        {
            m_fingerprint.clear();
            m_userPassword.clear();
        }
        tout << "Please input '0' to return to the TOP-MENU\n";
    }
}

}
// namespace cli

