#include <cstdlib>
#include <filesystem>
namespace fs = std::filesystem;

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "crow.h"
#include "CRSDK/CameraRemote_SDK.h"
#include "CameraDevice.h"
#include "Text.h"
#include "sio_client.h"

namespace SDK = SCRSDK;

const std::string FOLDER_PATH_LOCAL = "C:\\Users\\Jiaro\\Desktop\\PinOn\\CrSDK\\pictures\\";
const std::string FOLDER_PATH_REMOTE = "http://192.168.202.1:3000/";
const std::string IMAGE_EXTENSION = ".JPG";

std::string intToFiveDigitString(int value) {
    std::ostringstream ss;
    ss << std::setw(5) << std::setfill('0') << value;
    return ss.str();
}

std::string getCurrentDateTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = *std::localtime(&now_c);

    std::ostringstream oss;
    oss << std::put_time(&now_tm, "%Y%m%d%H%M%S");
    return oss.str();
}

cli::text convertStringToText(std::string input) {
    std::wstring w_string(input.begin(), input.end());
    return w_string.c_str();
}

std::shared_ptr<cli::CameraDevice> InitializeCamera() {
    auto init_success = SDK::Init();
    if (!init_success) {
        cli::tout << "Failed to initialize Remote SDK. Terminating.\n";
        SDK::Release();
        std::exit(EXIT_FAILURE);
    }
    cli::tout << "Remote SDK successfully initialized.\n\n";

    SDK::ICrEnumCameraObjectInfo* camera_list = nullptr;
    auto enum_status = SDK::EnumCameraObjects(&camera_list);
    if (CR_FAILED(enum_status) || camera_list == nullptr) {
        cli::tout << "No cameras detected. Connect a camera and retry.\n";
        SDK::Release();
        std::exit(EXIT_FAILURE);
    }

    typedef std::shared_ptr<cli::CameraDevice> CameraDevicePtr;
    std::int32_t cameraNumUniq = 1;

    CrInt32u no = 1;
    auto* camera_info = camera_list->GetCameraObjectInfo(no - 1);

    CameraDevicePtr camera = CameraDevicePtr(new cli::CameraDevice(cameraNumUniq, camera_info));

    camera_list->Release();

    camera->connect(SDK::CrSdkControlMode_Remote, SDK::CrReconnecting_ON);
    Sleep(2000);

    return camera;
}

int main()
{

    auto camera = InitializeCamera();
    if (!camera) {
        cli::tout << "Initialize camera failed!\n";
        std::exit(EXIT_FAILURE);
    }

    crow::SimpleApp app; //define your crow application

    //define your endpoint at the root directory
    CROW_ROUTE(app, "/take_photo/<int>").methods("GET"_method)([&camera](int seq_number) {
        crow::json::wvalue result;
        try {
            std::string seq_str = intToFiveDigitString(seq_number);
            std::string now_time = getCurrentDateTime();
            //std::string image_path = FOLDER_PATH_LOCAL + now_time + seq_str + IMAGE_EXTENSION;
            std::string image_path = FOLDER_PATH_REMOTE + now_time + seq_str + IMAGE_EXTENSION;

            cli::text folder_path = convertStringToText(FOLDER_PATH);
            cli::text time_prefix = convertStringToText(now_time);
            camera->set_save_info(folder_path, time_prefix, seq_number);

            camera->capture_image();

            result["status"] = "success";
            result["message"] = "Photo taken successfully";
            result["image_path"] = image_path;

        }
        catch (const std::exception& e) {
            result["status"] = "error";
            result["message"] = e.what();
        }

        return crow::response{ result };
        });

    //set the port, set the app to run on multiple threads, and run the app
    app.port(18080).multithreaded().run();

    cli::tout << "Release SDK resources.\n";
    SDK::Release();

    cli::tout << "Exiting application.\n";
    std::exit(EXIT_SUCCESS);
}
