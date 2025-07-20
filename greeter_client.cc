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
#include <fstream>
#include <grpcpp/grpcpp.h>

#ifdef BAZEL_BUILD
#include "helloworld.grpc.pb.h"
#else
#include "helloworld.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::Status;
using helloworld::Greeter;
using helloworld::HelloReply;
using helloworld::HelloRequest;
using helloworld::FileRequest;
using helloworld::FileChunk;

using helloworld::ChatMessage;
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

class ChatClient {
public:
    ChatClient(std::shared_ptr<Channel> channel) 
        : stub_(Greeter::NewStub(channel)) {}

    void Chat(const std::string& username) {
    ClientContext context;
    std::shared_ptr<ClientReaderWriter<ChatMessage, ChatMessage>> stream(stub_->Chat(&context));

    std::atomic<bool> done(false);

    // Writer thread
    std::thread writer([stream, username, &done]() {
        std::string input;
        while (true) {
            std::getline(std::cin, input);

            if (input == "exit" || input == "quit") {
                std::cout << "Exiting chat...\n";
                stream->WritesDone();  // Finish sending to server
                done = true;
                break;
            }

            ChatMessage msg;
            msg.set_user(username);
            msg.set_message(input);
            msg.set_timestamp(time(nullptr));
            stream->Write(msg);
        }
    });

    // Reader loop
    ChatMessage server_msg;
    while (stream->Read(&server_msg)) {
        std::cout << "[" << server_msg.user() << "]: " << server_msg.message() << "\n";
    }

    writer.join();  // Wait for the writer thread to finish
    stream->Finish();  // Finalize the stream
}


private:
    std::unique_ptr<Greeter::Stub> stub_;
};


int main(int argc, char** argv) {
    std::string address = "localhost";
    std::string port = "50051";
    std::string server_address = address + ":" + port;
    std::cout << "Client connected to: " << server_address << std::endl;

    auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
    GreeterClient greeter(channel);
    AuthClient auth(channel);
    FileClient file_client(channel);
    ChatClient chat(channel); // Uncomment if Chat is needed

    std::string user = "Muhannad";

    while (true) {
        std::cout << "\nSelect an option:\n"
                  << "1. Say Hello\n"
                  << "2. Say Hello Again\n"
                  << "3. Generate Token\n"
                  << "4. Download File\n"
                  << "5. Chat\n"  // Uncomment if Chat is active
                  << "6. Exit\n"
                  << "Enter choice: ";

        int choice;
        std::cin >> choice;
        std::cin.ignore();  // flush newline

        switch (choice) {
            case 1: {
                std::string reply = greeter.SayHello(user);
                std::cout << "Response: " << reply << std::endl;
                break;
            }
            case 2: {
                std::string reply = greeter.SayHelloAgain(user);
                std::cout << "Response: " << reply << std::endl;
                break;
            }
            case 3: {
                std::string token = auth.GenerateToken(user);
                std::cout << "Token: " << token << std::endl;
                break;
            }
            case 4: {
                std::string filename, output_path;
                std::cout << "Enter filename to download: ";
                std::getline(std::cin, filename);
                std::cout << "Enter output path: ";
                std::getline(std::cin, output_path);
                file_client.DownloadFile(filename, output_path);
                break;
            }
            case 5: {
                std::string username;
                std::cout << "Enter your chat username: ";
                std::getline(std::cin, username);
                chat.Chat(username);
                break;
            }
            case 6:
                std::cout << "Exiting client.\n";
                return 0;
            default:
                std::cout << "Invalid option.\n";
                break;
        }
    }

    return 0;
}

