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
#include <fstream>
#include <grpcpp/grpcpp.h>

#ifdef BAZEL_BUILD
#include "helloworld.grpc.pb.h"
#else
#include "helloworld.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using helloworld::Greeter;
using helloworld::HelloReply;
using helloworld::HelloRequest;
using helloworld::FileRequest;
using helloworld::FileChunk;
class GreeterClient {
 public:
  GreeterClient(std::shared_ptr<Channel> channel)
      : stub_(Greeter::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  std::string SayHello(const std::string& user) {
    // Data we are sending to the server.
    HelloRequest request;
    request.set_name(user);

    // Container for the data we expect from the server.
    HelloReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->SayHello(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.message();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

  std::string SayHelloAgain(const std::string& user) {
    // Follows the same pattern as SayHello.
    HelloRequest request;
    request.set_name(user);
    HelloReply reply;
    ClientContext context;

    // Here we can use the stub's newly available method we just added.
    Status status = stub_->SayHelloAgain(&context, request, &reply);
    if (status.ok()) {
      return reply.message();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

 private:
  std::unique_ptr<Greeter::Stub> stub_;
};

class AuthClient {
public:
    AuthClient(std::shared_ptr<Channel> channel)
        : stub_(Greeter::NewStub(channel)) {}

    std::string GenerateToken(const std::string& client_id) {
        HelloRequest request;
        request.set_client_id(client_id);

        HelloReply reply;
        ClientContext context;

        Status status = stub_->GenerateToken(&context, request, &reply);

        if (status.ok()) {
            return reply.set_token();
            std::cout << "Token: " << reply.set_token() << std::endl;
        } else {
            std::cerr << "gRPC call failed: " << status.error_message() << std::endl;
            std::cerr << status.error_code() << " - " << status.error_message() << std::endl;

            return "";
        }
    }

private:
    std::unique_ptr<Greeter::Stub> stub_;
};


class FileClient {
public:
    FileClient(std::shared_ptr<Channel> channel)
        : stub_(Greeter::NewStub(channel)) {}

    void DownloadFile(const std::string& filename, const std::string& output_path) {
        FileRequest request;
        request.set_filename(filename);
        ClientContext context;

        std::unique_ptr<grpc::ClientReader<FileChunk>> reader(
            stub_->DownloadFile(&context, request));

        std::ofstream output_file(output_path, std::ios::out | std::ios::binary);
        if (!output_file.is_open()) {
            std::cerr << "Failed to open output file: " << output_path << std::endl;
            return;
        }

        FileChunk chunk;
        while (reader->Read(&chunk)) {
            output_file.write(chunk.content().data(), chunk.content().size());
        }

        Status status = reader->Finish();
        if (status.ok()) {
            std::cout << "File downloaded successfully." << std::endl;
        } else {
            std::cerr << "gRPC failed: " << status.error_message() << filename << std::endl;
        }

        output_file.close();
    }

private:
    std::unique_ptr<Greeter::Stub> stub_;
};


int main(int argc, char** argv) {
  std::string address = "localhost";
  std::string port = "50051";
  std::string server_address = address + ":" + port;
  std::cout << "Client querying server address: " << server_address << std::endl;


  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint (in this case,
  // localhost at port 50051). We indicate that the channel isn't authenticated
  // (use of InsecureChannelCredentials()).
  GreeterClient greeter(grpc::CreateChannel(
      server_address, grpc::InsecureChannelCredentials()));
  std::string user("Muhannad");

  std::string reply = greeter.SayHello(user);
  std::cout << "Greeter received: " << reply << std::endl;

  reply = greeter.SayHelloAgain(user);
  std::cout << "Greeter received: " << reply << std::endl;

  AuthClient client(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));
    std::string token = client.GenerateToken(user);
    std::cout << "Received token: " << token << std::endl;

    FileClient fclient(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));
    fclient.DownloadFile("New Rich Text Document.rtf", "C:\\Users\\DELL\\Desktop\\grpc_downloaded_file.txt");

  return 0;
}
