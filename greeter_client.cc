#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <grpcpp/grpcpp.h>
#include "helloworld.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;

using helloworld::Greeter;
using helloworld::HelloRequest;
using helloworld::HelloReply;
using helloworld::FileRequest;
using helloworld::FileChunk;
using helloworld::ChatMessage;

std::unique_ptr<Greeter::Stub> stub;

void DownloadFile(const std::string& filename) {
    FileRequest request;
    request.set_filename(filename);

    ClientContext context;
    std::unique_ptr<ClientReader<FileChunk>> reader(stub->DownloadFile(&context, request));

    std::ofstream output("downloaded_" + filename, std::ios::binary);
    if (!output.is_open()) {
        std::cerr << "Failed to open output file." << std::endl;
        return;
    }

    FileChunk chunk;
    while (reader->Read(&chunk)) {
        output.write(chunk.content().data(), chunk.content().size());
    }
    output.close();

    Status status = reader->Finish();
    if (status.ok()) {
        std::cout << "File downloaded successfully." << std::endl;
    } else {
        std::cerr << "Download failed: " << status.error_message() << std::endl;
    }
}

void StartChat(const std::string& username) {
    ClientContext context;
    std::shared_ptr<ClientReaderWriter<ChatMessage, ChatMessage>> stream(stub->Chat(&context));

    std::thread writer([&]() {
        std::string input;
        while (true) {
            std::getline(std::cin, input);
            if (input == "exit") {
                stream->WritesDone();
                break;
            }
            ChatMessage msg;
            msg.set_user(username);
            msg.set_message(input);
            msg.set_timestamp(time(nullptr));
            stream->Write(msg);
        }
    });

    ChatMessage server_msg;
    while (stream->Read(&server_msg)) {
        std::cout << "[" << server_msg.user() << "]: " << server_msg.message() << std::endl;
    }
    writer.join();
    Status status = stream->Finish();
    if (!status.ok()) {
        std::cerr << "Chat ended with error: " << status.error_message() << std::endl;
    }
}

void ShowMainMenu(const std::string& token, const std::string& username) {
    while (true) {
        std::cout << "\nMain Menu:\n";
        std::cout << "1. Say Hello\n";
        std::cout << "2. Say Hello Again\n";
        std::cout << "3. Generate Token\n";
        std::cout << "4. Download File\n";
        std::cout << "5. Start Chat\n";
        std::cout << "6. Log out\n";
        std::cout << "Choose an option: ";
        int option;
        std::cin >> option;
        std::cin.ignore();

        if (option == 1 || option == 2 || option == 3) {
            HelloRequest request;
            request.set_name(username);
            request.set_client_id(username);
            HelloReply reply;
            ClientContext context;

            if (option == 1) {
                stub->SayHello(&context, request, &reply);
            } else if (option == 2) {
                stub->SayHelloAgain(&context, request, &reply);
            } else {
                stub->GenerateToken(&context, request, &reply);
                std::cout << "New Token: " << reply.set_token() << std::endl;
                continue;
            }

            std::cout << reply.message() << std::endl;
        } else if (option == 4) {
            std::string filename;
            std::cout << "Enter filename to download: ";
            std::getline(std::cin, filename);
            DownloadFile(filename);
        } else if (option == 5) {
            StartChat(username);
        } else if (option == 6) {
            std::remove("auth.txt");
            std::cout << "Logged out successfully.\n";
            break;
        } else {
            std::cout << "Invalid option.\n";
        }
    }
}

int main(int argc, char** argv) {
    stub = Greeter::NewStub(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));

    std::string token, username;
    std::ifstream authFile("auth.txt");

    if (!authFile.is_open()) {
        std::cout << "1. Register\n2. Login\nChoose option: ";
        int choice;
        std::cin >> choice;
        std::cin.ignore();

        std::string name;
        std::cout << "Enter username: ";
        std::getline(std::cin, name);

        HelloRequest request;
        request.set_name(name);
        request.set_client_id(name);
        HelloReply reply;
        grpc::ClientContext context;

        if (choice == 1) {
            stub->SayHello(&context, request, &reply);
            std::cout << reply.message() << "\n";
        } else {
            stub->SayHelloAgain(&context, request, &reply);
            std::cout << reply.message() << "\n";
        }

        grpc::ClientContext tokenContext;
        stub->GenerateToken(&tokenContext, request, &reply);
        token = reply.set_token();
        username = name;

        std::ofstream out("auth.txt");
        out << token << "\n" << username;
        out.close();

        std::cout << "Token saved.\n";
    } else {
        std::getline(authFile, token);
        std::getline(authFile, username);
        authFile.close();
    }

    ShowMainMenu(token, username);
    return 0;
}