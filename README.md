# cloudprocess


# 使用AWS C++ SDK、lambda和S3部署基于云端的文件处理服务

平时的工作中团队主要以开发桌面端程序为主，采用C++和Qt，在Visual studio 2017下进行开发。而我之前的工作是主要以云端app为主，因此萌生了将桌面端程序中耦合比较低的一部分代码转移为单独的模块以云端APP的方式进行部署的想法，这一过程中涉及到多个aws服务，包括lambda，cpp sdk和s3， API gateway。


## 1. 代码结构

代码的结构如下

![image-20210921120707691](C:\Users\huat-wuxi001\Documents\OneDrive\HUSTWUXI\研究院工作\工作总结\叶涛\云端部署文件处理服务\项目结构.png)



## 2. 环境准备

### 2.1 准备虚拟机

由于云端环境的部署种用到的服务和工具极大的依赖Linux环境，而我们平时查找资料或者用的计算分析或者CAD软件以Windows平台为主，为了更加方便的在两个系统种切换，建议安装虚拟机，本工作种采用VMWare Player （免费），安装ubuntu18.04镜像。



### 2.2 Ubuntu 镜像

为了在ubuntu下更好的更新和下载程序包，我们将ubuntu软件源更换为清华镜像（https://mirrors.tuna.tsinghua.edu.cn/help/ubuntu/）

```
# 更改 apt源文件
sudo vi /etc/apt/sources.list
```

将如下的地址替换进来

```
# 默认注释了源码镜像以提高 apt update 速度，如有需要可自行取消注释
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ bionic main restricted universe multiverse
# deb-src https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ bionic main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ bionic-updates main restricted universe multiverse
# deb-src https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ bionic-updates main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ bionic-backports main restricted universe multiverse
# deb-src https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ bionic-backports main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ bionic-security main restricted universe multiverse
# deb-src https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ bionic-security main restricted universe multiverse

# 预发布软件源，不建议启用
# deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ bionic-proposed main restricted universe multiverse
# deb-src https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ bionic-proposed main restricted universe multiverse
```

完成替换后，执行更新

```
sudo apt-get update
sudo apt-get upgrade
```

### 2.3 VPN代理设置

由于开发过程中涉及从Github上clone代码，以及和AWS服务器的上传下载等操作，经过多种方式尝试，目前看来VPN是最直接可靠的方式。本文采用Clash。从如下地址下载clash

https://github.com/Dreamacro/clash/releases

执行 `cd && mkdir clash` 在用户目录下创建 clash 文件夹。

下载适合的 Clash 二进制文件并解压重命名为 `clash`

一般个人的64位电脑下载 clash-linux-amd64.tar.gz 即可。

在终端 `cd` 到 Clash 二进制文件所在的目录，执行 `wget -O config.yaml [clash订阅地址]`

上文中[ ]中的文字需要购买相应供应商的订阅。这里config.yaml如果在linux环境下无法下载（这个可能时下载的时候网络问题），可以在window下先预先下载好。

执行 `./clash -d .` 即可启动 Clash，同时启动 HTTP 代理和 Socks5 代理。

如提示权限不足，请执行 `chmod +x clash`

启动clash后将其设为系统代理

打开系统设置，选择网络，点击网络代理右边的 ⚙ 按钮，选择手动，填写 HTTP 和 HTTPS 代理为 `127.0.0.1:7890`，填写 Socks 主机为 `127.0.0.1:7891`，即可启用系统代理



### 2.4 Github DNS 解析和镜像

方法1：修改host （不建议使用，可尝试）

由于我们需要从github上下载很多sdk编译用作开发依赖，而目前GitHub的域名解析由于国内防火墙的原因被污染，为了减少麻烦，我们可以修改host文件直接按照ip地址访问或者使用镜像站点。

```
sudo /etc/hosts
```

从下面地址获取最新域名解析内容

https://github.com/521xueweihan/GitHub520

并且添加到hosts文件种保存

方法2： Gitee镜像

Gitee提供了每日和github上仓库的同步服务，我们可以到如下地址，搜索是否有github上的同名镜像，然后从gitee下载即可：

