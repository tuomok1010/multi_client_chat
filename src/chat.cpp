#include <string>
#include <iostream>
#include <sys/socket.h>


// own classes
#include "server.h"

void PrintStartMessage()
{
    std::cout 
        << "************************************************\n"
        << "Welcome to a simple multi client chat server. After starting\n"
        << "the server you can simply use telnet to connect to it from a different terminal.\n"
        << "Example: 'telnet 127.0.0.1 9114'\n"
        << "************************************************\n\n";
}

std::string GetPortNum()
{
    std::string input{};
    do
    {
        std::cout << "Enter the port number to begin > " << std::flush;
        std::cin >> input;

        try
        {
            if(std::stoi(input) > 65535)
            {
                std::cout << "Error! Port number too large. Maximum is 65535. Press enter to try again." << std::flush;
                std::cin.ignore();
                std::cin.get();
            }
            else if(std::stoi(input) <= 1024)
            {
                std::cout << "Error! Port number should be larger than 1024. Press enter to try again." << std::flush;
                std::cin.ignore();
                std::cin.get();
            }
        }
        catch(const std::invalid_argument& e)
        {
            std::cerr << "Invalid port number. Press enter to try again." << std::endl;
            std::cin.ignore();
            std::cin.get();
            input = "0";
        }

        system("clear");

    } while (std::stoi(input) > 65535 || std::stoi(input) <= 1024);
   
    system("clear");
    return input;
}

int GetNumMaxConnections()
{
    int input{};
    do
    {
        std::cout << "Enter the max number of clients > " << std::flush;
        std::cin >> input;
        system("clear");

    } while (input > SOMAXCONN || input <= 0);
   
    system("clear");
    return input;
}

int main()
{

    PrintStartMessage();

    std::string port = GetPortNum();

    int maxConnections = GetNumMaxConnections();

    Server server(port, maxConnections);

    server.CreateSocket();

    server.Listen();

    std::cout << "waiting for connections..." << std::endl;

    while(true)
    {
        server.PrintPollList();

        server.Poll();

        server.Accept();
        
        server.RelayMessagesToClients();
    }
    return 0;
}