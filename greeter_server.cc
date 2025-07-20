/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <jwt-cpp/jwt.h>
#include <grpcpp/grpcpp.h>

#include <fstream>
#ifdef BAZEL_BUILD
#include "helloworld.grpc.pb.h"
#else
#include "helloworld.grpc.pb.h"
#endif

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ServerWriter;
using grpc::ServerReaderWriter;

using helloworld::HelloRequest;
using helloworld::HelloReply;
using helloworld::Greeter;
using helloworld::FileRequest;
using helloworld::FileChunk;

using helloworld::ChatMessage;
// Logic and data behind the server's behavior.
class GreeterServiceImpl final : public Greeter::Service {
  Status SayHello(ServerContext* context, const HelloRequest* request,
                  HelloReply* reply) override {
    std::string prefix("Hello ");
    reply->set_message(prefix + request->name());
    return Status::OK;
  }
//say hellow again
  Status SayHelloAgain(ServerContext* context, const HelloRequest* request,
                       HelloReply* reply) override {
    std::string prefix("Hello again ");
    reply->set_message(prefix + request->name());
    return Status::OK;
  }

  const std::string kSecret = "your-256-bit-secret";
//generate token
  Status GenerateToken(ServerContext* context, const HelloRequest* request, HelloReply* reply) override {
        std::string client_id = request->client_id();

        // Generate JWT token using jwt-cpp
        auto token = jwt::create()
            .set_issuer("auth_server")
            .set_type("JWT")
            .set_subject(client_id)
            .set_issued_at(std::chrono::system_clock::now())
            .set_expires_at(std::chrono::system_clock::now() + std::chrono::minutes{15})
            .sign(jwt::algorithm::hs256{kSecret});

        reply->set_set_token(token);
        std::cout << "Issued token for " << client_id << ": " << token << std::endl;
        return Status::OK;
    }
//file generation
    Status DownloadFile(ServerContext* context, const FileRequest* request, ServerWriter<FileChunk>* writer) override {
        std::string base_path = "C:/Users/DELL/Desktop/gRPC/grpc_getting_started/files/";
        std::string full_path = base_path + request->filename();
        std::ifstream file(full_path, std::ios::binary);
        if (!file.is_open()) {
            return Status(grpc::StatusCode::NOT_FOUND, "File not found");
        }

        const size_t buffer_size = 1024;
        char buffer[buffer_size];
        while (file.read(buffer, buffer_size) || file.gcount() > 0) {
            FileChunk chunk;
            chunk.set_content(buffer, file.gcount());
            writer->Write(chunk);
        }

        file.close();
        return Status::OK;
    }
//chat 
    grpc::Status Chat(ServerContext* context,
                  ServerReaderWriter<ChatMessage, ChatMessage>* stream) override {
    ChatMessage msg;
    bool running = true;

    // Writer thread sends messages periodically
    std::thread writer([&]() {
        while (running) {
            ChatMessage reply;
            reply.set_user("Server");
            reply.set_message("Pong from server");
            reply.set_timestamp(time(nullptr));
            stream->Write(reply);
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    });

    // Main reader loop
    while (stream->Read(&msg)) {
        std::string message = msg.message();
        std::cout << "[" << msg.user() << "]: " << message << "\n";

        // If client sends "exit", break and end chat for that client
        if (message == "exit") {
            std::cout << "[INFO] Client exited the chat.\n";
            break;
        }
    }

    running = false;  // Stop writer thread
    writer.join();    // Clean up the writer thread

    return grpc::Status::OK;
}

};



class AuthServiceImpl final : public Greeter::Service {
    
};

void RunServer() {
  std::string address = "0.0.0.0";
  std::string port = "50051";
  std::string server_address = address + ":" + port;
  GreeterServiceImpl service;
  AuthServiceImpl servicee;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  //builder.RegisterService(&servicee);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  RunServer();

  return 0;
}