https://gitee.com/mirrors

### 2.5 配置ssh key

github的新政策让我们不能使用http方式git clone，需要配置ssh key。

```shell
ssh-keygen -t ed25519 -C "your_email@example.com"
```

将生成的公钥添加到github中，接下来的clone操作都会基于ssh。



## 3. 工具和依赖包安装

部署aws需要配套的aws cli工具，同时因为我们部署的是cpp工程，通过cpp程序来驱动aws上的各项服务，因此我们需要安装aws cpp sdk。lambda服务还需要安装aws lambda runtime。

### 3.1 安装CMake最新版

aws sdk cpp的编译需要比较新的版本的cmake，而ubuntu通过sudo apt安装的版本比较低，因此我们需要自行安装新版本。安装方法这里参考的是https://askubuntu.com/questions/355565/how-do-i-install-the-latest-version-of-cmake-from-the-command-line

首先删除系统里可能已经有的cmake老的版本

```sh
sudo apt remove --purge --auto-remove cmake
```

准备

```
sudo apt update && sudo apt install -y software-properties-common lsb-release && sudo apt clean all
```

获取kitware的sign key

```sh
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
```

将kitware增加到source

```sh
sudo apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
```

反正就是坐下如下操作吧

```sh
sudo apt update
sudo apt install kitware-archive-keyring
sudo rm /etc/apt/trusted.gpg.d/kitware.gpg
```

安装如下开发工具

```sh
sudo apt install build-essential libtool autoconf unzip wget libssl-dev
```

执行如下操作下载最新的cmake，这个可能需要点时间，因为网络的原因，毕竟是外网。

```sh
version=3.21
build=2
mkdir ~/temp
cd ~/temp
wget https://cmake.org/files/v$version/cmake-$version.$build.tar.gz
tar -xzvf cmake-$version.$build.tar.gz
cd cmake-$version.$build/
```

编译并安装cmake

```sh
./bootstrap
make -j$(nproc)
sudo make install
```



### 3.2 安装aws cli

回到~/temp，这是个我们默认安装第三方程序的零时目录，使用curl下载aws cli程序包并执行安装脚本。

先安装curl工具和zlib

```
sudo apt install curl
sudo apt install libcurl4-openssl-dev
sudo apt install zlib1g-dev
```

然后下载并安装awscli

```
curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
unzip awscliv2.zip
sudo ./aws/install
```

### 3.3 安装 AWS C++ SDK

注意这里需要几个小时的时间才能安装成功。安装之前，我们还是记得要看下本地又没哟安装git，如果没有的话先安装git 工具

```
sudo apt install git
```

下面正式安装aws-cpp-sdk，首先clone sdk的文件

```
$ mkdir ~/install
$ git clone --recurse-submodules https://github.com/aws/aws-sdk-cpp

```

尽管我们可以用镜像或者host修改来加速github访问，但是笔者的实践难免发现有时候两者都不好用，尤其设计到submodule，这样在完成了上面的clone后会有丢失的模块没有成功下载，这是我们需要不断的使用

```
cd aws-sdk-cpp
git submodule --recursive --init
```

直到最终全部模块都已经clone下来

```
$ mkdir build
$ cd build
$ cmake .. 
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DCUSTOM_MEMORY_MANAGEMENT=OFF \
  -DCMAKE_INSTALL_PREFIX=~/install \
  -DENABLE_UNITY_BUILD=ON

$ make
$ make install
```

### 3.4 安装AWS Lambda C++ Runtime

```
$ git clone https://github.com/awslabs/aws-lambda-cpp-runtime.git
$ cd aws-lambda-cpp-runtime
$ mkdir build
$ cd build
$ cmake .. -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_INSTALL_PREFIX=~/install \
$ make
$ make install
```

至此，所有的library和include头文件都已经安装到~/install目录下。

## 4. 实例工程

本文以一个文件处理功能的lambda函数的开发为例。我们称整个工程cloudpost。Linux下可以选择vs code或者Clion开发，写CMake文件来指定编译规则。以下为CMakeLists.txt, main.cpp, utility.h 三个函数

