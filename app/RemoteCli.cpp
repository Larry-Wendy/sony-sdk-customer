#include <cstdlib>
#include <filesystem>
namespace fs = std::filesystem;

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <random>
#include <opencv2/opencv.hpp>
#include "crow.h"
#include "crow/middlewares/cors.h"
#include "CRSDK/CameraRemote_SDK.h"
#include "CameraDevice.h"
#include "Text.h"
#include <cpprest/http_client.h>
 
namespace SDK = SCRSDK;

const std::string FOLDER_PATH_LOCAL = "C:\\Users\\kefei\\Documents\\PinOn\\images\\";
//const std::string FOLDER_PATH_REMOTE = "http://192.168.202.1:3000/pictures/";
//const std::string FOLDER_PATH_REMOTE = "https://94c4-2603-7000-9900-307c-78e0-566a-393-4a00.ngrok-free.app/pictures/";
const std::string IMAGE_EXTENSION = ".JPG";
//const int compress_factor = 10;
//const int resize_width = 1620;
//const int resize_height = 1080;
const int port = 18080;
const std::string host = "127.0.0.1";


BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_CLOSE_EVENT) {
        std::cout << "Closing event caught. Performing cleanup..." << std::endl;

        cli::tout << "Release SDK resources.\n";
        SDK::Release();

        cli::tout << "Exiting application.\n";
        std::exit(EXIT_SUCCESS);

        std::cout << "Cleanup completed." << std::endl;
    }
    return TRUE;
}

int getRandomNumber(int min, int max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
}

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

