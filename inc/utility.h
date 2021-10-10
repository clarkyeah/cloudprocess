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

vector<vector<double>> process_solution1(vector<string> cls_string);

void call_process(std::vector<std::string>& raw_format, std::string post_name, std::vector<std::string>& new_format)
{
    // converting example here to parse line by line string into another format
    vector<vector<double>> ValueVector;
    ValueVector =  process_solution1(raw_format);
    int line_num = ValueVector.size();
    int field_num = ValueVector[0].size();
    for (int i = 0;i<line_num;i++)
        new_format.push_back("X"+std::to_string(ValueVector[i][0]) + " Y"+
        std::to_string(ValueVector[i][1])+" Z"+std::to_string(ValueVector[i][2]) +
        " A3="+std::to_string(ValueVector[i][3])+" B3="+std::to_string(ValueVector[i][4])+" C3="+std::to_string(ValueVector[i][5]));
}

std::vector<double> parseSingleLine(const string& raw_format)
{
    const std::regex rx(R"((?:|\s)([+-]?[[:digit:]]+(?:\.[[:digit:]]+)?)(?=$|\s))"); //用正则split刀位文件得到其中的x,y,z,i,j,k
    // Declare the regex with a raw string literal
    match_results<string::const_iterator> m;
    auto str = raw_format;
    vector<double> ValueVector;
    while (regex_search(str, m, rx))
    {
        ValueVector.push_back(stod(m[1]));
        str = m.suffix().str(); // Proceed to the next match
    }
    return ValueVector;
}

vector<vector<double>> process_solution1(vector<string> cls_string)
{
    vector<vector<double>> clsValueVector;

    for (auto it = cls_string.begin(); it < cls_string.end(); ++it)
    {
        //if (std::string(*it).find("x") != std::string::npos) {
        vector<double> cls_value = parseSingleLine(*it);
        vector<double> cls_value_new;

        for (auto it_value = cls_value.begin(); it_value < cls_value.end(); ++it_value)
            cls_value_new.push_back(1 + *it_value);
        clsValueVector.push_back(cls_value_new);
        //}
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
    file.close();
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
        std::vector<std::string>& rawFormat)
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
                rawFormat);
        if (result)
            encoded_output = Aws::String("file process successfully");
        return {}; //parse(s, encoded_output, clsString);
    }
    else {
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
    ss << "JSON parse successful...\n ";

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

            Aws::String base64_encoded_output;
            std::vector<std::string> raw_format; // vector of strings for storage of cls data
            auto err = download_and_parse_file(client, bucket, key, base64_encoded_output, raw_format);
            if (!err.empty()) {
                return invocation_response::failure(err, "DownloadFailure");
            }

            // here is post process call
            std::vector<std::string> new_format;
            std::string post_name = "grob";
            call_process(raw_format, post_name, new_format);

            //customized function implement - yetao
            // make a file
            write(new_format);


            if (!PutObject("cloudpost-receiver", "/example.txt", "ap-east-1", client))
            {
                invocation_response::failure(err, "cannot put objectmak");;
            }
            //ss << (body_v.ValueExists("s3bucket") && body_v.GetObject("s3key").IsString() ? body_v.GetString("time") : "");
        }
    }

    JsonValue resp;
    resp.WithString("message", ss.str());

    return invocation_response::success(resp.View().WriteCompact(), "application/json");
}
#endif //CLOUDPOST_UTILITY_H