工程的功能时通过HTTP POST API传递JSON消息

{

​	”s3bucket": "receiver",

​    "s3key": "file.txt",

​	"s3region":"ap-east-1"

}

从而将file.txt 中的数据格式做转化，存到s3 bucket中

原始格式为：

FORMAT/34.001, 23.021, 2.2,0.1,0.2,0.3

...

转换后的格式为：

X34.001 Y23.021 Z2.2 TX0.1 TY0.2 TZ0.3

...

```c++
cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)
project(cloudpost LANGUAGES CXX)

include_directories(${PROJECT_SOURCE_DIR}/inc)

find_package(aws-lambda-runtime)
find_package(AWSSDK COMPONENTS s3)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/src/main.cpp ${PROJECT_SOURCE_DIR}/src/coordinate.cpp ${PROJECT_SOURCE_DIR}/src/geometry.cpp ${PROJECT_SOURCE_DIR}/src/Point3D.cpp ${PROJECT_SOURCE_DIR}/src/postprocessor.cpp ${PROJECT_SOURCE_DIR}/src/vector.cpp inc/utility.h)

target_link_libraries(${PROJECT_NAME} PRIVATE AWS::aws-lambda-runtime ${AWSSDK_LINK_LIBRARIES})

target_compile_options(${PROJECT_NAME} PRIVATE
    "-Wall"
    "-Wextra"
    "-Wconversion"
    "-Wshadow"
    "-Wno-sign-conversion")

target_compile_features(${PROJECT_NAME} PRIVATE "cxx_std_11")

aws_lambda_package_target(${PROJECT_NAME})
```



```
#include <aws/core/Aws.h>
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/HashingUtils.h>
#include <aws/core/platform/Environment.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/lambda-runtime/runtime.h>
#include <iostream>
#include <fstream>
#include <memory>
#include "postprocessor.h"

#include <aws/s3/model/PutObjectRequest.h>
#include <aws/core/utils/memory/stl/AWSStringStream.h>
#include <sys/stat.h>
#include "utility.h"

using namespace aws::lambda_runtime;
using namespace Aws::S3;
using namespace Aws::S3::Model;

static const char* KEY = "test.txt";//"s3_cpp_sample_key";
static const char* BUCKET = "cloudpost-receiver";//"s3-cpp-sample-bucket";


std::function<std::shared_ptr<Aws::Utils::Logging::LogSystemInterface>()> GetConsoleLoggerFactory()
{
    return [] {
        return Aws::MakeShared<Aws::Utils::Logging::ConsoleLogSystem>(
            "console_logger", Aws::Utils::Logging::LogLevel::Trace);
    };
}



int main()
{
    using namespace Aws;
    SDKOptions options;
    options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace;
    options.loggingOptions.logger_create_fn = GetConsoleLoggerFactory();

    InitAPI(options);
    {
        Client::ClientConfiguration config;
        config.region = Aws::Environment::GetEnv("AWS_REGION");
        config.caFile = "/etc/pki/tls/certs/ca-bundle.crt";

        auto credentialsProvider = Aws::MakeShared<Aws::Auth::EnvironmentAWSCredentialsProvider>(TAG);
        S3::S3Client client(credentialsProvider, config);
        auto handler_fn = [&client](aws::lambda_runtime::invocation_request const& req) {
        return post_handler_api(req, client);
        };
        run_handler(handler_fn);
    }
    ShutdownAPI(options);
    return 0;
}

```