cv::Mat readImage(std::string image_path) {
    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        throw std::runtime_error("Could not read the image!");
    }
    return image;
}
void compressImage(cv::Mat image, std::string image_path, int resize_width, int resize_height) {
    cv::Mat resized_image;
    //cv::Size size(image.cols / compress_factor, image.rows / compress_factor);
    cv::Size size(resize_width, resize_height);
    cv::resize(image, resized_image, size);

    cv::imwrite(image_path, resized_image);
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
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        std::cerr << "ERROR: Could not set control handler" << std::endl;
        return 1;
    }

    auto camera = InitializeCamera();
    if (!camera) {
        cli::tout << "Initialize camera failed!\n";
        std::exit(EXIT_FAILURE);
    }

    crow::App<crow::CORSHandler> app;

    // Customize CORS
    auto& cors = app.get_middleware<crow::CORSHandler>();


    web::http::client::http_client_config config;
    config.set_timeout(std::chrono::seconds(300));
    web::http::client::http_client client(U("http://localhost:23333/api/detect/surface/create"), config);

    // clang-format off
    cors
        .global()
        .headers("Content-Type", "Authorization", "ngrok-skip-browser-warning")
        .methods("POST"_method, "GET"_method)
        .origin("*");

    CROW_ROUTE(app, "/")
        ([]() {
        return "Check Access-Control-Allow-Methods header";
    });

    //define your endpoint at the root directory
    CROW_ROUTE(app, "/take_photo").methods("POST"_method)([&camera](const crow::request& req) {
        crow::json::rvalue request_data = crow::json::load(req.body);
        crow::json::wvalue result;

        if (!request_data || !request_data.has("isCompress")) {
            result["message"] = "Invalid request data";
            return crow::response(400, result);
        }

        bool is_compress = request_data["isCompress"].b();

        int seq_number = getRandomNumber(1, 9999);
        std::string seq_str = intToFiveDigitString(seq_number);
        std::string now_time = getCurrentDateTime();
        std::string image_name = now_time + seq_str + IMAGE_EXTENSION;
        std::string image_path_local = FOLDER_PATH_LOCAL +image_name;
        //std::string image_path_remote = FOLDER_PATH_REMOTE + now_time + seq_str + IMAGE_EXTENSION;
        std::cout << image_path_local << "\n";

        cli::text folder_path = convertStringToText(FOLDER_PATH_LOCAL);
        cli::text time_prefix = convertStringToText(now_time);
        camera->set_save_info(folder_path, time_prefix, seq_number);

        camera->capture_image();
        Sleep(1200);

        cv::Mat image;
        try {
            image = readImage(image_path_local);
        }
        catch (const std::runtime_error& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            result["message"] = e.what();
            return crow::response(500, result);
        }


        if (is_compress) {
            compressImage(image, image_path_local, 1620, 1080);
        } else {
            compressImage(image, image_path_local, 9072, 6048);
        }

        result["message"] = "Photo taken successfully";
        //result["image_path"] = image_path_remote;
        result["image_path"] = "http://" + host + ":" + std::to_string(port) + "/images/" + image_name;

        return crow::response{ result };
        });


    CROW_ROUTE(app, "/detect").methods("POST"_method)([&camera, &client](const crow::request& req){
        crow::json::rvalue request_data = crow::json::load(req.body);
        crow::json::wvalue result;

        if (!request_data) {
            return crow::response(400, "Invalid JSON");
        }
        std::string detectProductId = request_data["detectProductId"].s();
        std::string surfaceId = request_data["surfaceId"].s();
        int64_t sequenceNumber = request_data["sequenceNumber"].i();
        // get image path
        int seq_number = getRandomNumber(1, 9999);
        std::string seq_str = intToFiveDigitString(seq_number);
        std::string now_time = getCurrentDateTime();
        std::string image_name = now_time + seq_str + IMAGE_EXTENSION;
        std::string image_path_local = FOLDER_PATH_LOCAL +image_name;
        std::cout << image_path_local << "\n";
        // set save info
        cli::text folder_path = convertStringToText(FOLDER_PATH_LOCAL);
        cli::text time_prefix = convertStringToText(now_time);
        camera->set_save_info(folder_path, time_prefix, seq_number);
        // capture image
        camera->capture_image();
        Sleep(1200);

        cv::Mat image;
        try {
            image = readImage(image_path_local);
        }
        catch (const std::runtime_error& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            result["message"] = e.what();
            return crow::response(500, result);
        }

        compressImage(image, image_path_local, 9072, 6048);

        // make new request
        web::json::value postData = web::json::value::object();
        postData[U("imagePath")] = web::json::value::string(utility::conversions::to_string_t(image_path_local));
        postData[U("detectProductId")] = web::json::value::string(utility::conversions::to_string_t(detectProductId));
        postData[U("surfaceId")] = web::json::value::string(utility::conversions::to_string_t(surfaceId));
        postData[U("sequenceNumber")] = web::json::value::number(sequenceNumber);

        // send request
        web::http::http_response httpResponse;
        try {
            httpResponse = client.request(web::http::methods::POST, U(""),
                                          postData.serialize(), U("application/json")).get();
        } catch (const std::exception &e) {
            // 处理异常
            return crow::response(500, e.what());
        }

        if (httpResponse.status_code() == web::http::status_codes::OK) {
            auto jsonResponse = httpResponse.extract_json().get();
            std::string imagePath = "http://" + host + ":" + std::to_string(port) + "/images/" + image_name;
            jsonResponse[U("imagePath")] = web::json::value::string(utility::conversions::to_string_t(imagePath));
            std::string jsonResponseStr = utility::conversions::to_utf8string(jsonResponse.serialize());
            crow::json::rvalue jsonResponseRvalue = crow::json::load(jsonResponseStr);
            return crow::response{ crow::json::wvalue{ jsonResponseRvalue } };
        } else {
            // 错误处理
            return crow::response(httpResponse.status_code());
        }
    });


    CROW_ROUTE(app, "/exit")([]() {
        cli::tout << "Release SDK resources.\n";
        SDK::Release();

        cli::tout << "Exiting application.\n";
        std::exit(EXIT_SUCCESS);
        return "Exit";
    });

    std::string images_folder = FOLDER_PATH_LOCAL;

    CROW_ROUTE(app, "/images/<string>")([&images_folder](const std::string& filename) {
        std::string file_path = images_folder + filename;
        std::ifstream file(file_path, std::ios::binary);
        if (file) {
            std::stringstream buffer;
            buffer << file.rdbuf();

            crow::response resp(buffer.str());
            resp.set_header("Content-Type", "image/jpeg");  // 适当调整 MIME 类型
            return resp;
        }
        else {
            return crow::response(404);
        }
        });

    //set the port, set the app to run on multiple threads, and run the app
    app.port(port).multithreaded().run();

    //cli::tout << "Release SDK resources.\n";
    //SDK::Release();

    //cli::tout << "Exiting application.\n";
    //std::exit(EXIT_SUCCESS);
}
