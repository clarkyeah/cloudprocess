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