```c++
//
// Created by yetao on 9/29/21.
//


#ifndef CLOUDPOST_UTILITY_H
#define CLOUDPOST_UTILITY_H
#include <iostream>
#include <regex>
#include <string>
#include <future>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/memory/stl/SimpleStringStream.h>

char const TAG[] = "LAMBDA_ALLOC";
using namespace aws::lambda_runtime;
using namespace Aws::S3;
using namespace Aws::S3::Model;
using namespace std;

vector<vector<double>> post_solution1(vector<string> cls_string);

void call_post(std::vector<std::string>& cls_string, std::string post_name, std::vector<std::string>& nc_string)
{
    vector<vector<double>> clsValueVector;
    clsValueVector =  post_solution1(cls_string);
    int line_num = clsValueVector.size();
    int field_num = clsValueVector[0].size();
    for (int i = 0;i<line_num;i++)
        nc_string.push_back("X"+std::to_string(clsValueVector[i][0]) + " Y"+
        std::to_string(clsValueVector[i][1])+" Z"+std::to_string(clsValueVector[i][2]) +
        " TX"std::to_string(clsValueVector[i][3])+" TY="std::to_string(clsValueVector[i][4])+" TZ="std::to_string(clsValueVector[i][5]));
}

std::vector<double> parseClsSingleLine(const string& cls_string)
{
    const std::regex rx_head_FORMAT(R"(^(FORMAT))");
    const std::regex rx_body_FORMAT(R"(([+-]?[\d]+(\.\d+)?)(?=$|\s|,))");
    // Declare the regex with a raw string literal
    match_results<string::const_iterator> m;
    match_results<string::const_iterator> n;
    auto str = cls_string;
    vector<double> cls_value;
    auto elem_count = 0;
    if(regex_search(str, n, rx_head_FORMAT) )
        while (regex_search(str,m,rx_body_FORMAT) && elem_count<6 )
        {
            cls_value.push_back(stod(m[1]));
            str = m.suffix().str(); // Proceed to the next match
            elem_count++;
        }
    return cls_value;
}

vector<vector<double>> post_solution1(vector<string> cls_string)
{
    vector<vector<double>> clsValueVector;

    for (auto it = cls_string.begin(); it < cls_string.end(); ++it)
    {
        vector<double> cls_value = parseClsSingleLine(*it);
        vector<double> cls_value_new;

        if(cls_value.size()>0)
        {
            for (auto it_value = cls_value.begin(); it_value < cls_value.end(); ++it_value)
                cls_value_new.push_back(*it_value);
            clsValueVector.push_back(cls_value_new);

        }
    }
    return clsValueVector;
}

void write(std::vector<std::string> v){
    ofstream file;
    file.open("/tmp/example.txt");

    for(std::vector<std::string>::const_iterator iter=v.begin();iter!=v.end();++iter)
    {
        file<<*iter<<std::endl;
    }
}

bool getFileContent(Aws::IOStream& in, std::vector<std::string> & vec_of_strs)
{
    if (!in)
    {
        std::cerr << "Cannot open the File : " << std::endl;
        return false;
    }

    std::string str;
    // Read the next line from File untill it reaches the end.
    while (std::getline(in, str))
    {
        // Line contains string of length > 0 then save it in vector
        if (!str.empty())
            vec_of_strs.push_back(str);
    }
    return true;
}


std::string download_and_parse_file(
        Aws::S3::S3Client const& client,
        Aws::String const& bucket,
        Aws::String const& key,
        Aws::String& encoded_output,
        std::vector<std::string>& clsString,
        Aws::SimpleStringStream& ss)
{
    using namespace Aws;

    S3::Model::GetObjectRequest request;
    request.WithBucket(bucket).WithKey(key);

    auto outcome = client.GetObject(request);
    if (outcome.IsSuccess()) {
        AWS_LOGSTREAM_INFO(TAG, "Download completed!");
        auto& s = outcome.GetResult().GetBody();
        bool result = getFileContent(
                s,
                clsString);
        if (result)
            encoded_output = Aws::String("file process successfully");
        else
            ss<<"\n"<<"seems getfieContent failed...";
        return {}; //parse(s, encoded_output, clsString);
    }
    else {
        ss<<"\n"<<"seems getobject failed..."<<"\n";
        AWS_LOGSTREAM_ERROR(TAG, "Failed with error: " << outcome.GetError());
        return outcome.GetError().GetMessage();
    }
}

// customized function declare yetao
bool PutObject(const Aws::String& bucketName,
               const Aws::String& objectName,
               const Aws::String& region,
               Aws::S3::S3Client const& s3_client)
{
    // Verify that the file exists.
    struct stat buffer;

    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(bucketName);
    //We are using the name of the file as the key for the object in the bucket.
    //However, this is just a string and can set according to your retrieval needs.
    request.SetKey(objectName);

    std::shared_ptr<Aws::IOStream> input_data =
            Aws::MakeShared<Aws::FStream>("SampleAllocationTag",
                                          "/tmp/example.txt",
                                          std::ios_base::in | std::ios_base::binary);

    request.SetBody(input_data);

    Aws::S3::Model::PutObjectOutcome outcome =
            s3_client.PutObject(request);

    if (outcome.IsSuccess()) {

        std::cout << "Added object '" << objectName << "' to bucket '"
                  << bucketName << "'.";
        return true;
    }
    else
    {
        std::cout << "Error: PutObject: " <<
                  outcome.GetError().GetMessage() << std::endl;

        return false;
    }
}


static invocation_response post_handler_api(invocation_request const& request, Aws::S3::S3Client const& client)
{
    using namespace Aws::Utils::Json;

    JsonValue json(request.payload);
    if (!json.WasParseSuccessful()) {
        return invocation_response::failure("Failed to parse input JSON", "InvalidJSON");
    }

    auto v = json.View();
    Aws::SimpleStringStream ss;
    ss << "Good ";

    if (v.ValueExists("body") && v.GetObject("body").IsString()) {
        auto body = v.GetString("body");
        JsonValue body_json(body);

        if (body_json.WasParseSuccessful()) {
            auto body_v = body_json.View();
            auto bucket = body_v.GetString("s3bucket");
            auto key = body_v.GetString("s3key");
            auto region = body_v.GetString("s3region");
            ss << ", "<<bucket<<key;

            // perform post
            AWS_LOGSTREAM_INFO(TAG, "Attempting to download file from s3://" << bucket << "/" << key);

            Aws::String base64_encoded_file;
            std::vector<std::string> cls_string; // vector of strings for storage of cls data
            auto err = download_and_parse_file(client, bucket, key, base64_encoded_file, cls_string, ss);
            if (!err.empty()) {
                return invocation_response::failure(err, "DownloadFailure");
            }

            // here is post process call
            std::vector<std::string> nc_string;
            std::string post_name = "test";
            call_post(cls_string, post_name, nc_string);

            //customized function implement - yetao
            // make a file
            write(nc_string);


            if (!PutObject("cloudpost-receiver", "/example.txt", "ap-east-1", client))
            {
                invocation_response::failure(err, "cannot put object");;
            }
            
        } else
        {
            ss<<"seems json message parsed failed..."<<"\n";
        }
    }

    JsonValue resp;
    resp.WithString("message", ss.str());

    return invocation_response::success(resp.View().WriteCompact(), "application/json");
}
#endif //CLOUDPOST_UTILITY_H

```

