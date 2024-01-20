# sony-sdk-customer

#### Developed based on [Sony Remote Sdk](https://support.d-imaging.sony.co.jp/app/sdk/en/index.html) to realize the interaction between the back-end using socket.io.

### Prerequisites
- Visual Studio 2022
- CMake 3.17.3 (and above)
- Crow installed by vcpkg

### Installation
1. Read and follow the instructions of `RemoteSampleApp_IM_v1.11.00.pdf`, and make sure the Sony camera is successfully connected
2. Clone the repository and cd into it
3. Open file `app/CRSDK/RemoteCli.cpp`, and modify the 31st line `FOLDER_PATH` to your path
4. Make build folder and execute cmake
   ```
   mkdir build
   cd build
   cmake ..
   ```
5. Open the solution file ¡°../build/RemoteCli.sln¡± and build it
