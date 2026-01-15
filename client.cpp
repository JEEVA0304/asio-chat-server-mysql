#include <asio.hpp>
#include <iostream>
#include <thread>
using asio::ip::tcp;

void reader(tcp::socket& socket){
    try {
        asio::streambuf buffer;
        while(true){
            asio::read_until(socket, buffer, '\n');
            std::istream is(&buffer);
            std::string msg; std::getline(is,msg);
            std::cout<<msg<<"\n";
        }
    } catch(...){ std::cout<<"Disconnected\n"; }
}

int main(){
    try{
        asio::io_context io_context;
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1","12345");
        tcp::socket socket(io_context);
        asio::connect(socket,endpoints);

        std::thread t(reader,std::ref(socket));
        std::string msg;
        while(std::getline(std::cin,msg)){
            asio::write(socket,asio::buffer(msg+"\n"));
        }
        t.join();
    }catch(std::exception& e){ std::cerr<<e.what()<<"\n"; }
}