### 4.1 部署

对上述工程进行编译

```
$ mkdir build
$ cd build
$ cmake .. -DCMAKE_BUILD_TYPE= -DCMAKE_INSTALL_PREFIX=~/lambda-install
$ make
$ make cloudpost
```

上述命令会将编译好的代码打包成cloudpost.zip

建立如下policy

```
$ cat trust-policy.json
{
 "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Principal": {
        "Service": ["lambda.amazonaws.com"]
      },
      "Action": "sts:AssumeRole"
    }
  ]
}


```

建立iam role

```
aws iam create-role --role-name cloudpost --assume-role-policy-document file://trust-policy.json
```

记录下 role arn

然后执行如下命令，赋予lambda写cloudwatch权限

```
$ aws iam attach-role-policy --role-name cloudpost --policy-arn arn:aws:iam::aws:policy/service-role/AWSLambdaBasicExecutionRole
```

在linux bash shell中建立新的lambda函数

```
$ aws lambda create-function --function-name cloudpost \
--role <specify role arn from previous step here> \
--runtime provided --timeout 30 --memory-size 128 \
--handler cloudpost --zip-file fileb://cloudpost.zip

```

### 4.2 更新函数

如果我们对代码做了更新，可以用如下命令将更新好的代码重新部署

```
aws lambda update-function-code --function-name cloudpost --zip fileb://cloudpost.zip
```

## 5. 测试

我们可以测试以上lambda函数，通过postman发送http post请求即可。
